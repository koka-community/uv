#include <uv.h>

#define kk_uv_status_code(i, _ctx) kk_uv_status_dash_code_int_fs_status_code(i, _ctx)

static inline char has_callback(kk_uv_any_t* handle, kk_context_t* _ctx) {
  return !kk_function_is_null(handle->callback, _ctx);
}

// ------------------------------
// Error / exception helpers
// ------------------------------

// return an error for the given status
__attribute__((unused))
static kk_std_core_exn__error kk_uv_error(int status, kk_context_t* _ctx) {
  kk_uv_status_code_t code = kk_uv_status_code(status, _ctx);
  kk_string_t msg = kk_uv_status_dash_code_message(code, _ctx);
  return kk_std_core_types__new_Error(
    kk_std_core_exn__exception_box(
      kk_std_core_exn__new_Exception(
        msg,
        kk_uv_status_dash_code__new_AsyncExn(kk_reuse_null, 0, code, _ctx),
        _ctx),
      _ctx),
    _ctx);
}

// return `Ok(ok_expr)` if the status is UV_OK, otherwise Error(...)
#define kk_uv_error_or(status, ok_expr, _ctx) \
  ((status >= 0) ? kk_std_core_types__new_Ok(ok_expr, _ctx) : kk_uv_error(status, _ctx))


// ------------------------------
// Helpers for invoking callbacks
// with common argument types
// ------------------------------

__attribute__((unused))
static void kk_uv_status_code_callback(kk_function_t callback, int status, kk_context_t* _ctx) {
  kk_function_call(kk_unit_t,
    (kk_function_t, kk_uv_status_code_t, kk_context_t*),
    callback,
    (callback, kk_uv_status_code(status, _ctx), _ctx), _ctx);
}
__attribute__((unused))
static void kk_uv_error_status_callback(kk_function_t callback, int status, kk_context_t* _ctx) {
  kk_function_call(kk_unit_t,
    (kk_function_t, kk_std_core_exn__error, kk_context_t*),
    callback, (callback, kk_uv_error(status, _ctx), _ctx), _ctx);
}

__attribute__((unused))
static void kk_uv_error_callback(kk_function_t callback, kk_std_core_exn__error value, kk_context_t* _ctx) {
  kk_function_call(kk_unit_t,
    (kk_function_t, kk_std_core_exn__error, kk_context_t*),
    callback, (callback, value, _ctx), _ctx);
}

// ------------------------------
// Handle destruction
// ------------------------------

__attribute__((unused))
static void kk_uv_req_free(kk_uv_any_t *hnd, kk_context_t *_ctx) {
  kk_uv_any_drop_references(hnd, _ctx);
  kk_free(hnd, _ctx);
}

// free a request type after taking (and then returning) its callback
__attribute__((unused))
static kk_function_t kk_uv_req_take_callback_and_free(kk_uv_any_t *hnd, kk_context_t *_ctx) {
  kk_function_t callback = kk_uv_any_take_callback(hnd, _ctx);
  kk_uv_req_free(hnd, _ctx);
  return callback;
}
