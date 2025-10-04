#define kk_function_as_ptr(f,ctx) ((void*)kk_datatype_as_ptr(f, ctx))
#define kk_function_from_ptr(p,ctx) (kk_datatype_from_ptr(p, ctx))

#define kk_owned_handle_to_uv_handle(tp, hndl) ((tp*)kk_cptr_unbox_borrowed(hndl.internal, kk_context()))
#define uv_handle_to_owned_kk_handle(hndl, free_fn, mod, tp) \
  kk_uv_##mod##__new_Uv_##tp(kk_cptr_raw_box(&free_fn, (void*)hndl, kk_context()), kk_context())
#define uv_handle_to_owned_kk_handle_box(hndl, free_fn, mod, tp) \
 kk_uv_##mod##__uv_##tp##_box(uv_handle_to_owned_kk_handle(hndl,free_fn,mod,tp), kk_context())

#define handle_to_owned_kk_handle(hndl, free_fn, mod, tp) \
  kk_uv_##mod##__new_##tp(kk_cptr_raw_box(&free_fn, (void*)hndl, kk_context()), kk_context())
#define handle_to_owned_kk_handle_box(hndl, free_fn, mod, lctp, tp) \
  kk_uv_##mod##__##lctp##_box(uv_handle_to_owned_kk_handle(hndl,free_fn,mod,tp), kk_context())

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#include <uv.h>

#define kk_unit_callback(callback) \
  kk_function_call(void, (kk_function_t, kk_context_t*), callback, (callback, kk_context()), kk_context());

static kk_decl_thread uv_loop_t* kk_uv_loop_default;

void kk_set_uv_loop(uv_loop_t* loop);
uv_loop_t* uvloop();

#define UV_OK 0
kk_std_core_exn__error kk_async_error_from_errno( int err, kk_context_t* ctx );

uv_buf_t* kk_bytes_list_to_uv_buffs(kk_std_core_types__list buffs, int* size, kk_context_t* _ctx);
uv_buf_t* kk_bytes_to_uv_buffs(kk_bytes_t bytes, kk_context_t* _ctx);

void kk_handle_free(void* p, kk_block_t* block, kk_context_t* _ctx);

#define kk_uv_exn_callback(callback, result) \
  kk_function_call(void, (kk_function_t, kk_std_core_exn__error, kk_context_t*), callback, (callback, result, kk_context()), kk_context());

#define kk_uv_status_code_callback(callback, status) \
  kk_function_call(void, (kk_function_t, kk_uv_utils__uv_status_code, kk_context_t*), callback, (callback, kk_uv_utils_int_fs_status_code(status, _ctx), kk_context()), kk_context());

#define kk_uv_error_callback(callback, result) \
  kk_uv_exn_callback(callback, kk_async_error_from_errno(result, kk_context()))

#define kk_uv_okay_callback(callback, result) \
  kk_uv_exn_callback(callback, kk_std_core_exn__new_Ok(result, kk_context()))

typedef struct {
  kk_function_t callback;
  kk_box_t hnd;
  // TODO add `bytes`?
} kk_hnd_callback_t;

// TODO drop this struct? Or at least make the layout match kk_hnd_callback_t?
typedef struct kk_uv_buff_callback_s {
  kk_function_t callback;
  kk_bytes_t bytes;
} kk_uv_buff_callback_t;

// TODO: remove?
static inline kk_uv_buff_callback_t* kk_new_uv_buff_callback(kk_function_t cb, kk_bytes_t bytes, uv_handle_t* handle, kk_context_t* _ctx) {
  kk_uv_buff_callback_t* c = kk_malloc(sizeof(kk_uv_buff_callback_t), _ctx);
  c->callback = cb;
  c->bytes = bytes;
  handle->data = c;
  return c;
}

// TODO: remove?
// Sets the data of the handle to point to the callback
// TODO: Check if data is already assigned?
#define kk_set_hnd_cb(hnd_t, handle, uvhnd, callback) \
  hnd_t* uvhnd = kk_owned_handle_to_uv_handle(hnd_t, handle); \
  kk_hnd_callback_t* uvhnd_cb = kk_malloc(sizeof(kk_hnd_callback_t), kk_context()); \
  uvhnd_cb->callback = callback; \
  uvhnd_cb->hnd = handle.internal; \
  uvhnd->data = uvhnd_cb;

// TODO: remove?
// TODO: Should I get the ctx from the loop?
#define kk_uv_hnd_get_callback(handle, kk_hnd, callback) \
  kk_context_t* _ctx = kk_get_context(); \
  kk_hnd_callback_t* hndcb = (kk_hnd_callback_t*)handle->data; \
  kk_box_t kk_hnd = hndcb->hnd; \
  kk_function_t callback = hndcb->callback;

// Get the raw handle corresponding to a boxed handle (or request!)
static inline uv_handle_t* kk_uv_get_raw_handle(kk_box_t kk_handle, kk_context_t* _ctx) {
  return (uv_handle_t*)kk_cptr_raw_unbox_borrowed(kk_handle, _ctx);
}

