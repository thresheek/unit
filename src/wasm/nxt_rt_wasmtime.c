#include <stdio.h>
#include <stdbool.h>

#include <wasm.h>
#include <wasi.h>
#include <wasmtime.h>

#include "nxt_wasm.h"

typedef struct {
    wasm_engine_t       *engine;
    wasmtime_store_t    *store;
    wasmtime_memory_t   memory;
    wasmtime_module_t   *module;
    wasmtime_linker_t   *linker;
    wasmtime_context_t  *ctx;
} nxt_wasmtime_ctx_t;

static nxt_wasmtime_ctx_t  nxt_wasmtime_ctx;


static void
nxt_wasmtime_err_msg(const char *message, wasmtime_error_t *error,
                     wasm_trap_t *trap)
{
    wasm_byte_vec_t  error_message;

    fprintf(stderr, "wasmtime error: %s\n", message);
    if (error != NULL) {
        wasmtime_error_message(error, &error_message);
        wasmtime_error_delete(error);
    } else {
        wasm_trap_message(trap, &error_message);
        wasm_trap_delete(trap);
    }
    fprintf(stderr, "wasmtime: %.*s\n", (int)error_message.size,
            error_message.data);

    wasm_byte_vec_delete(&error_message);
}


static void
nxt_wasmtime_meminfo(const nxt_wasm_ctx_t *ctx)
{
    nxt_wasmtime_ctx_t  *rt_ctx = &nxt_wasmtime_ctx;

    printf("==[MEMINFO] Linear memory @ %p is %" PRIu64 " pages totalling "
           "%lu bytes\n",
           ctx->baddr - ctx->baddr_offs,
           wasmtime_memory_size(rt_ctx->ctx, &rt_ctx->memory),
           wasmtime_memory_data_size(rt_ctx->ctx, &rt_ctx->memory));
}


static wasm_trap_t
*nxt_wasm_get_init_mem_size(void *env, wasmtime_caller_t *caller,
                            const wasmtime_val_t *args, size_t nargs,
                            wasmtime_val_t *results, size_t nresults)
{
    results[0].of.i32 = NXT_WASM_MEM_SIZE;

    return NULL;
}


static wasm_trap_t
*nxt_wasm_response_end(void *env, wasmtime_caller_t *caller,
                       const wasmtime_val_t *args, size_t nargs,
                       wasmtime_val_t *results, size_t nresults)
{
    nxt_wasm_do_response_end(env);

    return NULL;
}


static wasm_trap_t
*nxt_wasm_send_response(void *env, wasmtime_caller_t *caller,
                        const wasmtime_val_t *args, size_t nargs,
                        wasmtime_val_t *results, size_t nresults)
{
    printf("\n# Got response @ [%u]\n", args[0].of.i32);
    nxt_wasm_do_send_response(args[0].of.i32, env);

    return NULL;
}


static wasm_trap_t
*nxt_wasm_send_headers(void *env, wasmtime_caller_t *caller,
                       const wasmtime_val_t *args, size_t nargs,
                       wasmtime_val_t *results, size_t nresults)
{
    printf("\n# Got response headers @ [%u]\n", args[0].of.i32);
    nxt_wasm_do_send_headers(args[0].of.i32, env);
    printf("\n");

    return NULL;
}


static void
nxt_wasmtime_execute_hook(const nxt_wasm_ctx_t *ctx, nxt_wasm_fh_t hook)
{
    const char             *name = ctx->fh[hook].func_name;
    wasm_trap_t            *trap = NULL;
    wasmtime_error_t       *error;
    nxt_wasmtime_ctx_t     *rt_ctx = &nxt_wasmtime_ctx;
    const nxt_wasm_func_t  *func = &ctx->fh[hook].func;

    if (name == NULL) {
        return;
    }

    error = wasmtime_func_call(rt_ctx->ctx, func, NULL, 0, NULL, 0, &trap);
    if (error != NULL || trap != NULL) {
        nxt_wasmtime_err_msg("failed to call function [->hook]", error, trap);
    }
}


