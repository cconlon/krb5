/*
 * plugins/authdata/saml_server/saml_kdc.cpp
 *
 * Copyright 2009 by the Massachusetts Institute of Technology.
 *
 * Export of this software from the United States of America may
 *   require a specific license from the United States Government.
 *   It is the responsibility of any person or organization contemplating
 *   export to obtain such a license before exporting.
 *
 * WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
 * distribute this software and its documentation for any purpose and
 * without fee is hereby granted, provided that the above copyright
 * notice appear in all copies and that both that copyright notice and
 * this permission notice appear in supporting documentation, and that
 * the name of M.I.T. not be used in advertising or publicity pertaining
 * to distribution of the software without specific, written prior
 * permission.  Furthermore if you modify this software you must label
 * your software as modified software and not distribute it in such a
 * fashion that it might be confused with the original M.I.T. software.
 * M.I.T. makes no representations about the suitability of
 * this software for any purpose.  It is provided "as is" without express
 * or implied warranty.
 *
 *
 * SAML KDC authorization data plugin
 */

#include <string.h>
#include <errno.h>

#include "saml_kdc.h"

/*
 * Some notes on the binding between SAML assertions and Kerberos principals.
 *
 * A KDC-issued assertion will contain the Kerberos principal as the subject
 * name.
 *
 * A KDC-vouched assertion will contain the principal in the subject
 * confirmation. This will match the principal name in the ticket. This
 * principal may be the well-known SAML principal name.
 *
 * KRB5SignedData protects these assertions with the TGS session key
 * within the service's realm realm.
 *
 * Now to consider some interesting cases:
 *
 * (a) In the cross-realm S4U2Self case, the principal name in returned
 *     ticket is not changed until the server's realm. To protect misuse
 *     of referral tickets, the service must check the name binding. This
 *     is no different to PAC-based S4U2Self.
 *
 * (b) An IdP acquires a ticket with non-SAML S4U2Self and ends up with a
 *     ticket with an assertion. It then issues its own assertion and
 *     includes the ticket in the assertion. Then it does S4U2Proxy with
 *     its own assertion in the TGS-REQ. In this case we ignore the first
 *     assertion: ideally, the IdP has KRB5_KDB_NO_AUTH_DATA_REQUIRED set
 *     on its service principal.
 *
 * (c) A service does S4U2Self with a KDC-issued assertion. This is more
 *     akin to doing S4U2Proxy with a ticket: somehow we need to verify
 *     that we issued the assertion (we can't use the session key because
 *     the ticket containing the session key is not available). For now,
 *     we're not going to support this, although we could with a TGS
 *     signature. (Actually, the right way to do this is to have an
 *     assertion signed with the IdP's shared secret.)
 */

krb5_error_code
saml_init(krb5_context ctx, void **data)
{
    SAMLConfig &config = SAMLConfig::getConfig();

    XMLToolingConfig& xmlconf = XMLToolingConfig::getConfig();

    if (getenv("SAML_DEBUG"))
        xmlconf.log_config("DEBUG");
    else
        xmlconf.log_config();

    if (!config.init()) {
        return KRB5KDC_ERR_SVC_UNAVAILABLE;
    }

    *data = &config;

    return 0;
}

void
saml_fini(krb5_context ctx, void *data)
{
    SAMLConfig *config = (SAMLConfig *)data;

    config->term();
}

static krb5_error_code
saml_kdc_build_issuer(krb5_context context,
                      krb5_const_principal principal,
                      Issuer **pIssuer)
{
    Issuer *issuer;
    XMLCh *unicodePrincipal = NULL;
    krb5_error_code code;

    code = saml_krb_unparse_name_xmlch(context, principal, &unicodePrincipal);
    if (code != 0)
        goto cleanup;

    issuer = IssuerBuilder::buildIssuer();
    issuer->setFormat(NameIDType::KERBEROS);
    issuer->setName(unicodePrincipal);

    *pIssuer = issuer;

cleanup:
    delete unicodePrincipal;

    return code;
}

