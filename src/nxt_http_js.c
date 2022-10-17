
/*
 * Copyright (C) NGINX, Inc.
 */

#include <nxt_router.h>
#include <nxt_http.h>
#include <njs.h>


static njs_int_t nxt_http_js_ext_method(njs_vm_t *vm, njs_object_prop_t *prop,
    njs_value_t *value, njs_value_t *setval, njs_value_t *retval);
static njs_int_t nxt_http_js_ext_uri(njs_vm_t *vm, njs_object_prop_t *prop,
    njs_value_t *value, njs_value_t *setval, njs_value_t *retval);
static njs_int_t nxt_http_js_ext_version(njs_vm_t *vm, njs_object_prop_t *prop,
    njs_value_t *value, njs_value_t *setval, njs_value_t *retval);
static njs_int_t nxt_http_js_ext_host(njs_vm_t *vm, njs_object_prop_t *prop,
    njs_value_t *value, njs_value_t *setval, njs_value_t *retval);
static njs_int_t nxt_http_js_ext_remote_addr(njs_vm_t *vm,
    njs_object_prop_t *prop, njs_value_t *value, njs_value_t *setval,
    njs_value_t *retval);
static njs_int_t nxt_http_js_ext_status(njs_vm_t *vm, njs_object_prop_t *prop,
    njs_value_t *value, njs_value_t *setval, njs_value_t *retval);
static njs_int_t nxt_http_js_ext_get_arg(njs_vm_t *vm,
    njs_object_prop_t *prop, njs_value_t *value, njs_value_t *setval,
    njs_value_t *retval);
static njs_int_t nxt_http_js_ext_keys_arg(njs_vm_t *vm, njs_value_t *value,
    njs_value_t *keys);
static njs_int_t nxt_http_js_ext_get_header(njs_vm_t *vm,
    njs_object_prop_t *prop, njs_value_t *value, njs_value_t *setval,
    njs_value_t *retval);
static njs_int_t nxt_http_js_ext_keys_header(njs_vm_t *vm, njs_value_t *value,
    njs_value_t *keys);


static njs_external_t  nxt_http_js_proto[] = {
    {
        .flags = NJS_EXTERN_PROPERTY,
        .name.string = njs_str("method"),
        .enumerable = 1,
        .u.property = {
            .handler = nxt_http_js_ext_method,
        }
    },

    {
        .flags = NJS_EXTERN_PROPERTY,
        .name.string = njs_str("uri"),
        .enumerable = 1,
        .u.property = {
            .handler = nxt_http_js_ext_uri,
        }
    },

    {
        .flags = NJS_EXTERN_PROPERTY,
        .name.string = njs_str("version"),
        .enumerable = 1,
        .u.property = {
            .handler = nxt_http_js_ext_version,
        }
    },

    {
        .flags = NJS_EXTERN_PROPERTY,
        .name.string = njs_str("host"),
        .enumerable = 1,
        .u.property = {
            .handler = nxt_http_js_ext_host,
        }
    },

    {
        .flags = NJS_EXTERN_PROPERTY,
        .name.string = njs_str("remoteAddr"),
        .enumerable = 1,
        .u.property = {
            .handler = nxt_http_js_ext_remote_addr,
        }
    },

    {
        .flags = NJS_EXTERN_PROPERTY,
        .name.string = njs_str("status"),
        .enumerable = 1,
        .u.property = {
            .handler = nxt_http_js_ext_status,
        }
    },

    {
        .flags = NJS_EXTERN_OBJECT,
        .name.string = njs_str("args"),
        .enumerable = 1,
        .u.object = {
            .enumerable = 1,
            .prop_handler = nxt_http_js_ext_get_arg,
            .keys = nxt_http_js_ext_keys_arg,
        }
    },

    {
        .flags = NJS_EXTERN_OBJECT,
        .name.string = njs_str("headers"),
        .enumerable = 1,
        .u.object = {
            .enumerable = 1,
            .prop_handler = nxt_http_js_ext_get_header,
            .keys = nxt_http_js_ext_keys_header,
        }
    },
};


void
nxt_http_register_js_proto(nxt_js_conf_t *jcf)
{
    nxt_js_set_proto(jcf, nxt_http_js_proto, njs_nitems(nxt_http_js_proto));
}


static njs_int_t
nxt_http_js_ext_method(njs_vm_t *vm, njs_object_prop_t *prop,
    njs_value_t *value, njs_value_t *setval, njs_value_t *retval)
{
    nxt_http_request_t  *r;

    r = njs_vm_external(vm, nxt_js_proto_id, value);
    if (r == NULL) {
        njs_value_undefined_set(retval);
        return NJS_DECLINED;
    }

    return njs_vm_value_string_set(vm, retval, r->method->start,
                                   r->method->length);
}


static njs_int_t
nxt_http_js_ext_uri(njs_vm_t *vm, njs_object_prop_t *prop,
    njs_value_t *value, njs_value_t *setval, njs_value_t *retval)
{
    nxt_http_request_t  *r;

    r = njs_vm_external(vm, nxt_js_proto_id, value);
    if (r == NULL) {
        njs_value_undefined_set(retval);
        return NJS_DECLINED;
    }

    return njs_vm_value_string_set(vm, retval, r->path->start, r->path->length);
}