// Allocate, initialize and box a handle.
// For simplicity, aborts if the resource fails to init.
// Fix this if we encounter a resource which can actually fail initialization
#define KK_ALLOC_AND_BOX_HANDLE(typ) \
  uv_##typ##_t* typ##_uvhnd = (uv_##typ##_t*)kk_malloc(sizeof(uv_##typ##_t), kk_context()); \
  typ##_uvhnd->data = NULL; \
  kk_box_t typ = kk_cptr_raw_box(&kk_handle_free, typ##_uvhnd, _ctx); \
  kk_info_message("created uv_handle type %s\n", #typ);


// TODO: remove?
#define kk_new_req_cb(req_t, req, cb) \
  req_t* req = kk_malloc(sizeof(req_t), kk_context()); \
  req->data = kk_function_as_ptr(cb, kk_context());

// Sets the data of the handle to point to the callback struct
// (containing the function and a reference to the handle)
static inline void kk_assign_uv_callback(
  kk_box_t handle,
  kk_function_t cb,
  kk_context_t* _ctx
) {
  uv_handle_t* uvhnd = kk_uv_get_raw_handle(handle, _ctx);
  kk_assert(uvhnd->data == NULL);
  kk_hnd_callback_t* cb_struct = kk_malloc(sizeof(kk_hnd_callback_t), _ctx);
  cb_struct->hnd = handle;
  cb_struct->callback = cb;
  uvhnd->data = cb_struct;
}

// TODO: remove?
#define kk_uv_get_callback(req, cb) \
  kk_context_t* _ctx = kk_get_context(); \
  kk_function_t cb = kk_function_from_ptr(req->data, kk_context());

// access the cb_struct from a raw handle
static inline kk_hnd_callback_t* kk_get_callback_struct(uv_handle_t* uvhnd) {
  return (kk_hnd_callback_t*)uvhnd->data;
}

static inline void kk_resolve_callback_struct(
  uv_handle_t* uvhnd,
  kk_hnd_callback_t* cb_struct,
  kk_box_t* arg,
  kk_context_t* _ctx
) {
  uvhnd->data = NULL;

  if (cb_struct == NULL) {
    kk_warning_message("kk_resolve_callback_struct(cb=NULL) - dangling libuv operation?\n");
    return;
  }

  kk_assert(cb_struct != NULL);
  kk_function_t callback = cb_struct->callback;

  // cb_struct->callback is "dropped" by calling it; so we just need to drop the handle
  kk_box_drop(cb_struct->hnd, _ctx);
  kk_free(cb_struct, _ctx);

  if (arg == NULL) {
    kk_function_call(void, (kk_function_t, kk_context_t*), callback, (callback, _ctx), _ctx);
  } else {
    kk_function_call(void, (kk_function_t, kk_box_t, kk_context_t*), callback, (callback, *arg, _ctx), _ctx);
  }
}

// When invoking an operation which we know will cause a pending callback to be
// skipped (e.g. uv_timer_stop), we explicitly free the callback resources.
// Note that some "abort" operations will still invoke the callback (with status=UV_ECANCELED),
// this function should not be used for those operations.
static inline void kk_drop_pending_uv_callback(
  uv_handle_t* uvhnd,
  kk_context_t* _ctx
) {
  kk_hnd_callback_t* cb_struct = kk_get_callback_struct(uvhnd);
  if(cb_struct == NULL) {
    // perhaps the operation was cancelled after the callback was invoked
    return;
  }
  uvhnd->data = NULL;
  kk_function_drop(cb_struct->callback, _ctx);
  kk_box_drop(cb_struct->hnd, _ctx);
  kk_free(cb_struct, _ctx);
}

// Convenience to access then invoke callback
static inline void kk_resolve_uv_callback(uv_handle_t* uvhnd, kk_box_t* arg, kk_context_t* _ctx) {
  kk_resolve_callback_struct(uvhnd, kk_get_callback_struct(uvhnd), arg, _ctx);
}


// TODO: Change all apis to return status code or return error, not a mix

// If the status is not OK, return Error(...). Otherwise continue
#define kk_uv_check_bail(status) \
  if (status < UV_OK) { \
    return kk_async_error_from_errno(status, kk_context()); \
  }

// If the status is not OK, return Error(...) (after dropping)
#define kk_uv_check_bail_drops(status, drops) \
  if (status < UV_OK) { \
    do drops while (0); \
    return kk_async_error_from_errno(status, kk_context()); \
  }

// Return Ok(()) or Error(...) based on status. Drops in failure case.
#define kk_uv_check_status_drops(status, drops) \
  if (status < UV_OK) { \
    do drops while (0); \
  } \
  return kk_uv_utils_int_fs_status_code(status, kk_context());

// Sometimes the return value is a file descriptor which is why this is a < UV_OK check instead of == UV_OK
#define kk_uv_check_return(err, result) \
  if (err < UV_OK) { \
    return kk_async_error_from_errno(err, kk_context()); \
  } else { \
    return kk_std_core_exn__new_Ok(result, kk_context()); \
  }

// Typically used to clean up when an error occurs
#define kk_uv_check_return_err_drops(err, result, drops) \
  if (err < UV_OK) { \
    do drops while (0); \
    return kk_async_error_from_errno(err, kk_context()); \
  } else { \
    return kk_std_core_exn__new_Ok(result, kk_context()); \
  }

// Typically used when cleaning up a handle
#define kk_uv_check_return_ok_drops(err, result, drops) \
  if (err < UV_OK) { \
    return kk_async_error_from_errno(err, kk_context()); \
  } else { \
    do drops while (0); \
    return kk_std_core_exn__new_Ok(result, kk_context()); \
  }

// Check the uv status code and return a kk_std_core_exn__error Ok or Error
#define kk_uv_check(err) kk_uv_check_return(err, kk_unit_box(kk_Unit))

// Check the uv status code and return a kk_std_core_exn__error Ok or Error
// Dropping the references if it was an error
#define kk_uv_check_err_drops(err, drops) \
  kk_uv_check_return_err_drops(err, kk_unit_box(kk_Unit), drops)

// Check the uv status code and return a kk_std_core_exn__error Ok or Error
// Dropping the references if the result is Okay
#define kk_uv_check_ok_drops(err, drops) \
  kk_uv_check_return_ok_drops(err, kk_unit_box(kk_Unit), drops)

#endif