static krb5_error_code
saml_kdc_build_subject(krb5_context context,
                       krb5_const_principal principal,
                       Subject **pSubject)
{
    NameID *nameID;
    Subject *subject;
    krb5_error_code code;

    code = saml_krb_build_nameid(context, principal, &nameID);
    if (code != 0)
        return code;

    subject = SubjectBuilder::buildSubject();
    subject->setNameID(nameID);

    *pSubject = subject;

    return 0;
}

static krb5_error_code
saml_kdc_annotate_assertion(krb5_context context,
                            unsigned int flags,
                            krb5_const_principal client_princ,
                            krb5_db_entry *client,
                            krb5_db_entry *server,
                            krb5_db_entry *tgs,
                            krb5_enc_tkt_part *enc_tkt_reply,
                            saml2::Assertion *assertion)
{
    AuthnStatement *statement;
    AuthnContext *authnContext;
    AuthnContextClassRef *authnContextClass;
    Conditions *conditions;
    DateTime authtime((time_t)enc_tkt_reply->times.authtime);
    DateTime starttime((time_t)enc_tkt_reply->times.starttime);
    DateTime endtime((time_t)enc_tkt_reply->times.endtime);
    auto_ptr_XMLCh method("urn:oasis:names:tc:SAML:2.0:ac:classes:Kerberos");

    authnContext = AuthnContextBuilder::buildAuthnContext();
    authnContextClass = AuthnContextClassRefBuilder::buildAuthnContextClassRef();
    authnContextClass->setReference(method.get());
    authnContext->setAuthnContextClassRef(authnContextClass);

    statement = AuthnStatementBuilder::buildAuthnStatement();
    statement->setAuthnInstant(authtime.getFormattedString());
    statement->setAuthnContext(authnContext);

    conditions = ConditionsBuilder::buildConditions();
    conditions->setNotBefore(starttime.getFormattedString());
    conditions->setNotOnOrAfter(endtime.getFormattedString());

    assertion->setConditions(conditions);
    assertion->getAuthnStatements().push_back(statement);

    return 0;
}

static krb5_error_code
saml_kdc_build_subject_confirmation(krb5_context context,
                                    unsigned int flags,
                                    krb5_const_principal client_princ,
                                    krb5_db_entry *client,
                                    krb5_db_entry *server,
                                    krb5_enc_tkt_part *enc_tkt_reply,
                                    saml2::SubjectConfirmation **pSubjectConf)
{
    saml2::SubjectConfirmation *subjectConf;
    saml2::KeyInfoConfirmationDataType *keyConfData;
    DateTime authtime((time_t)enc_tkt_reply->times.authtime);
    DateTime starttime((time_t)enc_tkt_reply->times.starttime);
    DateTime endtime((time_t)enc_tkt_reply->times.endtime);
    NameID *nameID = NULL;
    krb5_error_code code;
    KeyInfo *keyInfo;

    *pSubjectConf = NULL;

    code = saml_krb_build_principal_keyinfo(context, client_princ, &keyInfo);
    if (code != 0)
        return code;

    if (flags & KRB5_KDB_FLAGS_S4U) {
        code = saml_krb_build_nameid(context, server->princ, &nameID);
        if (code != 0) {
            delete keyInfo;
            return code;
        }
    }

    keyConfData = KeyInfoConfirmationDataTypeBuilder::buildKeyInfoConfirmationDataType();
    keyConfData->setNotBefore(authtime.getFormattedString());
    keyConfData->setNotOnOrAfter(endtime.getFormattedString());
    keyConfData->getKeyInfos().push_back(keyInfo);

    subjectConf = SubjectConfirmationBuilder::buildSubjectConfirmation();
    if (nameID != NULL)
        subjectConf->setNameID(nameID);
    subjectConf->setMethod(SubjectConfirmation::HOLDER_KEY);
    subjectConf->setSubjectConfirmationData(keyConfData);

    *pSubjectConf = subjectConf;

    return 0;
}

