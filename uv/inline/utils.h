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
void kk_uv_req_free(void* p, kk_block_t *block, kk_context_t *_ctx);
void kk_hnd_callback_replace_bytes(kk_bytes_t *dest, kk_bytes_t replacement, kk_context_t* _ctx);

#define kk_uv_exn_callback(callback, result) \
  kk_function_call(void, (kk_function_t, kk_std_core_exn__error, kk_context_t*), callback, (callback, result, kk_context()), kk_context());

#define kk_uv_status_code_callback(callback, status) \
  kk_function_call(void, (kk_function_t, kk_uv_utils__uv_status_code, kk_context_t*), callback, (callback, kk_uv_utils_int_fs_status_code(status, _ctx), kk_context()), kk_context());

#define kk_uv_error_callback(callback, result) \
  kk_uv_exn_callback(callback, kk_async_error_from_errno(result, kk_context()))

#define kk_uv_okay_callback(callback, result) \
  kk_uv_exn_callback(callback, kk_std_core_exn__new_Ok(result, kk_context()))

// TODO: rename kk_uv_callback_t?
// TODO: this could be a union if it's too awkward to
// populate all fields
typedef struct  {
  kk_function_t callback;
  kk_box_t hnd;
  kk_bytes_t bytes;
} kk_hnd_callback_t;

void kk_uv_hnd_callback_free(kk_hnd_callback_t* hndcb, kk_context_t* _ctx);
kk_function_t kk_uv_hnd_callback_into_callback(kk_hnd_callback_t* hndcb, kk_context_t* _ctx);
kk_function_t kk_uv_req_into_callback(uv_handle_t *hnd, kk_context_t *_ctx);
kk_function_t kk_uv_hnd_callback_dup(kk_hnd_callback_t* hndcb, kk_context_t* _ctx);

#define kk_uv_hnd_data_free(hnd) \
  if(hnd->data != NULL) { \
    kk_uv_hnd_callback_free((kk_hnd_callback_t*)hnd->data, kk_context()); \
    hnd->data = NULL; \
  };

// friendly names for koka status code type
#define kk_uv_status_code_t kk_uv_utils__uv_status_code
#define kk_uv_status_code(i) kk_uv_utils_int_fs_status_code(i, kk_context())

// ======== PATTERNS ========
// = One-shot request =
//
// Performs a single async request with a plain callback.
// Allocates, installs and then tears down the request/callback if unsuccessful
// TODO: invoke uv_callback_fn instead of kk_callback_fn, to ensure proper cleanup?
#define kk_uv_oneshot_req_setup(kk_callback_fn, req_t, uv_setup_fn, handle_t, handle, ...) \
  kk_new_req_cb(req_t, req, kk_callback_fn); \
  handle_t* uvhnd = kk_owned_handle_to_uv_handle(handle_t, handle); \
  int status = uv_setup_fn(req, uvhnd __VA_OPT__(,) __VA_ARGS__); \
  if (status != UV_OK) { \
    kk_callback_fn = kk_uv_req_into_callback((uv_handle_t*)uvhnd, _ctx); \
    kk_uv_status_code_callback(kk_callback_fn, status); \
  }

// Performs a single async request for the FS module
#define kk_uv_oneshot_fs_setup(cb, bytes, uv_setup_fn, uv_callback_fn, drops, ...) \
  kk_new_req(uv_fs_t, req); \
  req->data = kk_uv_hnd_callback_create(NULL_BOX, cb, bytes, _ctx); \
  int status = uv_setup_fn(uvloop(), req __VA_OPT__(,) __VA_ARGS__, uv_callback_fn); \
  do drops while (0); \
  if (status != UV_OK) { \
    req->result = status; \
    uv_callback_fn(req); \
  }

// Performs a single async request for the FS module.
// TODO remove _box suffix and use this macro for all usages, just explicitly pass in NULL_BOX if needed
#define kk_uv_oneshot_fs_setup_box(box, cb, uv_setup_fn, uv_callback_fn, drops, ...) \
  kk_new_req(uv_fs_t, req); \
  req->data = kk_uv_hnd_callback_create(box, cb, NULL_BYTES, _ctx); \
  int status = uv_setup_fn(uvloop(), req __VA_OPT__(,) __VA_ARGS__, uv_callback_fn); \
  do drops while (0); \
  if (status != UV_OK) { \
    req->result = status; \
    uv_callback_fn(req); \
  }

