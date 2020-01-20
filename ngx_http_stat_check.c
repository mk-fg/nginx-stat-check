//
// Simple nginx module that returns 403 if stat() of specified path succeeds.
//
// Build and install it like this:
//  % ver=1.16.1 # make sure it matches installed nginx version
//  % curl -L https://nginx.org/download/nginx-$ver.tar.gz | tar -cz
//  % cd nginx-$ver
//  % test -e Makefile || ./configure --with-compat --add-dynamic-module=..
//  % make modules
//  % cd objs
//  % install -m755 -t /usr/lib/nginx/modules/ ngx_http_stat_check.so
//
// Usage:
//   load_module /usr/lib/nginx/modules/ngx_http_stat_check.so;
//   ...
//     location /test { stat_check /var/cache/blacklist/$remote_addr; }
//
// Note: path in stat_check DOES NOT use "root", "alias"
//   and such paths in any way - just a templated path without any tricks.
// (see e.g. ngx_http_try_files_module.c for code to use these)
//
// Docs:
//   http://www.grid.net.ru/nginx/nginx-modules.html
//   http://nginx.org/en/docs/dev/development_guide.html
//   https://www.evanmiller.org/nginx-modules-guide.html
//

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


static char *
ngx_http_stat_check_parse(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);

static void * ngx_http_stat_check_loc_conf_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_stat_check_init(ngx_conf_t *cf);
static ngx_int_t ngx_http_stat_check_handler(ngx_http_request_t *r);


typedef struct {
	// Holds compiled script from path spec
	ngx_str_t path;
	ngx_str_t *path_tpl;
	ngx_array_t *lengths;
	ngx_array_t *values;
} ngx_http_stat_check_loc_conf_t;

static ngx_command_t ngx_http_stat_check_commands[] = {
	{ ngx_string("stat_check"),
		NGX_HTTP_LOC_CONF | NGX_CONF_TAKE1,
		ngx_http_stat_check_parse,
		NGX_HTTP_LOC_CONF_OFFSET,
		0,
		NULL },
	ngx_null_command
};

static ngx_http_module_t ngx_http_stat_check_ctx = {
	NULL, // preconfiguration
	ngx_http_stat_check_init, // postconfiguration
	NULL, // create main configuration
	NULL, // init main configuration
	NULL, // create server configuration
	NULL, // merge server configuration
	ngx_http_stat_check_loc_conf_init, // create location configuration
	NULL // merge location configuration
};

ngx_module_t ngx_http_stat_check = {
	NGX_MODULE_V1,
	&ngx_http_stat_check_ctx,
	ngx_http_stat_check_commands,
	NGX_HTTP_MODULE,
	NULL, // init master
	NULL, // init module
	NULL, // init process
	NULL, // init thread
	NULL, // exit thread
	NULL, // exit process
	NULL, // exit master
	NGX_MODULE_V1_PADDING
};


static void *
ngx_http_stat_check_loc_conf_init(ngx_conf_t *cf) {
	ngx_http_stat_check_loc_conf_t *loc_conf =
		ngx_pcalloc(cf->pool, sizeof(ngx_http_stat_check_loc_conf_t));
	if (loc_conf == NULL) return NULL;
	return loc_conf;
}


static ngx_int_t
ngx_http_stat_check_init(ngx_conf_t *cf) {
	// Installs handler on "access" phase of request processing
	ngx_http_core_main_conf_t *cmcf =
		ngx_http_conf_get_module_main_conf(cf, ngx_http_core_module);
	ngx_http_handler_pt *h =
		ngx_array_push(&cmcf->phases[NGX_HTTP_ACCESS_PHASE].handlers);
	if (h == NULL) return NGX_ERROR;
	*h = ngx_http_stat_check_handler;
	return NGX_OK;
}


static char *
ngx_http_stat_check_parse(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
	// Runs when directive is parsed, compiles path template (nginx script)
	if (cf->args->nelts < 2) return NGX_CONF_OK;

	ngx_http_stat_check_loc_conf_t *loc_conf = conf;
	ngx_str_t *args = cf->args->elts;
	loc_conf->path_tpl = args + 1;

	ngx_int_t n = ngx_http_script_variables_count(loc_conf->path_tpl);
	ngx_http_script_compile_t sc;
	ngx_array_t *lengths = NULL;
	ngx_array_t *values = NULL;
	ngx_memzero(&sc, sizeof(ngx_http_script_compile_t));
	sc.cf = cf;
	sc.source = loc_conf->path_tpl;
	sc.lengths = &lengths;
	sc.values = &values;
	sc.variables = n;
	sc.complete_lengths = 1;
	sc.complete_values = 1;
	if (ngx_http_script_compile(&sc) != NGX_OK) return NGX_CONF_ERROR;

	loc_conf->lengths = lengths;
	loc_conf->values = values;

	return NGX_CONF_OK;
}


static ngx_int_t
ngx_http_stat_check_handler(ngx_http_request_t *r) {
	// Runs on every request
	ngx_http_stat_check_loc_conf_t *loc_conf =
		ngx_http_get_module_loc_conf(r, ngx_http_stat_check);
	if (loc_conf->path_tpl == NULL) return NGX_OK;

	if (ngx_http_script_run( r, &loc_conf->path,
			loc_conf->lengths->elts, 0, loc_conf->values->elts ) == NULL)
		return NGX_ERROR;

	struct stat buf;
	int check = stat((char *) loc_conf->path.data, &buf);
	// int check = stat("test", &buf);
	return check == 0 ? NGX_HTTP_FORBIDDEN : NGX_OK;
}