static krb5_error_code
saml_kdc_build_assertion(krb5_context context,
                         unsigned int flags,
                         krb5_const_principal client_princ,
                         krb5_db_entry *client,
                         krb5_db_entry *server,
                         krb5_db_entry *tgs,
                         krb5_enc_tkt_part *enc_tkt_reply,
                         saml2::Assertion **pAssertion)
{
    krb5_error_code code;
    Issuer *issuer;
    Subject *subject;
    AttributeStatement *attrStatement;
    saml2::Assertion *assertion;
    DateTime authtime((time_t)enc_tkt_reply->times.authtime);

    try {
        assertion = AssertionBuilder::buildAssertion();
        assertion->addNamespace(Namespace(XSD_NS, XSD_PREFIX));
        assertion->addNamespace(Namespace(XSI_NS, XSI_PREFIX));
        assertion->addNamespace(Namespace(XMLSIG_NS, XMLSIG_PREFIX));
        assertion->addNamespace(Namespace(SAML20_NS, SAML20_PREFIX));

        assertion->setIssueInstant(authtime.getFormattedString());

        saml_kdc_build_issuer(context, tgs->princ, &issuer);
        assertion->setIssuer(issuer);

        saml_kdc_build_subject(context, client_princ, &subject);
        assertion->setSubject(subject);

        saml_kdc_build_attrs_ldap(context, client, server, &attrStatement);
        if (attrStatement != NULL) {
            assertion->addNamespace(Namespace(SAML20X500_NS, SAML20X500_PREFIX));
            assertion->getAttributeStatements().push_back(attrStatement);
        }

        saml_kdc_annotate_assertion(context, flags, client_princ,
                                    client, server, tgs,
                                    enc_tkt_reply,
                                    assertion);

        code = 0;
        *pAssertion = assertion;
    } catch (exception &e) {
        code = ASN1_PARSE_ERROR; /* XXX */
        delete assertion;
    }

    return code;
}

/*
 * Look for an assertion submitted by the client or in the TGT. In
 * the latter case, we considered it verified because it was encrypted
 * in our secret key.
 */
static krb5_error_code
saml_kdc_get_assertion(krb5_context context,
                       unsigned int flags,
                       krb5_kdc_req *request,
                       krb5_enc_tkt_part *enc_tkt_request,
                       saml2::Assertion **pAssertion,
                       krb5_boolean *verified)
{
    krb5_error_code code;
    krb5_authdata **authdata = NULL;
    krb5_boolean fromEncPart = FALSE;

    *pAssertion = NULL;

    code = krb5int_find_authdata(context,
                                 request->unenc_authdata,
                                 NULL,
                                 KRB5_AUTHDATA_SAML,
                                 &authdata);
    if (code != 0)
        return code;

    /*
     * In the S4U case, we can't use any assertion present in the TGT
     * because it would refer to the service itself, not the client.
     */
    if (authdata == NULL &&
        (flags & KRB5_KDB_FLAGS_S4U) == 0) {
        code = krb5int_find_authdata(context,
                                     enc_tkt_request->authorization_data,
                                     NULL,
                                     KRB5_AUTHDATA_SAML,
                                     &authdata);
        if (code != 0)
            return code;

        fromEncPart = TRUE;
    }

    if (authdata == NULL ||
        authdata[0]->ad_type != KRB5_AUTHDATA_SAML ||
        authdata[1] != NULL)
        return 0;

    code = saml_krb_decode_assertion(context, authdata[0], pAssertion);

    *verified = fromEncPart;

    return code;
}

static krb5_error_code
saml_kdc_confirm_subject(krb5_context context,
                         unsigned int flags,
                         krb5_const_principal client_princ,
                         krb5_db_entry *client,
                         krb5_db_entry *server,
                         krb5_db_entry *tgs,
                         krb5_enc_tkt_part *enc_tkt_reply,
                         saml2::Assertion *assertion)
{
    SubjectConfirmation *subjectConfirmation;
    krb5_error_code code;
    krb5_boolean isSamlPrincipal;

    isSamlPrincipal = saml_krb_is_saml_principal(context, client_princ);

    /*
     * We confirm the subject - that is, add the principal name to the
     * list of subject confirmations - where we have mapped the assertion
     * to an explicit principal in our realm. In the case of cross-realm
     * S4U2Self, only the realm that has the originating mapping confirms
     * the subject. In the case of no explicit mapping (the well known
     * SAML principal is used) then there is no subject confirmation.
     */
    if (isSamlPrincipal ||
        !krb5_realm_compare(context, client_princ, tgs->princ))
        return 0;

    code = saml_kdc_build_subject_confirmation(context, flags,
                                               client_princ, client, server,
                                               enc_tkt_reply,
                                               &subjectConfirmation);
    if (code != 0)
        return code;

    assertion->getSubject()->getSubjectConfirmations().push_back(subjectConfirmation);

    return 0;
}