// Implementation of a oneshot request callback.
// Frees the request
#define kk_uv_oneshot_req_callback(req, status) \
  kk_uv_req_get_callback(req, callback); \
  kk_uv_status_code_callback(callback, status); \
  kk_free(req, kk_context());

// Implementation of a oneshot handle callback.
// Removes the `data` cb struct from the handle.
#define kk_uv_oneshot_hnd_callback(hnd, status) \
  kk_context_t* _ctx = kk_get_context(); \
  kk_uv_hnd_remove_callback(hnd, kk_hnd, callback); \
  kk_box_drop(kk_hnd, _ctx); \
  kk_uv_status_code_callback(callback, status);

// Implementation of a oneshot request callback for the FS module.
// TODO: kk_hnd is just a unit box in practice, this is a bit confusing.
// Consider union types
#define kk_uv_oneshot_fs_callback(req) \
  kk_context_t* _ctx = kk_get_context(); \
  int status = req->result; \
  uv_fs_req_cleanup(req); \
  kk_uv_hnd_remove_callback(req, kk_hnd, callback); \
  kk_box_drop(kk_hnd, _ctx); \
  kk_free(req, _ctx); \
  kk_uv_status_code_callback(callback, status);

// Implementation of a oneshot request callback for the FS module with a single result,
// i.e. an error<t>
#define kk_uv_oneshot_fs_callback1(req, success_expr) \
  kk_context_t* _ctx = kk_get_context(); \
  int status = req->result; \
  kk_box_t successval; \
  if (status >= 0) { \
    successval = success_expr; \
  } \
  kk_uv_hnd_remove_callback(req, kk_hnd, callback); \
  uv_fs_req_cleanup(req); \
  kk_box_drop(kk_hnd, _ctx); \
  kk_free(req, _ctx); \
  if (status < 0) { \
    kk_uv_error_callback(callback, status); \
  } else { \
    kk_uv_okay_callback(callback, successval); \
  }

// An operation that stops / cancels an active
// operation on a stream. Removes the handle's `data` and returns status
// noop if data is already null
#define kk_uv_hnd_cancel_return(handle_t, handle, uv_fn) \
  handle_t* uvhnd = kk_owned_handle_to_uv_handle(handle_t, handle); \
  int status = uv_fn(uvhnd); \
  kk_uv_hnd_data_free(uvhnd); \
  return kk_uv_status_code(status);


// static inline kk_uv_buff_callback_t* kk_new_uv_buff_callback(kk_function_t cb, kk_bytes_t bytes, uv_handle_t* handle, kk_context_t* _ctx) {
//   kk_uv_buff_callback_t* c = kk_malloc(sizeof(kk_uv_buff_callback_t), _ctx);
//   c->callback = cb;
//   c->bytes = bytes;
//   handle->data = c;
//   return c;
// }

#define NULL_BOX kk_unit_box(kk_Unit)
#define NULL_BYTES kk_bytes_empty()


// Sets the data of the handle to point to the callback
static inline int kk_uv_hnd_data_create(kk_box_t internal, kk_function_t callback, kk_context_t* _ctx) {
  uv_handle_t* uvhnd = kk_cptr_unbox_borrowed(internal, _ctx);
  if (uvhnd->data != NULL) {
    return UV_EBUSY;
  }
  kk_hnd_callback_t* uvhnd_cb = kk_malloc(sizeof(kk_hnd_callback_t), kk_context());
  uvhnd_cb->callback = callback;
  uvhnd_cb->hnd = internal;
  uvhnd_cb->bytes = NULL_BYTES;
  // kk_warning_message("allocated kk_hnd @ %p, assigned to data field of handle %p (type %d)",
    // uvhnd_cb, uvhnd, uvhnd->type);
  uvhnd->data = uvhnd_cb;
  return UV_OK;
}

