#include <uv.h>

// TODO move reusable function definitions into .c file and remove `static`

// ------------------------------
// Helpful aliases
// ------------------------------

#define UV_OK 0
#define kk_uv_status_code_t kk_uv_status_dash_code__uv_status_code
#define kk_uv_status_code(i, _ctx) kk_uv_status_dash_code_int_fs_status_code(i, _ctx)

// ------------------------------
// Event loop
// ------------------------------
static kk_decl_thread uv_loop_t* kk_uv_loop_default;
void kk_set_uv_loop(uv_loop_t* loop);
uv_loop_t* uvloop();

// ------------------------------
// Handle flags
// ------------------------------
typedef struct kk_uv_flags_s {
  char bits;
  // HANDLE, CALLBACK etc
} kk_uv_flags_t;

#define NULL_FLAGS = 0
#define BOX_BIT 1 // TODO move box into req->data?
#define CALLBACK_BIT (1 << 1)
// TODO: no need for this? All handles are refcounted
#define REFCOUNTED_BIT (1 << 2)

static inline char has_box(kk_uv_flags_t flags) {
  return flags.bits & BOX_BIT;
}

static inline char has_callback(kk_uv_flags_t flags) {
  return flags.bits & CALLBACK_BIT;
}

// whether `box` points to the koka refcounted version of `self`
// (if unset, `box` may still be populated to some related structure)
static inline char is_refcounted(kk_uv_flags_t flags) {
  return flags.bits & REFCOUNTED_BIT;
}

#define init_flags(flags) \
  (flags).bits = 0

#define set_flag(flags, flag) \
  (flags).bits = (flags).bits | flag##_BIT

#define clear_flag(flags, flag) \
  (flags).bits = (flags).bits & ~(flag##_BIT)

// ------------------------------
// Handle datastructures
// ------------------------------

// Every UV type we use has a corresponding wrapper, which embeds the UV structure
// as well as a common preamble for storing references to callbacks, etc.
// The `flags` field describes which of these are set, since not all fields are needed in all cases.
//
// This macro also defines typed conversions for:
// - getting a pointer to the kk wrapper given a raw uv pointer (_as_kk)
// - getting a pointer from a boxed version (_unbox_borrowed)
//
// handle structs have a reference to themselves in the `box` field, which is responsible for eventual deallocation
// request structs have the same layout, however `box` (if set) does not refer to the request,
// the request is single-use and not reference counted
// TODO: do we really need box? We could probably take it out of request types, but then we couldn't
// have the same function work on handles/requests. Maybe that's fine though?
#define declare_uv_handle_base(uv_type) \
  typedef struct kk_##uv_type##_s { \
    kk_uv_flags_t flags; \
    kk_function_t callback; \
    kk_box_t box; \
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
  kk_uv_flags_t flags;
  kk_function_t callback;
} kk_uv_any_t;

// A request is similar to a handle, but don't have a `box` self-reference.
// Requests don't participate in reference counting, they are freed when the operation
// completes or is canceled.
// TODO: is `flags` needed? I think we can use kk_function_null instead
#define declare_uv_req_base(uv_type) \
  typedef struct kk_##uv_type##_s { \
    kk_uv_flags_t flags; \
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
  }

// kk_uv_handle_t
declare_uv_handle_base(uv_handle);

// kk_uv_req_t
declare_uv_req_base(uv_req);

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
  static kk_box_t kk_##uv_type##_box(kk_##uv_type##_t* p, kk_context_t* _ctx) { \
    return kk_cptr_raw_box(&kk_uv_handle_free_fn, (void*)p, _ctx); \
  }

// Builds on declare_uv_struct_base with additional utilities:
// - casting a kk_uv_<type> to a generic kk_uv_req_t (_as_req)
// - casting a raw UV pointer to a generic kk_uv_req_t (_as_kk_req)
#define declare_uv_req(uv_type) \
  declare_uv_req_base(uv_type); \
  __attribute__((unused)) \
  static kk_uv_req_t* kk_##uv_type##_as_req(kk_##uv_type##_t* p) { \
    return (kk_uv_req_t*)p; \
  } \
  __attribute__((unused)) \
  static kk_uv_req_t* uv_type##_as_kk_req(uv_type##_t* p) { \
    return kk_##uv_type##_as_req(uv_type##_as_kk(p)); \
  }

// ------------------------------
// Handle creation / initialization
// ------------------------------