/*
 * Verify a client-submitted assertion is bound to the client principal:
 *
 * (a) TGS-REQ: assertion must be bound to the header ticket client
 * (b) S4U2Self: assertion must be bound to the S4U2Self client
 * (c) S4U2Proxy: assertion must be bound to evidence ticket client
 *
 * Regardless, client_princ is set to the appropriate value in all cases.
 */
static krb5_error_code
saml_kdc_verify_assertion(krb5_context context,
                          unsigned int flags,
                          krb5_const_principal client_princ,
                          krb5_db_entry *client,
                          krb5_db_entry *server,
                          krb5_keyblock *server_key,
                          krb5_db_entry *tgs,
                          krb5_keyblock *tgs_key,
                          krb5_kdc_req *request,
                          krb5_enc_tkt_part *enc_tkt_request,
                          saml2::Assertion *assertion,
                          krb5_boolean fromTGT,
                          krb5_boolean *verified)
{
    krb5_error_code code;
    unsigned usage = SAML_KRB_VERIFY_TRUSTENGINE;

    /*
     * Verify using PKI or potentially the server key if the server
     * is a trusted entity.
     */
    if (fromTGT)
        usage |= SAML_KRB_VERIFY_KDC_VOUCHED;

    code = saml_krb_verify(context,
                           assertion,
                           NULL,
                           client_princ,
                           client,
                           request->server,
                           0,
                           usage,
                           verified);
    if (code == 0) {
    }

    return code;
}

static krb5_error_code
saml_kdc_build_signature(krb5_context context,
                         krb5_const_principal keyOwner,
                         krb5_keyblock *key,
                         unsigned int usage,
                         Signature **pSignature)
{
    krb5_error_code code;
#if 0
    KeyInfo *keyInfo;
#endif
    XSECCryptoKey *xmlKey;
    Signature *signature;
    auto_ptr_XMLCh algorithm(URI_ID_HMAC_SHA512);

    *pSignature = NULL;

#if 0
    code = saml_krb_build_principal_keyinfo(context, keyOwner, &keyInfo);
    if (code != 0)
        return code;
#endif

    code = saml_krb_derive_key(context, key, usage, &xmlKey);
    if (code != 0) {
        return code;
    }

    signature = SignatureBuilder::buildSignature();
    signature->setSignatureAlgorithm(algorithm.get());
    signature->setSigningKey(xmlKey);
#if 0
    signature->setKeyInfo(keyInfo);
#endif

    *pSignature = signature;

    return 0;
}

