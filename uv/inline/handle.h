#include <uv.h>

// TODO move reusable function definitions into .c file and remove `static`

// ------------------------------
// Helpful aliases
// ------------------------------

#define UV_OK 0
#define kk_uv_status_code_t kk_uv_utils__uv_status_code
#define kk_uv_status_code(i, _ctx) kk_uv_utils_int_fs_status_code(i, _ctx)

// ------------------------------
// Handle datastructures
// ------------------------------

// Every UV type we use has a corresponding wrapper, which embeds the UV structure
// as well as a common preamble for storing references to callbacks, etc.
//
// This macro also defines typed conversions for:
// - getting a pointer to the kk wrapper given a raw uv pointer (_as_kk)
// - getting a pointer from a boxed version (_unbox_borrowed)
#define declare_uv_handle_base(uv_type) \
  typedef struct kk_##uv_type##_s { \
    kk_function_t callback; \
    uv_type##_t uv; \
  } kk_##uv_type##_t; \
  __attribute__((unused)) \
  static kk_##uv_type##_t* uv_type##_as_kk(uv_type##_t* p) { \
    return (kk_##uv_type##_t *) (((char*)p) - offsetof(kk_##uv_type##_t, uv)); \
  } \
  __attribute__((unused)) \
  static kk_##uv_type##_t* kk_##uv_type##_unbox_borrowed(kk_box_t box, kk_context_t* _ctx) { \
    return ((kk_##uv_type##_t*) kk_cptr_unbox_borrowed(box, _ctx)); \
  } \
  __attribute__((unused)) \
  static kk_uv_any_t* kk_##uv_type##_as_any(kk_##uv_type##_t* p) { \
    return (kk_uv_any_t*) p; \
  }

// kk_uv requests and handles share a comon prefix of fields. Used for manipulating an
// attached callback
typedef struct kk_uv_any_s {
  kk_function_t callback;
} kk_uv_any_t;

static inline char has_callback(kk_uv_any_t* handle, kk_context_t* _ctx) {
  return !kk_function_is_null(handle->callback, _ctx);
}

// kk_uv_handle_t
declare_uv_handle_base(uv_handle);

// Builds on declare_uv_struct_base with additional utilities:
// - casting a kk_uv_<type> to a generic kk_uv_handle_t (_as_handle)
// - casting a raw UV pointer to a generic kk_uv_handle_t (_as_kk_handle)
// - boxing up a raw pointer (_box)
#define declare_uv_handle(uv_type) \
  declare_uv_handle_base(uv_type); \
  __attribute__((unused)) \
  static kk_uv_handle_t* kk_##uv_type##_as_handle(kk_##uv_type##_t* p) { \
    return (kk_uv_handle_t*)p; \
  } \
  __attribute__((unused)) \
  static kk_uv_handle_t* uv_type##_as_kk_handle(uv_type##_t* p) { \
    return kk_##uv_type##_as_handle(uv_type##_as_kk(p)); \
  } \
  __attribute__((unused)) \
  static kk_uv_any_t* uv_type##_as_kk_any(uv_type##_t* p) { \
    return kk_##uv_type##_as_any(uv_type##_as_kk(p)); \
  } \
  __attribute__((unused)) \
  static kk_box_t kk_##uv_type##_box(kk_##uv_type##_t* p, kk_context_t* _ctx) { \
    return kk_cptr_raw_box(&kk_uv_handle_free_fn, (void*)p, _ctx); \
  }

// A request is similar to a handle, but the `uv` struct isn't a uv_handle_t.
// There is no `_as_req` function, only `_as_any`.
// Requests don't participate in reference counting, they are freed when the operation
// completes or is canceled.
#define declare_uv_req(uv_type) \
  typedef struct kk_##uv_type##_s { \
    kk_function_t callback; \
    uv_type##_t uv; \
  } kk_##uv_type##_t; \
  __attribute__((unused)) \
  static kk_##uv_type##_t* uv_type##_as_kk(uv_type##_t* p) { \
    return (kk_##uv_type##_t *) (((char*)p) - offsetof(kk_##uv_type##_t, uv)); \
  } \
  __attribute__((unused)) \
  static kk_uv_any_t* kk_##uv_type##_as_any(kk_##uv_type##_t* p) { \
    return (kk_uv_any_t*) p; \
  } \
  __attribute__((unused)) \
  static kk_uv_any_t* uv_type##_as_kk_any(uv_type##_t* p) { \
    return kk_##uv_type##_as_any(uv_type##_as_kk(p)); \
  }

// ------------------------------
// Handle creation / initialization
// ------------------------------

// Raw allocation logic shared by requests / handles
// On error, the memory is freed and no action is needed from the caller.
#define malloc_and_init_raw(uv_type, hnd, status, ...) \
  kk_##uv_type##_t* hnd = kk_malloc(sizeof(kk_##uv_type##_t), _ctx); \
  status = uv_type##_init(__VA_ARGS__); \
  if (status != UV_OK) { \
    kk_free(hnd, _ctx); \
  }