static void
nxt_wasmtime_execute_request(const nxt_wasm_ctx_t *ctx)
{
    int                    i = 0;
    wasm_trap_t            *trap= NULL;
    wasmtime_val_t         args[1] = { };
    wasmtime_val_t         results[1] = { };
    wasmtime_error_t       *error;
    nxt_wasmtime_ctx_t     *rt_ctx = &nxt_wasmtime_ctx;
    const nxt_wasm_func_t  *func = &ctx->fh[NXT_WASM_FH_REQUEST].func;

    args[i].kind = WASMTIME_I32;
    args[i++].of.i32 = ctx->baddr_offs;

    { /* XXX DEBUG. Once removed also remove inttypes.h include */
        nxt_wasm_request_t *wr;
        nxt_wasm_http_hdr_field_t *f, *f_end;

        wr = (nxt_wasm_request_t *)ctx->baddr;

        printf("== Header data starts @ %lu bytes\n",
               sizeof(nxt_wasm_request_t) +
               (sizeof(nxt_wasm_http_hdr_field_t) * wr->nr_fields));

        printf("-- Got [%u] headers\n", wr->nr_fields);

        f_end = wr->fields + wr->nr_fields;
        for (f = wr->fields; f < f_end; f++)
            printf("== [%u/%.*s] = [%u/%u/%.*s]\n",
                   f->name_offs, f->name_len,
                   (unsigned char *)wr + f->name_offs,
                   f->value_len, f->value_offs, f->value_len,
                   (unsigned char *)wr + f->value_offs);
    }
    error = wasmtime_func_call(rt_ctx->ctx, func, args, i, results, 1, &trap);
    if (error != NULL || trap != NULL) {
        nxt_wasmtime_err_msg("failed to call function [->wasm_request_handler]",
                             error, trap);
    }
}


typedef enum {
    NXT_WASM_FT_0_0,
    NXT_WASM_FT_1_0,
    NXT_WASM_FT_0_1,
} nxt_wasm_func_type_t;

typedef struct {
    const char                *func_name;
    wasmtime_func_callback_t  func;
    wasm_valkind_t            params[1];
    wasm_valkind_t            results[1];
    nxt_wasm_func_type_t      ft;
} nxt_wasm_function_import_t;

static void
nxt_wasmtime_set_function_imports(nxt_wasm_ctx_t *ctx)
{
    nxt_wasmtime_ctx_t                       *rt_ctx = &nxt_wasmtime_ctx;
    static const nxt_wasm_function_import_t  *imf;
    static const nxt_wasm_function_import_t  import_functions[] = {
        {
            .func_name = "nxt_wasm_get_init_mem_size",
            .func      = nxt_wasm_get_init_mem_size,
            .results   = { WASM_I32 },
            .ft        = NXT_WASM_FT_0_1
        }, {
            .func_name = "nxt_wasm_response_end",
            .func      = nxt_wasm_response_end,
            .ft        = NXT_WASM_FT_0_0
        }, {
            .func_name = "nxt_wasm_send_response",
            .func      = nxt_wasm_send_response,
            .params    = { WASM_I32 },
            .ft        = NXT_WASM_FT_1_0
        }, {
            .func_name = "nxt_wasm_send_headers",
            .func      = nxt_wasm_send_headers,
            .params    = { WASM_I32 },
            .ft        = NXT_WASM_FT_1_0
        },

        { }
    };

    for (imf = import_functions; imf->func_name != NULL; imf++) {
        wasm_functype_t  *func_ty;

        switch (imf->ft) {
        case NXT_WASM_FT_0_0:
            func_ty = wasm_functype_new_0_0();
            break;
        case NXT_WASM_FT_1_0:
            func_ty = wasm_functype_new_1_0(wasm_valtype_new(imf->params[0]));
            break;
        case NXT_WASM_FT_0_1:
            func_ty = wasm_functype_new_0_1(wasm_valtype_new(imf->results[0]));
            break;
        default:
            /* Stop GCC complaining about func_ty being used uninitialised */
            func_ty = NULL;
        }

        wasmtime_linker_define_func(rt_ctx->linker, "env", 3,
                                    imf->func_name, strlen(imf->func_name),
                                    func_ty,  imf->func, ctx, NULL);
        wasm_functype_delete(func_ty);
    }
}


