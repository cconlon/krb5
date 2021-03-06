.. _kdb5_ldap_util(8):

kdb5_ldap_util
===============

SYNOPSIS
--------

.. _kdb5_ldap_util_synopsis:

**kdb5_ldap_util**
[**-D** *user_dn* [**-w** *passwd*]]
[**-H** *ldapuri*]
**command**
[*command_options*]

.. _kdb5_ldap_util_synopsis_end:


DESCRIPTION
-----------

kdb5_ldap_util allows an administrator to manage realms, Kerberos
services and ticket policies.


COMMAND-LINE OPTIONS
--------------------

.. _kdb5_ldap_util_options:

**-D** *user_dn*
    Specifies the Distinguished Name (DN) of the user who has
    sufficient rights to perform the operation on the LDAP server.

**-w** *passwd*
    Specifies the password of *user_dn*.  This option is not
    recommended.

**-H** *ldapuri*
    Specifies the URI of the LDAP server.  It is recommended to use
    ``ldapi://`` or ``ldaps://`` to connect to the LDAP server.

.. _kdb5_ldap_util_options_end:


COMMANDS
--------

create
~~~~~~

.. _kdb5_ldap_util_create:

    **create**
    [**-subtrees** *subtree_dn_list*]
    [**-sscope** *search_scope*]
    [**-containerref** *container_reference_dn*]
    [**-k** *mkeytype*]
    [**-kv** *mkeyVNO*]
    [**-m|-P** *password*\|\ **-sf** *stashfilename*]
    [**-s**]
    [**-r** *realm*]
    [**-maxtktlife** *max_ticket_life*]
    [**-maxrenewlife** *max_renewable_ticket_life*]
    [*ticket_flags*]

Creates realm in directory. Options:

**-subtrees** *subtree_dn_list*
    Specifies the list of subtrees containing the principals of a
    realm.  The list contains the DNs of the subtree objects separated
    by colon (``:``).

**-sscope** *search_scope*
    Specifies the scope for searching the principals under the
    subtree.  The possible values are 1 or one (one level), 2 or sub
    (subtrees).

**-containerref** *container_reference_dn*
    Specifies the DN of the container object in which the principals
    of a realm will be created.  If the container reference is not
    configured for a realm, the principals will be created in the
    realm container.

**-k** *mkeytype*
    Specifies the key type of the master key in the database.  The
    default is given by the **master_key_type** variable in
    :ref:`kdc.conf(5)`.

**-kv** *mkeyVNO*
    Specifies the version number of the master key in the database;
    the default is 1.  Note that 0 is not allowed.

**-m**
    Specifies that the master database password should be read from
    the TTY rather than fetched from a file on the disk.

**-P** *password*
    Specifies the master database password. This option is not
    recommended.

**-r** *realm*
    Specifies the Kerberos realm of the database.

**-sf** *stashfilename*
    Specifies the stash file of the master database password.

**-s**
    Specifies that the stash file is to be created.

**-maxtktlife** *max_ticket_life*
    (:ref:`getdate` string) Specifies maximum ticket life for
    principals in this realm.

**-maxrenewlife** *max_renewable_ticket_life*
    (:ref:`getdate` string) Specifies maximum renewable life of
    tickets for principals in this realm.

*ticket_flags*
    Specifies global ticket flags for the realm.  Allowable flags are
    documented in the description of the **add_principal** command in
    :ref:`kadmin(1)`.

Example:

 ::

    kdb5_ldap_util -D cn=admin,o=org -H ldaps://ldap-server1.mit.edu
        create -subtrees o=org -sscope SUB -r ATHENA.MIT.EDU
    Password for "cn=admin,o=org":
    Initializing database for realm 'ATHENA.MIT.EDU'
    You will be prompted for the database Master Password.
    It is important that you NOT FORGET this password.
    Enter KDC database master key:
    Re-enter KDC database master key to verify:

.. _kdb5_ldap_util_create_end:

modify
~~~~~~

.. _kdb5_ldap_util_modify:

    **modify**
    [**-subtrees** *subtree_dn_list*]
    [**-sscope** *search_scope*]
    [**-containerref** *container_reference_dn*]
    [**-r** *realm*]
    [**-maxtktlife** *max_ticket_life*]
    [**-maxrenewlife** *max_renewable_ticket_life*]
    [*ticket_flags*]

Modifies the attributes of a realm.  Options:

