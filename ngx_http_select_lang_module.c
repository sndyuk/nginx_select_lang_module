#include <stdlib.h>

#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>

static ngx_str_t cookie_name_lang = ngx_string("lang");


static char *ngx_http_select_lang(ngx_conf_t *cf, ngx_command_t *cmd, void *conf);
static ngx_int_t ngx_http_select_lang_variable(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data);


static ngx_http_module_t ngx_http_select_lang_module_ctx = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
static ngx_command_t ngx_http_select_lang_commands[] = {
  {
    ngx_string("select_lang"),
    NGX_HTTP_MAIN_CONF | NGX_CONF_1MORE,
    ngx_http_select_lang,
    NGX_HTTP_MAIN_CONF_OFFSET,
    0,
    NULL
  },
  ngx_null_command
};

ngx_module_t ngx_http_select_lang_module = {
  NGX_MODULE_V1,
  &ngx_http_select_lang_module_ctx, // module context
  ngx_http_select_lang_commands, // module directives
  NGX_HTTP_MODULE, // module type
  NULL, // init master
  NULL, // init module
  NULL, // init process
  NULL, // init thread
  NULL, // exit thread
  NULL, // exit process
  NULL, // exit master
  NGX_MODULE_V1_PADDING
};


static char * ngx_http_select_lang(ngx_conf_t *cf, ngx_command_t *cmd, void *conf) {
  ngx_str_t *value = cf->args->elts;
  ngx_str_t name = value[1];

  if (name.data[0] != '$') {
    ngx_conf_log_error(NGX_LOG_WARN, cf, 0, "\"%V\" variable name should start with '$'", &name);
  } else {
    name.len--;
    name.data++;
  }

  ngx_http_variable_t *var = ngx_http_add_variable(cf, &name, NGX_HTTP_VAR_CHANGEABLE);
  if (var == NULL) {
    return NGX_CONF_ERROR;
  }
  if (var->get_handler != NULL) {
    ngx_conf_log_error(NGX_LOG_EMERG, cf, 0, "variable already defined: \"%V\"", &name);
    return NGX_CONF_ERROR;
  }

  var->get_handler = ngx_http_select_lang_variable;
  ngx_array_t *langs_group_array = ngx_array_create(cf->pool, cf->args->nelts - 1, sizeof(ngx_array_t));
  var->data = (uintptr_t)langs_group_array;

  ngx_uint_t i;
  for (i = 2; i < cf->args->nelts; i++) {
    ngx_array_t *elt_group = ngx_array_push(langs_group_array);
    ngx_uint_t count = 1;
    u_char *tmp = value[i].data;
    while (*tmp) {
      if (':' == *tmp) {
          count++;
      }
      tmp++;
    }
    ngx_array_t *langs_array = ngx_array_create(cf->pool, count, sizeof(ngx_str_t));

    char *pos = strtok((char *)value[i].data, ":");
    while (pos) {
      ngx_str_t *lang = ngx_array_push(langs_array);
      if (lang == NULL) {
        return NGX_CONF_ERROR;
      }
      lang->data = (u_char *)pos;
      lang->len = strlen(pos);
      pos = strtok(NULL, ":");
    }
    if (langs_array->nelts == 0) {
      return NGX_CONF_ERROR;
    }
    *elt_group = *langs_array;
  }

  return NGX_CONF_OK;
}


static ngx_str_t* find_lang(ngx_array_t *langs_group_array, u_char* start, ngx_uint_t length) {
  ngx_array_t *langs_group = (ngx_array_t *) langs_group_array->elts;
  ngx_uint_t i;
  for (i = 0; i < langs_group_array->nelts; i++) {
    ngx_array_t *langs_array = (ngx_array_t *)&langs_group[i];
    ngx_str_t *langs = (ngx_str_t *) langs_array->elts;
    ngx_uint_t j;
    for (j = 0; j < langs_array->nelts; j++) {
      if (length == langs[j].len
          && ngx_strncasecmp(start, langs[j].data, langs[j].len) == 0) {
        return &langs[0];
      }
    }
  }
  return NULL;
}

static ngx_str_t* find_lang_from_cookie(ngx_http_request_t *r, ngx_http_variable_value_t *v, ngx_array_t *langs_group_array) {
  if (r->headers_in.cookies.nelts == 0) {
    return NULL;
  }

  ngx_str_t s;
  if(ngx_http_parse_multi_header_lines(&(r->headers_in.cookies), &cookie_name_lang, &s) == NGX_DECLINED) {
   return NULL;
  }

  return find_lang(langs_group_array, s.data, s.len);
}

static ngx_str_t* find_lang_from_accept_lang(ngx_http_request_t *r, ngx_http_variable_value_t *v, ngx_array_t *langs_group_array) {
  if (r->headers_in.accept_language == NULL) {
    return NULL;
  }
  uint8_t *pos;
  uint8_t *start = r->headers_in.accept_language->value.data;
  uint8_t *end = start + r->headers_in.accept_language->value.len;
  while (start < end) {
    // eating spaces
    while (start < end && *start == ' ') {
      start++;
    }
    pos = start;
    while (pos < end && *pos != ',' && *pos != ';') {
      pos++;
    }

    ngx_str_t *lang = find_lang(langs_group_array, start, (ngx_uint_t) (pos - start));
    if (lang != NULL) {
      return lang;
    }

    // Discard the quality value
    if (*pos == ';') {
      while (pos < end && *pos != ',') {
        pos++;
      }
    }
    if (*pos == ',') {
      pos++;
    }
    start = pos;
  }
  return NULL;
}

static ngx_int_t ngx_http_select_lang_variable(ngx_http_request_t *r, ngx_http_variable_value_t *v, uintptr_t data) {
  ngx_array_t *langs_group_array = (ngx_array_t *) data;
  ngx_str_t *lang = find_lang_from_cookie(r, v, langs_group_array);
  if (lang == NULL) {
    lang = find_lang_from_accept_lang(r, v, langs_group_array);
  }
  if (lang == NULL) {
    // If not found, use the first language as a default.
    ngx_array_t *langs_group = (ngx_array_t *) langs_group_array->elts;
    lang = (ngx_str_t *) ((ngx_array_t *)&langs_group[0])->elts;
  }

  v->data = lang->data;
  v->len = lang->len;
  v->valid = 1;
  v->no_cacheable = 0;
  v->not_found = 0;

  return NGX_OK;
}
