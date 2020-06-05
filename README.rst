ngx_http_stat_check module
==========================

.. contents::
  :backlinks: none


Description
-----------

Simple nginx_ module that returns HTTP "403 Forbidden" error if `stat()`_
of specified path succeeds.

Intended use is trivial djbdns-style filesystem-db blacklisting by any
`nginx variable`_ set on request, for example $remote_addr (think fail2ban).

.. _nginx: https://www.nginx.com/
.. _stat(): https://en.wikipedia.org/wiki/Stat_(system_call)
.. _nginx variable: https://nginx.org/en/docs/varindex.html


Build / install
---------------

Module requires source code for same-ish nginx version as it will be loaded by::

  % ver=1.16.1 # make sure it matches installed nginx version
  % curl -L https://nginx.org/download/nginx-$ver.tar.gz | tar -xz

  % cd nginx-$ver
  % test -e Makefile || ./configure --with-compat --add-dynamic-module=..
  % make modules

  % cd objs
  % install -m755 -t /usr/lib/nginx/modules/ ngx_http_stat_check.so

Re-run two last bits to rebuild after any code or nginx updates.


Usage
-----

Module adds single "stat_check" directive, which takes path template
as its one argument, and is only allowed in "location" context.

::

  load_module /usr/lib/nginx/modules/ngx_http_stat_check.so;
  ...
  location /my-files {
    alias /srv/www/my-files;
    autoindex on;
    stat_check /tmp/blacklist/$remote_addr;
  }

This will have module run single stat() call for every request, checking if
"/tmp/blacklist/$remote_addr" path exists, returning "403 Forbidden" if that's
the case.

For example, if request comes from 1.2.3.4 IP address (be sure to `check docs on
remote_addr first!`_), path /tmp/blacklist/1.2.3.4 will be checked, and if
exists, any request to /my-files location will be denied for this client.

Any other nginx variables can be used in the same way, including regexp-matched
path fragments, mapped or custom-set values.

.. _check docs on remote_addr first!: https://nginx.org/en/docs/http/ngx_http_core_module.html#var_remote_addr


Why it exists
-------------

Intended use is blocking access dynamically to various unwanted http spam and
bots on relatively idle servers (see also `nginx-access-log-stat-block script`_).

Regular nginx configuration does not allow that, as it lacks any kind of
external/dynamic configuration.

It is however available in a `premium Nginx Plus version via "keyval" module`_
via JSON API, and maybe will be merged into open version someday, so be sure to
check for that first.

While I don't think that extra per-request stat() might make a difference
performance-wise, this module is not intended or tested for/in any kind of
high-load environments, and might not be suitable against something like a
DDoS attack - use CloudFlare, firewalls, Nginx Plus functionality or whatever
dedicated solutions for that.

See `"Dynamic blacklisting configuration" blog post here`_ for even more info.

.. _nginx-access-log-stat-block script: https://github.com/mk-fg/fgtk#nginx-access-log-stat-block
.. _premium Nginx Plus version via "keyval" module: https://docs.nginx.com/nginx/admin-guide/security-controls/blacklisting-ip-addresses/
.. _"Dynamic blacklisting configuration" blog post here: https://blog.fraggod.net/2020/01/03/dynamic-blacklisting-configuration-for-nginx-access-via-custom-module.html


Room for improvement
--------------------

Items here look like an obvious improvements, mostly for flexibility and
unlocking new vast use-cases, but doubt I'll get to these unless will need it myself.

- Make HTTP error page configurable, e.g. like =403 in try_files directive.

- Separate more complex directive that respects "root" and "alias" paths.

- Option to invert exists/missing logic for returning errors.

- Allow to specify $uri or @named-location to jump to on this condition.

- Allow/use hashed paths to avoid storing plaintext request info and to allow
  easy nesting of these on filesystem (instead of having all nodes in one dir).


Links
-----

- https://github.com/mk-fg/fgtk#nginx-access-log-stat-block

  Script to use blocking by $remote_addr with access_log from some honeypot-trap
  URL to block misbehaving bots that violate robots.txt and similar restrictions.

- https://github.com/agile6v/awesome-nginx#third-party-modules
- https://github.com/Vasfed/nginx_ipset_blacklist/
- https://github.com/rryqszq4/ngx_sqlite/