**-subtrees** *subtree_dn_list*
    Specifies the list of subtrees containing the principals of a
    realm.  The list contains the DNs of the subtree objects separated
    by colon (``:``).  This list replaces the existing list.

**-sscope** *search_scope*
    Specifies the scope for searching the principals under the
    subtrees.  The possible values are 1 or one (one level), 2 or sub
    (subtrees).

**-containerref** *container_reference_dn* Specifies the DN of the
    container object in which the principals of a realm will be
    created.

**-r** *realm*
    Specifies the Kerberos realm of the database.

**-maxtktlife** *max_ticket_life*
    (:ref:`getdate` string) Specifies maximum ticket life for
    principals in this realm.

**-maxrenewlife** *max_renewable_ticket_life*
    (:ref:`getdate` string) Specifies maximum renewable life of
    tickets for principals in this realm.

*ticket_flags*
    Specifies global ticket flags for the realm.  Allowable flags are
    documented in the description of the **add_principal** command in
    :ref:`kadmin(1)`.

Example:

 ::

    shell% kdb5_ldap_util -D cn=admin,o=org -H
        ldaps://ldap-server1.mit.edu modify +requires_preauth -r
        ATHENA.MIT.EDU
    Password for "cn=admin,o=org":
    shell%

.. _kdb5_ldap_util_modify_end:

view
~~~~

.. _kdb5_ldap_util_view:

    **view** [**-r** *realm*]

Displays the attributes of a realm.  Options:

**-r** *realm*
    Specifies the Kerberos realm of the database.

Example:

 ::

    kdb5_ldap_util -D cn=admin,o=org -H ldaps://ldap-server1.mit.edu
        view -r ATHENA.MIT.EDU
    Password for "cn=admin,o=org":
    Realm Name: ATHENA.MIT.EDU
    Subtree: ou=users,o=org
    Subtree: ou=servers,o=org
    SearchScope: ONE
    Maximum ticket life: 0 days 01:00:00
    Maximum renewable life: 0 days 10:00:00
    Ticket flags: DISALLOW_FORWARDABLE REQUIRES_PWCHANGE

.. _kdb5_ldap_util_view_end:

destroy
~~~~~~~

.. _kdb5_ldap_util_destroy:

    **destroy** [**-f**] [**-r** *realm*]

Destroys an existing realm. Options:

**-f**
    If specified, will not prompt the user for confirmation.

**-r** *realm*
    Specifies the Kerberos realm of the database.

Example:

 ::

    shell% kdb5_ldap_util -D cn=admin,o=org -H
        ldaps://ldap-server1.mit.edu destroy -r ATHENA.MIT.EDU
    Password for "cn=admin,o=org":
    Deleting KDC database of 'ATHENA.MIT.EDU', are you sure?
    (type 'yes' to confirm)? yes
    OK, deleting database of 'ATHENA.MIT.EDU'...
    shell%

.. _kdb5_ldap_util_destroy_end:

list
~~~~

.. _kdb5_ldap_util_list:

    **list**

Lists the name of realms.

Example:

 ::

    shell% kdb5_ldap_util -D cn=admin,o=org -H
        ldaps://ldap-server1.mit.edu list
    Password for "cn=admin,o=org":
    ATHENA.MIT.EDU
    OPENLDAP.MIT.EDU
    MEDIA-LAB.MIT.EDU
    shell%

.. _kdb5_ldap_util_list_end:

stashsrvpw
~~~~~~~~~~

.. _kdb5_ldap_util_stashsrvpw:

    **stashsrvpw**
    [**-f** *filename*]
    *servicedn*

Allows an administrator to store the password for service object in a
file so that KDC and Administration server can use it to authenticate
to the LDAP server.  Options:

**-f** *filename*
    Specifies the complete path of the service password file. By
    default, ``/usr/local/var/service_passwd`` is used.

*servicedn*
    Specifies Distinguished Name (DN) of the service object whose
    password is to be stored in file.

Example:

 ::

    kdb5_ldap_util stashsrvpw -f /home/andrew/conf_keyfile
        cn=service-kdc,o=org
    Password for "cn=service-kdc,o=org":
    Re-enter password for "cn=service-kdc,o=org":

.. _kdb5_ldap_util_stashsrvpw_end:

create_policy
~~~~~~~~~~~~~

.. _kdb5_ldap_util_create_policy:

    **create_policy**
    [**-r** *realm*]
    [**-maxtktlife** *max_ticket_life*]
    [**-maxrenewlife** *max_renewable_ticket_life*]
    [*ticket_flags*]
    *policy_name*