static njs_int_t
nxt_http_js_ext_version(njs_vm_t *vm, njs_object_prop_t *prop,
    njs_value_t *value, njs_value_t *setval, njs_value_t *retval)
{
    nxt_http_request_t  *r;

    r = njs_vm_external(vm, nxt_js_proto_id, value);
    if (r == NULL) {
        njs_value_undefined_set(retval);
        return NJS_DECLINED;
    }

    return njs_vm_value_string_set(vm, retval, r->version.start,
                                   r->version.length);
}


static njs_int_t
nxt_http_js_ext_host(njs_vm_t *vm, njs_object_prop_t *prop,
    njs_value_t *value, njs_value_t *setval, njs_value_t *retval)
{
    nxt_http_request_t  *r;

    r = njs_vm_external(vm, nxt_js_proto_id, value);
    if (r == NULL) {
        njs_value_undefined_set(retval);
        return NJS_DECLINED;
    }

    return njs_vm_value_string_set(vm, retval, r->host.start, r->host.length);
}


static njs_int_t
nxt_http_js_ext_remote_addr(njs_vm_t *vm, njs_object_prop_t *prop,
    njs_value_t *value, njs_value_t *setval, njs_value_t *retval)
{
    nxt_http_request_t  *r;

    r = njs_vm_external(vm, nxt_js_proto_id, value);
    if (r == NULL) {
        njs_value_undefined_set(retval);
        return NJS_DECLINED;
    }

    return njs_vm_value_string_set(vm, retval,
                                   nxt_sockaddr_address(r->remote),
                                   r->remote->address_length);
}


static njs_int_t
nxt_http_js_ext_status(njs_vm_t *vm, njs_object_prop_t *prop,
    njs_value_t *value, njs_value_t *setval, njs_value_t *retval)
{
    nxt_str_t           str;
    nxt_http_request_t  *r;

    r = njs_vm_external(vm, nxt_js_proto_id, value);
    if (r == NULL) {
        njs_value_undefined_set(retval);
        return NJS_DECLINED;
    }

    str.start = nxt_mp_nget(r->mem_pool, 3);
    if (nxt_slow_path(str.start == NULL)) {
        return NJS_ERROR;
    }

    str.length = nxt_sprintf(str.start, str.start + 3, "%03d", r->status)
                             - str.start;

    return njs_vm_value_string_set(vm, retval, str.start, str.length);
}


static njs_int_t
nxt_http_js_ext_get_arg(njs_vm_t *vm, njs_object_prop_t *prop,
    njs_value_t *value, njs_value_t *setval, njs_value_t *retval)
{
    njs_int_t              rc;
    njs_str_t              key;
    nxt_array_t            *args;
    nxt_http_request_t     *r;
    nxt_http_name_value_t  *nv, *start;

    r = njs_vm_external(vm, nxt_js_proto_id, value);
    if (r == NULL) {
        njs_value_undefined_set(retval);
        return NJS_DECLINED;
    }

    rc = njs_vm_prop_name(vm, prop, &key);
    if (rc != NJS_OK) {
        njs_value_undefined_set(retval);
        return NJS_DECLINED;
    }

    args = nxt_http_arguments_parse(r);
    if (nxt_slow_path(args == NULL)) {
        return NJS_ERROR;
    }

    start = args->elts;
    nv = start + args->nelts - 1;

    while (nv >= start) {

        if (key.length == nv->name_length
            && nxt_memcmp(key.start, nv->name, nv->name_length) == 0)
        {
            return njs_vm_value_string_set(vm, retval, nv->value,
                                           nv->value_length);
        }

        nv--;
    }

    njs_value_undefined_set(retval);

    return NJS_DECLINED;
}


static njs_int_t
nxt_http_js_ext_keys_arg(njs_vm_t *vm, njs_value_t *value, njs_value_t *keys)
{
    return NJS_OK;
}


static njs_int_t
nxt_http_js_ext_get_header(njs_vm_t *vm, njs_object_prop_t *prop,
    njs_value_t *value, njs_value_t *setval, njs_value_t *retval)
{
    njs_int_t           rc;
    njs_str_t           key;
    nxt_http_field_t    *f;
    nxt_http_request_t  *r;

    r = njs_vm_external(vm, nxt_js_proto_id, value);
    if (r == NULL) {
        njs_value_undefined_set(retval);
        return NJS_DECLINED;
    }

    rc = njs_vm_prop_name(vm, prop, &key);
    if (rc != NJS_OK) {
        njs_value_undefined_set(retval);
        return NJS_DECLINED;
    }

    nxt_list_each(f, r->fields) {

        if (key.length == f->name_length
            && nxt_memcmp(key.start, f->name, f->name_length) == 0)
        {
            return njs_vm_value_string_set(vm, retval, f->value,
                                           f->value_length);
        }

    } nxt_list_loop;

    njs_value_undefined_set(retval);

    return NJS_DECLINED;
}


static njs_int_t
nxt_http_js_ext_keys_header(njs_vm_t *vm, njs_value_t *value, njs_value_t *keys)
{
    return NJS_OK;
}