static int
nxt_wasmtime_get_function_exports(nxt_wasm_ctx_t *ctx)
{
    int                 i;
    nxt_wasmtime_ctx_t  *rt_ctx = &nxt_wasmtime_ctx;

    for (i = 0; i < NXT_WASM_FH_NR; i++) {
        bool               ok;
        wasmtime_extern_t  item;

        if (ctx->fh[i].func_name == NULL) {
            continue;
        }

       ok = wasmtime_linker_get(rt_ctx->linker, rt_ctx->ctx, "", 0,
                                ctx->fh[i].func_name,
                                strlen(ctx->fh[i].func_name), &item);
       if (!ok) {
           fprintf(stderr, "%s: Couldn't get (%s) from module\n", __func__,
                   ctx->fh[i].func_name);
           return -1;
       }
       ctx->fh[i].func = item.of.func;
    }

    return 0;
}


static int
nxt_wasmtime_wasi_init(const nxt_wasm_ctx_t *ctx)
{
    char                **dir;
    wasi_config_t       *wasi_config;
    wasmtime_error_t    *error;
    nxt_wasmtime_ctx_t  *rt_ctx = &nxt_wasmtime_ctx;

    wasi_config = wasi_config_new();

    wasi_config_inherit_env(wasi_config);
    wasi_config_inherit_stdin(wasi_config);
    wasi_config_inherit_stdout(wasi_config);
    wasi_config_inherit_stderr(wasi_config);

    for (dir = ctx->dirs; dir != NULL && *dir != NULL; dir++) {
        printf("### wasmtime allowing guest access to (%s)\n", *dir);
        wasi_config_preopen_dir(wasi_config, *dir, *dir);
    }

    error = wasmtime_context_set_wasi(rt_ctx->ctx, wasi_config);
    if (error != NULL) {
        nxt_wasmtime_err_msg("failed to instantiate WASI", error, NULL);
        return -1;
    }

    return 0;
}


static int
nxt_wasmtime_init_memory(nxt_wasm_ctx_t *ctx)
{
    int                    i = 0;
    bool                   ok;
    wasm_trap_t            *trap = NULL;
    wasmtime_val_t         args[1] = { };
    wasmtime_val_t         results[1] = { };
    wasmtime_error_t       *error;
    wasmtime_extern_t      item;
    nxt_wasmtime_ctx_t     *rt_ctx = &nxt_wasmtime_ctx;
    const nxt_wasm_func_t  *func = &ctx->fh[NXT_WASM_FH_MALLOC].func;

    args[i].kind = WASMTIME_I32;
    args[i++].of.i32 = NXT_WASM_MEM_SIZE + NXT_WASM_PAGE_SIZE;

    error = wasmtime_func_call(rt_ctx->ctx, func, args, i, results, 1, &trap);
    if (error != NULL || trap != NULL) {
        nxt_wasmtime_err_msg("failed to call function [->wasm_malloc_handler]",
                             error, trap);
        return -1;
    }

    ok = wasmtime_linker_get(rt_ctx->linker, rt_ctx->ctx, "", 0, "memory",
                             strlen("memory"), &item);
    if (!ok) {
        fprintf(stderr, "%s: Couldn't get 'memory' from module\n", __func__);
        return -1;
    }
    rt_ctx->memory = item.of.memory;

    ctx->baddr_offs = results[0].of.i32;
    ctx->baddr = wasmtime_memory_data(rt_ctx->ctx, &rt_ctx->memory);

    printf("==[MEMINFO] Linear memory base addr : %p\n", ctx->baddr);
    ctx->baddr += ctx->baddr_offs;
    printf("==[MEMINFO] Linear memory WASM memory addr : %p\n", ctx->baddr);

    nxt_wasmtime_meminfo(ctx);

    return 0;
}