Creates a ticket policy in the directory.  Options:

**-r** *realm*
    Specifies the Kerberos realm of the database.

**-maxtktlife** *max_ticket_life*
    (:ref:`getdate` string) Specifies maximum ticket life for
    principals.

**-maxrenewlife** *max_renewable_ticket_life*
    (:ref:`getdate` string) Specifies maximum renewable life of
    tickets for principals.

*ticket_flags*
    Specifies the ticket flags.  If this option is not specified, by
    default, no restriction will be set by the policy.  Allowable
    flags are documented in the description of the **add_principal**
    command in :ref:`kadmin(1)`.

*policy_name*
    Specifies the name of the ticket policy.

Example:

 ::

    kdb5_ldap_util -D cn=admin,o=org -H ldaps://ldap-server1.mit.edu
        create_policy -r ATHENA.MIT.EDU -maxtktlife "1 day"
        -maxrenewlife "1 week" -allow_postdated +needchange
        -allow_forwardable tktpolicy
    Password for "cn=admin,o=org":

.. _kdb5_ldap_util_create_policy_end:

modify_policy
~~~~~~~~~~~~~

.. _kdb5_ldap_util_modify_policy:

    **modify_policy**
    [**-r** *realm*]
    [**-maxtktlife** *max_ticket_life*]
    [**-maxrenewlife** *max_renewable_ticket_life*]
    [*ticket_flags*]
    *policy_name*

Modifies the attributes of a ticket policy.  Options are same as for
**create_policy**.

Example:

 ::

    kdb5_ldap_util -D cn=admin,o=org -H
        ldaps://ldap-server1.mit.edu modify_policy -r ATHENA.MIT.EDU
        -maxtktlife "60 minutes" -maxrenewlife "10 hours"
        +allow_postdated -requires_preauth tktpolicy
    Password for "cn=admin,o=org":

.. _kdb5_ldap_util_modify_policy_end:

view_policy
~~~~~~~~~~~

.. _kdb5_ldap_util_view_policy:

    **view_policy**
    [**-r** *realm*]
    *policy_name*

Displays the attributes of a ticket policy.  Options:

*policy_name*
    Specifies the name of the ticket policy.

Example:

 ::

    kdb5_ldap_util -D cn=admin,o=org -H ldaps://ldap-server1.mit.edu
        view_policy -r ATHENA.MIT.EDU tktpolicy
    Password for "cn=admin,o=org":
    Ticket policy: tktpolicy
    Maximum ticket life: 0 days 01:00:00
    Maximum renewable life: 0 days 10:00:00
    Ticket flags: DISALLOW_FORWARDABLE REQUIRES_PWCHANGE

.. _kdb5_ldap_util_view_policy_end:

destroy_policy
~~~~~~~~~~~~~~

.. _kdb5_ldap_util_destroy_policy:

    **destroy_policy**
    [**-r** *realm*]
    [**-force**]
    *policy_name*

Destroys an existing ticket policy.  Options:

**-r** *realm*
    Specifies the Kerberos realm of the database.

**-force**
    Forces the deletion of the policy object.  If not specified, the
    user will be prompted for confirmation before deleting the policy.

*policy_name*
    Specifies the name of the ticket policy.

Example:

 ::

    kdb5_ldap_util -D cn=admin,o=org -H ldaps://ldap-server1.mit.edu
        destroy_policy -r ATHENA.MIT.EDU tktpolicy
    Password for "cn=admin,o=org":
    This will delete the policy object 'tktpolicy', are you sure?
    (type 'yes' to confirm)? yes
    ** policy object 'tktpolicy' deleted.

.. _kdb5_ldap_util_destroy_policy_end:

list_policy
~~~~~~~~~~~

.. _kdb5_ldap_util_list_policy:

    **list_policy**
    [**-r** *realm*]

Lists the ticket policies in realm if specified or in the default
realm.  Options:

**-r** *realm*
    Specifies the Kerberos realm of the database.

Example:

 ::

    kdb5_ldap_util -D cn=admin,o=org -H ldaps://ldap-server1.mit.edu
        list_policy -r ATHENA.MIT.EDU
    Password for "cn=admin,o=org":
    tktpolicy
    tmppolicy
    userpolicy

.. _kdb5_ldap_util_list_policy_end:


SEE ALSO
--------

:ref:`kadmin(1)`