static krb5_error_code
saml_kdc_encode(krb5_context context,
                unsigned int flags,
                krb5_const_principal client_princ,
                krb5_keyblock *server_key,
                krb5_keyblock *tgs_key,
                krb5_enc_tkt_part *enc_tkt_reply,
                krb5_boolean sign,
                saml2::Assertion *assertion)
{
    krb5_error_code code;
    krb5_authdata ad_datum, *ad_data[2], **kdc_issued = NULL;
    krb5_authdata **if_relevant = NULL;
    krb5_authdata **tkt_authdata;
    Signature *signature;

    string buf;

    try {
        if (sign) {
            code = saml_kdc_build_signature(context, client_princ,
                                            enc_tkt_reply->session,
                                            SAML_KRB_USAGE_SESSION_KEY,
                                            &signature);
            if (code != 0)
                return code;

            assertion->setSignature(signature);

            vector <Signature *> signatures(1, signature);
            XMLHelper::serialize(assertion->marshall((DOMDocument *)NULL, &signatures, NULL), buf);
        } else {
            XMLHelper::serialize(assertion->marshall((DOMDocument *)NULL), buf);
        }
    } catch (exception &e) {
        code = ASN1_PARSE_ERROR; /* XXX */
    }

    if (getenv("SAML_DEBUG_SERIALIZE"))
        fprintf(stderr, "%s\n", buf.c_str());

    ad_datum.ad_type = KRB5_AUTHDATA_SAML;
    ad_datum.contents = (krb5_octet *)buf.c_str();
    ad_datum.length = buf.length();

    ad_data[0] = &ad_datum;
    ad_data[1] = NULL;

    code = krb5_encode_authdata_container(context,
                                          KRB5_AUTHDATA_IF_RELEVANT,
                                          ad_data,
                                          &if_relevant);
    if (code != 0) {
        krb5_free_authdata(context, kdc_issued);
        return code;
    }

    code = krb5_merge_authdata(context,
                               if_relevant,
                               enc_tkt_reply->authorization_data,
                               &tkt_authdata);
    if (code == 0) {
        krb5_free_authdata(context, enc_tkt_reply->authorization_data);
        enc_tkt_reply->authorization_data = tkt_authdata;
    } else {
        krb5_free_authdata(context, if_relevant);
    }

    krb5_free_authdata(context, kdc_issued);

    return code;
}

krb5_error_code
saml_authdata(krb5_context context,
              unsigned int flags,
              krb5_db_entry *client,
              krb5_db_entry *server,
              krb5_db_entry *tgs,
              krb5_keyblock *client_key,
              krb5_keyblock *server_key,
              krb5_keyblock *tgs_key,
              krb5_data *req_pkt,
              krb5_kdc_req *request,
              krb5_const_principal for_user_princ,
              krb5_enc_tkt_part *enc_tkt_request,
              krb5_enc_tkt_part *enc_tkt_reply)
{
    krb5_error_code code;
    krb5_const_principal client_princ;
    saml2::Assertion *assertion = NULL;
    krb5_boolean vouch = FALSE;
    krb5_boolean fromTGT = FALSE;

    if (request->msg_type != KRB5_TGS_REQ ||
        (server->attributes & KRB5_KDB_NO_AUTH_DATA_REQUIRED))
        return 0;

    if (flags & KRB5_KDB_FLAG_PROTOCOL_TRANSITION)
        client_princ = for_user_princ;
    else
        client_princ = enc_tkt_reply->client;

    code = saml_kdc_get_assertion(context, flags,
                                  request, enc_tkt_request,
                                  &assertion, &fromTGT);
    if (code != 0)
        goto cleanup;

    if (assertion != NULL) {
        code = saml_kdc_verify_assertion(context, flags,
                                         client_princ, client,
                                         server, server_key,
                                         tgs, tgs_key,
                                         request, enc_tkt_request,
                                         assertion, fromTGT, &vouch);
        if (code != 0)
            goto cleanup;
    } else if (client != NULL) {
        code = saml_kdc_build_assertion(context, flags,
                                        client_princ, client,
                                        server, tgs,
                                        enc_tkt_reply,
                                        &assertion);
        if (code != 0)
            goto cleanup;

        vouch = TRUE; /* we built it, we'll vouch for it */
    }

    if (assertion != NULL) {
        if (vouch == TRUE) {
            code = saml_kdc_confirm_subject(context, flags,
                                            client_princ, client,
                                            server, tgs,
                                            enc_tkt_reply,
                                            assertion);
            if (code != 0)
                goto cleanup;
        }
        code = saml_kdc_encode(context, flags, client_princ,
                               server_key, tgs_key, enc_tkt_reply,
                               vouch, assertion);
        if (code != 0)
            goto cleanup;
    }

cleanup:
    delete assertion;

    return code;
}

krb5plugin_authdata_server_ftable_v2 authdata_server_2 = {
    "saml",
    saml_init,
    saml_fini,
    saml_authdata,
};