static int
nxt_wasmtime_init(nxt_wasm_ctx_t *ctx)
{
    int                 err;
    FILE                *fp;
    size_t              file_size;
    wasm_byte_vec_t     wasm;
    wasmtime_error_t    *error;
    nxt_wasmtime_ctx_t  *rt_ctx = &nxt_wasmtime_ctx;

    rt_ctx->engine = wasm_engine_new();
    rt_ctx->store = wasmtime_store_new(rt_ctx->engine, NULL, NULL);
    rt_ctx->ctx = wasmtime_store_context(rt_ctx->store);

    rt_ctx->linker = wasmtime_linker_new(rt_ctx->engine);
    error = wasmtime_linker_define_wasi(rt_ctx->linker);
    if (error != NULL) {
        nxt_wasmtime_err_msg("failed to link wasi", error, NULL);
        return -1;
    }

    fp = fopen(ctx->module_path, "r");
    if (!fp) {
        fprintf(stderr, "%s: Error loading file (%s)\n", __func__,
                ctx->module_path);
        return -1;
    }
    fseek(fp, 0L, SEEK_END);
    file_size = ftell(fp);
    wasm_byte_vec_new_uninitialized(&wasm, file_size);
    fseek(fp, 0L, SEEK_SET);
    if (fread(wasm.data, file_size, 1, fp) != 1) {
        fprintf(stderr, "%s: Error loading module\n", __func__);
        return -1;
    }
    fclose(fp);

    error = wasmtime_module_new(rt_ctx->engine, (uint8_t *)wasm.data, wasm.size,
                                &rt_ctx->module);
    if (!rt_ctx->module) {
        nxt_wasmtime_err_msg("failed to compile module", error, NULL);
        return -1;
    }
    wasm_byte_vec_delete(&wasm);

    nxt_wasmtime_set_function_imports(ctx);

    nxt_wasmtime_wasi_init(ctx);

    printf("Instantiating module...\n");
    error = wasmtime_linker_module(rt_ctx->linker, rt_ctx->ctx, "", 0,
                                   rt_ctx->module);
    if (error != NULL) {
         nxt_wasmtime_err_msg("failed to instantiate", error, NULL);
         return -1;
    }

    err = nxt_wasmtime_get_function_exports(ctx);
    if (err)
        return -1;

    err = nxt_wasmtime_init_memory(ctx);
    if (err)
        return -1;

    return 0;
}


static void
nxt_wasmtime_destroy(const nxt_wasm_ctx_t *ctx)
{
    int                    i = 0;
    wasmtime_val_t         args[1] = { };
    nxt_wasmtime_ctx_t     *rt_ctx = &nxt_wasmtime_ctx;
    const nxt_wasm_func_t  *func = &ctx->fh[NXT_WASM_FH_FREE].func;

    args[i].kind = WASMTIME_I32;
    args[i++].of.i32 = ctx->baddr_offs;

    (void)wasmtime_func_call(rt_ctx->ctx, func, args, i, NULL, 0, NULL);

    wasmtime_module_delete(rt_ctx->module);
    wasmtime_store_delete(rt_ctx->store);
    wasm_engine_delete(rt_ctx->engine);
}


const nxt_wasm_operations_t  wasm_ops = {
    .init               = nxt_wasmtime_init,
    .destroy            = nxt_wasmtime_destroy,
    .exec_request       = nxt_wasmtime_execute_request,
    .exec_hook          = nxt_wasmtime_execute_hook,
    .meminfo            = nxt_wasmtime_meminfo,
};