// Raw allocation logic shared by requests / handles
#define malloc_and_init_raw(uv_type, hnd, status, ...) \
  kk_##uv_type##_t* hnd = kk_malloc(sizeof(kk_##uv_type##_t), _ctx); \
  status = uv_type##_init(__VA_ARGS__); \
  kk_warning_message("init (%s_init) returned %x (type=%x)\n", #uv_type, status, hnd->uv.type); \
  if (status != UV_OK) { \
    kk_free(hnd, _ctx); \
  }

// Create a new long-lived handle, which stores a
// reference (kk_box_t) to itself.
// Used when the result will be returned to koka code.
#define malloc_and_init_handle(uv_type, hnd, status, ...) \
  malloc_and_init_raw(uv_type, hnd, status, __VA_ARGS__); \
  if (status == UV_OK) { \
    hnd->flags.bits = BOX_BIT & REFCOUNTED_BIT; \
    hnd->box = kk_##uv_type##_box(hnd, _ctx); \
  }

// Requests require no initialization on the uv side.
#define malloc_req(uv_type, hnd, cb, ...) \
  kk_##uv_type##_t* hnd = kk_malloc(sizeof(kk_##uv_type##_t), _ctx); \
  hnd->flags.bits = CALLBACK_BIT; \
  hnd->callback = cb;

// ------------------------------
// Starting operations
// ------------------------------

// Initiate a UV operation which will invoke a callback.
// On error the callback is taken out of the handle and the
// handle is dropped, `on_error` should invoke `callback`.
#define kk_uv_setup_callback(uv_t, hnd, status, cb, call_uv_expr, on_error, drops) \
  status = call_uv_expr; \
  if (status != UV_OK) { \
    kk_warning_message("kk_uv_setup_callback status=%d\n", status); \
    kk_uv_handle_t* raw_hnd = kk_##uv_t##_as_handle(hnd); \
    cb = kk_uv_any_take_callback(kk_uv_handle_as_any(raw_hnd)); \
    do drops while(0); \
    kk_uv_handle_drop(raw_hnd, _ctx); \
    do on_error while(0); \
    return; \
  } else { \
    do drops while(0); \
  }

// ------------------------------
// Handle mutation
// ------------------------------

// Remove the callback from the handle and return it
__attribute__((unused))
static kk_function_t kk_uv_any_take_callback(kk_uv_any_t* hnd) {
  kk_assert(has_callback(hnd->flags));
  clear_flag(hnd->flags, CALLBACK);
  return hnd->callback;
}
#define kk_uv_handle_take_callback(hnd) kk_uv_any_take_callback(kk_uv_handle_as_any(hnd))
#define kk_uv_req_take_callback(hnd) kk_uv_any_take_callback(kk_uv_req_as_any(hnd))

// Return a copy of the handle's callback
__attribute__((unused))
static kk_function_t kk_uv_any_dup_callback(kk_uv_any_t* hnd, kk_context_t* _ctx) {
  kk_assert(has_callback(hnd->flags));
  return kk_function_dup(hnd->callback, _ctx);
}
#define kk_uv_handle_dup_callback(hnd, _ctx) kk_uv_any_dup_callback(kk_uv_handle_as_any(hnd), _ctx)
#define kk_uv_req_dup_callback(hnd, _ctx) kk_uv_any_dup_callback(kk_uv_req_as_any(hnd), _ctx)

// Set the handle's callback. Aborts if handle is already set
__attribute__((unused))
static void kk_uv_any_set_callback(kk_uv_any_t* hnd, kk_function_t callback, kk_context_t* _ctx) {
  kk_assert(!has_callback(hnd->flags));
  set_flag(hnd->flags, CALLBACK);
  hnd->callback = callback;
}

#define kk_uv_handle_set_callback(hnd, cb, _ctx) kk_uv_any_set_callback(kk_uv_handle_as_any(hnd), cb, _ctx)
#define kk_uv_req_set_callback(hnd, cb, _ctx) kk_uv_any_set_callback(kk_uv_req_as_any(hnd), cb, _ctx)

// Set the handle's callback. Returns EBUSY if already set.
__attribute__((unused))
static int kk_uv_any_try_set_callback(kk_uv_any_t* hnd, kk_function_t callback, kk_context_t* _ctx) {
  if (has_callback(hnd->flags)) {
    return UV_EBUSY;
  } else {
    kk_uv_any_set_callback(hnd, callback, _ctx);
    return UV_OK;
  }
}

#define kk_uv_handle_try_set_callback(hnd, cb, _ctx) kk_uv_any_try_set_callback(kk_uv_handle_as_any(hnd), cb, _ctx)
#define kk_uv_req_try_set_callback(hnd, cb, _ctx) kk_uv_any_try_set_callback(kk_uv_req_as_any(hnd), cb, _ctx)

__attribute__((unused))
static void kk_uv_handle_set_box(kk_uv_handle_t* hnd, kk_box_t box, kk_context_t* _ctx) {
  kk_assert(!is_refcounted(hnd->flags));
  if (has_box(hnd->flags)) {
    kk_box_drop(hnd->box, _ctx);
  } else {
    set_flag(hnd->flags, BOX);
  }
  hnd->box = box;
}

__attribute__((unused))
static kk_box_t kk_uv_handle_take_box(kk_uv_handle_t* hnd, kk_context_t* _ctx) {
  kk_assert(!is_refcounted(hnd->flags) && has_box(hnd->flags));
  clear_flag(hnd->flags, BOX);
  return hnd->box;
}

// ------------------------------
// Error / exception helpers
// ------------------------------

// return an error for the given status
__attribute__((unused))
static kk_std_core_exn__error kk_status_error(int status, kk_context_t* _ctx) {
  kk_uv_status_code_t code = kk_uv_status_code(status, _ctx);
  kk_string_t msg = kk_uv_status_dash_code_message(code, _ctx);
  return kk_std_core_exn__new_Error(
    kk_std_core_exn__new_Exception(
      msg,
      kk_uv_status_dash_code__new_AsyncExn(kk_reuse_null, 0, code, _ctx),
      _ctx),
    _ctx);
}

// return `Ok(ok_expr)` if the status is UV_OK, otherwise Error(...)
__attribute__((unused))
#define kk_status_error_or(status, ok_expr, _ctx) \
  ((status == UV_OK) ? kk_std_core_exn__new_Ok(ok_expr, _ctx) : kk_status_error(status, _ctx))


// ------------------------------
// Helpers for invoking callbacks
// with common argument types
// ------------------------------
__attribute__((unused))
static void kk_unit_callback(kk_function_t callback, kk_context_t* _ctx) {
  kk_function_call(kk_unit_t, (kk_function_t, kk_context_t*), callback, (callback, _ctx), _ctx);
}

__attribute__((unused))
static void kk_status_code_callback(kk_function_t callback, int status, kk_context_t* _ctx) {
  kk_function_call(kk_unit_t,
    (kk_function_t, kk_uv_status_code_t, kk_context_t*),
    callback,
    (callback, kk_uv_status_code(status, _ctx), _ctx), _ctx);
}
__attribute__((unused))
static void kk_uv_error_status_callback(kk_function_t callback, int status, kk_context_t* _ctx) {
  kk_function_call(kk_unit_t,
    (kk_function_t, kk_std_core_exn__error, kk_context_t*),
    callback, (callback, kk_status_error(status, _ctx), _ctx), _ctx);
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

// Drop the contents of a uv handle, in preparation for the handle itself
// to be freed. Only drops what is set based on hnd->flags.
__attribute__((unused))
static void kk_uv_handle_drop_contents(kk_uv_handle_t *hnd, kk_context_t *_ctx) {
  if (has_callback(hnd->flags)) {
    clear_flag(hnd->flags, CALLBACK);
    kk_function_drop(hnd->callback, _ctx);
  }

  if (has_box(hnd->flags)) {
    if (!is_refcounted(hnd->flags)) {
      // only drop box if it's some related piece of state, not `self`
      clear_flag(hnd->flags, BOX);
      kk_box_drop(hnd->box, _ctx);
    }
  }
}

// TODO is this valid? Or do we need a req-only version?
__attribute__((unused))
static void kk_uv_req_drop_contents(kk_uv_req_t *hnd, kk_context_t *_ctx) {
  kk_uv_handle_drop_contents((kk_uv_handle_t*)hnd, _ctx);
}

// free a request type (immediately)
__attribute__((unused))
static void kk_uv_req_drop(kk_uv_req_t *hnd, kk_context_t *_ctx) {
  kk_warning_message("Directly dropping uv request %p of type %x with flags %x\n",
    hnd, hnd->uv.type, hnd->flags.bits);
  kk_uv_req_drop_contents(hnd, _ctx);
  kk_warning_message("freed contents...\n");
  kk_free(hnd, _ctx);
  kk_warning_message("freed!\n");
}

// Callback invoked by uv once a handle is closed.
__attribute__((unused))
static void kk_uv_handle_close_cb(uv_handle_t* uvhnd) {
  kk_context_t* _ctx = kk_get_context();
  kk_uv_handle_t* kk_hnd = uv_handle_as_kk(uvhnd);
  kk_uv_handle_drop_contents(kk_hnd, _ctx);
  kk_free(kk_hnd, _ctx);
}

// drop a refcounted type, free (close) a nonrefcounted type
__attribute__((unused))
static void kk_uv_handle_drop(kk_uv_handle_t *hnd, kk_context_t *_ctx) {
  // Will invoke kk_uv_free_fn (or equivalent) if
  // this is the last reference.
  kk_assert(has_box(hnd->flags));
  clear_flag(hnd->flags, BOX);
  kk_warning_message("Dropping uv box %p of type %x with flags %x\n",
    hnd, hnd->uv.type, hnd->flags.bits);
  kk_box_drop(hnd->box, _ctx);
}

// Free function used for refcounted handles.
// Triggers a uv_close, the object will be freed only
// once that completes.
__attribute__((unused))
static void kk_uv_handle_free_fn(void *p, kk_block_t *block, kk_context_t *_ctx) {
  kk_uv_handle_t* hnd = (kk_uv_handle_t*)p;
  kk_warning_message("Closing uv handle of type %x with flags %x\n",
    hnd->uv.type, hnd->flags.bits);
  uv_close(&hnd->uv, kk_uv_handle_close_cb);
}