// Create a new long-lived handle, which stores a
// reference (kk_box_t) to itself.
// Used when the result will be returned to koka code.
// On error, the handle is freed and no action is needed from the caller.
#define malloc_and_init_handle(uv_type, hnd, status, ...) \
  malloc_and_init_raw(uv_type, hnd, status, __VA_ARGS__); \
  hnd->callback = kk_function_null(_ctx);

// Requests require no initialization on the uv side.
#define malloc_req(uv_type, hnd, cb) \
  kk_##uv_type##_t* hnd = kk_malloc(sizeof(kk_##uv_type##_t), _ctx); \
  hnd->callback = cb;

// ------------------------------
// Handle mutation
// ------------------------------

// Remove the callback from the handle and return it
__attribute__((unused))
static kk_function_t kk_uv_any_take_callback(kk_uv_any_t* hnd, kk_context_t* _ctx) {
  kk_assert(has_callback(hnd, _ctx));
  kk_function_t cb = hnd->callback;
  hnd->callback = kk_function_null(_ctx);
  return cb;
}
#define kk_uv_handle_take_callback(hnd, _ctx) kk_uv_any_take_callback(kk_uv_handle_as_any(hnd), _ctx)

// Return a copy of the handle's callback
__attribute__((unused))
static kk_function_t kk_uv_any_dup_callback(kk_uv_any_t* hnd, kk_context_t* _ctx) {
  kk_assert(has_callback(hnd, _ctx));
  return kk_function_dup(hnd->callback, _ctx);
}
#define kk_uv_handle_dup_callback(hnd, _ctx) kk_uv_any_dup_callback(kk_uv_handle_as_any(hnd), _ctx)

// Set the handle's callback. Aborts if handle is already set
__attribute__((unused))
static void kk_uv_any_set_callback(kk_uv_any_t* hnd, kk_function_t callback, kk_context_t* _ctx) {
  kk_assert(!has_callback(hnd, _ctx));
  hnd->callback = callback;
}

#define kk_uv_handle_set_callback(hnd, cb, _ctx) kk_uv_any_set_callback(kk_uv_handle_as_any(hnd), cb, _ctx)

// Set the handle's callback. Returns EBUSY if already set.
__attribute__((unused))
static int kk_uv_any_try_set_callback(kk_uv_any_t* hnd, kk_function_t callback, kk_context_t* _ctx) {
  if (has_callback(hnd, _ctx)) {
    return UV_EBUSY;
  } else {
    kk_uv_any_set_callback(hnd, callback, _ctx);
    return UV_OK;
  }
}

#define kk_uv_handle_try_set_callback(hnd, cb, _ctx) kk_uv_any_try_set_callback(kk_uv_handle_as_any(hnd), cb, _ctx)

// ------------------------------
// Error / exception helpers
// ------------------------------

// return an error for the given status
__attribute__((unused))
static kk_std_core_exn__error kk_uv_error(int status, kk_context_t* _ctx) {
  kk_uv_status_code_t code = kk_uv_status_code(status, _ctx);
  kk_string_t msg = kk_uv_utils_message(code, _ctx);
  return kk_std_core_types__new_Error(
    kk_std_core_exn__exception_box(
      kk_std_core_exn__new_Exception(
        msg,
        kk_uv_utils__new_AsyncExn(kk_reuse_null, 0, code, _ctx),
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

// Drop the contents of a uv handle. Freeing won't actually occur
// until the last reference is dropped and the free_fn is invoked.
__attribute__((unused))
static void kk_uv_any_drop_references(kk_uv_any_t *hnd, kk_context_t *_ctx) {
  if (has_callback(hnd, _ctx)) {
    kk_function_drop(hnd->callback, _ctx);
    hnd->callback = kk_function_null(_ctx);
  }
}

#define kk_uv_handle_drop_references(hnd, _ctx) kk_uv_any_drop_references(kk_uv_handle_as_any(hnd), _ctx)

// free a request type (immediately)
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

// Callback invoked by uv once a handle is closed.
__attribute__((unused))
static void kk_uv_handle_close_cb(uv_handle_t* uvhnd) {
  kk_context_t* _ctx = kk_get_context();
  kk_uv_handle_t* kk_hnd = uv_handle_as_kk(uvhnd);
  // kk_warning_message("[kk_uv_handle_close_cb] CLOSED uv handle of type %d\n", uvhnd->type);
  kk_free(kk_hnd, _ctx);
}

// Free function used for refcounted handles.
// Triggers a uv_close, the object will be freed only
// once that completes.
__attribute__((unused))
static void kk_uv_handle_free_fn(void *p, kk_block_t *block, kk_context_t *_ctx) {
  kk_uv_handle_t* hnd = (kk_uv_handle_t*)p;
  uv_close(&hnd->uv, kk_uv_handle_close_cb);
}