static inline kk_hnd_callback_t* kk_uv_hnd_callback_create(kk_box_t hnd, kk_function_t callback, kk_bytes_t bytes, kk_context_t* _ctx) {
  kk_hnd_callback_t* uvhnd_cb = kk_malloc(sizeof(kk_hnd_callback_t), kk_context());
  uvhnd_cb->callback = callback;
  uvhnd_cb->hnd = hnd;
  uvhnd_cb->bytes = bytes;
  return uvhnd_cb;
}

// take a copy of the cb and drop the rest of the kk_uv_hnd_callback resource
// Typically used after creating a callback, but the operation failed so now
// the callback is being manually invoked, unused by libuv.
static inline kk_function_t kk_uv_hnd_data_take_cb(uv_handle_t *uvhnd, kk_context_t* _ctx) {
  kk_assert(uvhnd->data != NULL);
  kk_hnd_callback_t* uvhnd_cb = (kk_hnd_callback_t*)uvhnd->data;
  kk_function_t cb = kk_function_dup(uvhnd_cb->callback, _ctx);
  kk_uv_hnd_callback_free(uvhnd_cb, _ctx);
  uvhnd->data = NULL;
  return cb;
}

#define kk_uv_hnd_wrapper_data_take_cb(wrapper) \
  kk_uv_hnd_data_take_cb(kk_cptr_unbox_borrowed(wrapper.internal, kk_context()), kk_context());

// Sets the data of the handle to point to the callback
#define kk_set_hnd_cb(hnd_t, handle, uvhnd, callback) \
  hnd_t* uvhnd = kk_owned_handle_to_uv_handle(hnd_t, handle); \
  kk_uv_hnd_data_free(uvhnd); \
  uvhnd->data = kk_uv_hnd_callback_create(handle.internal, callback, NULL_BYTES, _ctx);

// TODO is kk_hnd actually needed/used? The caller will need to free it
// consider kk_uv_hnd_data_take_cb instead
#define kk_uv_hnd_remove_callback(handle, kk_hnd, callback) \
  kk_hnd_callback_t* hndcb = (kk_hnd_callback_t*)handle->data; \
  handle->data = NULL; \
  kk_box_t kk_hnd = hndcb->hnd; \
  kk_function_t callback = hndcb->callback;

// TODO: Should I get the ctx from the loop?
// TODO: rename copy_callback or something to indicate reuse
#define kk_uv_hnd_get_callback(handle, kk_hnd, callback) \
  kk_context_t* _ctx = kk_get_context(); \
  kk_hnd_callback_t* hndcb = (kk_hnd_callback_t*)handle->data; \
  kk_box_t kk_hnd = hndcb->hnd; \
  kk_function_t callback = hndcb->callback;

#define kk_new_req_cb(req_t, req, cb) \
  req_t* req = kk_malloc(sizeof(req_t), kk_context()); \
  req->data = kk_function_as_ptr(cb, kk_context());

#define kk_new_req(req_t, req) \
  req_t* req = kk_malloc(sizeof(req_t), kk_context()); \
  req->data = NULL;

#define kk_uv_req_get_callback(req, cb) \
  kk_context_t* _ctx = kk_get_context(); \
  kk_function_t cb = kk_function_from_ptr(req->data, kk_context());




// TODO: Change all apis to return status code or return error, not a mix

// If the status is not OK, drop before returning the status code
#define kk_uv_check_status_drops(status, drops) \
  if (status < UV_OK) { \
    do drops while (0); \
  } \
  return kk_uv_utils_int_fs_status_code(status, kk_context()); \

// Check the uv status code and return a kk_std_core_exn__error Error if unsuccessful. Continue on success
#define kk_uv_check_bail(err) \
  if (err < UV_OK) { \
    return kk_async_error_from_errno(err, kk_context()); \
  }

// As above with drops
#define kk_uv_check_bail_drops(err, drops) \
  if (err < UV_OK) { \
    do drops while (0); \
    return kk_async_error_from_errno(err, kk_context()); \
  }

// Sometimes the return value is a file descriptor which is why this is a < UV_OK check instead of == UV_OK
#define kk_uv_check_return(err, result) \
  kk_uv_check_bail(err); \
  return kk_std_core_exn__new_Ok(result, kk_context());

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
