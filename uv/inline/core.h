#include <uv.h>

// TODO move reusable function definitions into .c file and remove `static`

#define UV_OK 0

#define kk_uv_status_code_t kk_uv_status_dash_code__uv_status_code
#define kk_uv_status_code(i) kk_uv_status_dash_code_int_fs_status_code(i, kk_context())

typedef struct kk_uv_flags_s {
  char bits;
  // HANDLE, CALLBACK, BYTES
} kk_uv_flags_t;

#define NULL_FLAGS = 0
#define BOX_BIT 1
#define CALLBACK_BIT (1 << 1)
#define BYTES_BIT (1 << 2)
#define REFCOUNTED_BIT (1 << 3)

static inline char has_box(kk_uv_flags_t flags) {
  return flags.bits & BOX_BIT;
}

static inline char has_callback(kk_uv_flags_t flags) {
  return flags.bits & CALLBACK_BIT;
}

static inline char has_bytes(kk_uv_flags_t flags) {
  return flags.bits & BYTES_BIT;
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

// Every UV type we use has a corresponding wrapper, which embeds the UV structure
// as well as a common preamble for storing references to callbacks, bytes, etc.
// The `flags` field describes which of these are set, since not all fields are needed in all cases.
//
// This macro also defines a conversion from the raw uv_type_t* to the kk_uv_type_t*
//
// handle structs have a reference to themselves in the `box` field, which is responsible for eventual deallocation
// request structs have the same layout, however `box` (if set) does not refer to the request,
// the request is single-use and not reference counted
#define declare_uv_struct_base(uv_type) \
  typedef struct kk_##uv_type##_s { \
    kk_uv_flags_t flags; \
    kk_box_t box; \
    kk_function_t callback; \
    kk_bytes_t bytes; \
    uv_type##_t uv; \
  } kk_##uv_type##_t; \
  __attribute__((unused)) \
  static kk_##uv_type##_t* uv_type##_as_kk(uv_type##_t* p) { \
    return (kk_##uv_type##_t *) (((char*)p) - offsetof(kk_##uv_type##_t, uv)); \
  } \
  __attribute__((unused)) \
  __attribute__((unused)) \
  static kk_##uv_type##_t* kk_##uv_type##_unbox_borrowed(kk_box_t box, kk_context_t* _ctx) { \
    return ((kk_##uv_type##_t*) kk_cptr_unbox_borrowed(box, _ctx)); \
  }


// kk_uv_handle_t
declare_uv_struct_base(uv_handle);

#define declare_uv_struct(uv_type, free_fn) \
  declare_uv_struct_base(uv_type); \
  __attribute__((unused)) \
  static kk_uv_handle_t* kk_##uv_type##_as_handle(kk_##uv_type##_t* p) { \
    return (kk_uv_handle_t*)p; \
  } \
  static kk_uv_handle_t* uv_type##_as_kk_handle(uv_type##_t* p) { \
    return kk_##uv_type##_as_handle(uv_type##_as_kk(p)); \
  } \
  __attribute__((unused)) \
  static kk_box_t kk_##uv_type##_box(kk_##uv_type##_t* p, kk_context_t* _ctx) { \
    return kk_cptr_raw_box(&free_fn, (void*)p, _ctx); \
  }

// allocation patterns:
// raw: allocates but doesn't set flags
#define malloc_and_init_raw(uv_type, hnd, status, ...) \
  kk_##uv_type##_t* hnd = kk_malloc(sizeof(kk_##uv_type##_t), _ctx); \
  status = uv_type##_init(__VA_ARGS__); \
  if (status != UV_OK) { \
    kk_free(hnd, _ctx); \
  }

// handle: sets refcounted & box flags
#define malloc_and_init_handle(uv_type, hnd, status, ...) \
  malloc_and_init_raw(uv_type, hnd, status, __VA_ARGS__); \
  if (status == UV_OK) { \
    hnd->flags.bits = BOX_BIT & REFCOUNTED_BIT; \
    hnd->box = kk_##uv_type##_box(hnd, _ctx); \
  }


__attribute__((unused))
static kk_function_t kk_uv_handle_take_callback(kk_uv_handle_t* hnd) {
  kk_assert(has_callback(hnd->flags));
  clear_flag(hnd->flags, CALLBACK);
  return hnd->callback;
}

__attribute__((unused))
static kk_function_t kk_uv_handle_dup_callback(kk_uv_handle_t* hnd, kk_context_t* _ctx) {
  kk_assert(has_callback(hnd->flags));
  return kk_function_dup(hnd->callback, _ctx);
}

__attribute__((unused))
static void kk_uv_handle_set_callback(kk_uv_handle_t* hnd, kk_function_t callback, kk_context_t* _ctx) {
  kk_assert(!has_callback(hnd->flags));
  set_flag(hnd->flags, CALLBACK);
  hnd->callback = callback;
}

__attribute__((unused))
static void kk_unit_callback(kk_function_t callback, kk_context_t* _ctx) {
  kk_function_call(kk_unit_t, (kk_function_t, kk_context_t*), callback, (callback, _ctx), _ctx);
}

__attribute__((unused))
static void kk_status_callback(kk_function_t callback, int status, kk_context_t* _ctx) {
  kk_function_call(kk_unit_t,
    (kk_function_t, kk_uv_status_code_t, kk_context_t*),
    callback,
    (callback, kk_uv_status_code(status), _ctx), _ctx);
}

// Drop the contents of a uv handle, in preparation for the handle itself
// to be freed
__attribute__((unused))
static void kk_uv_drop_contents(kk_uv_handle_t *hnd, kk_context_t *_ctx) {
  if (has_callback(hnd->flags)) {
    clear_flag(hnd->flags, CALLBACK);
    kk_function_drop(hnd->callback, _ctx);
  }

  if (has_bytes(hnd->flags)) {
    clear_flag(hnd->flags, BYTES);
    kk_bytes_drop(hnd->bytes, _ctx);
  }
  
  if (has_box(hnd->flags)) {
    if (!is_refcounted(hnd->flags)) {
      // only drop box if it's some related piece of state, not `self`
      clear_flag(hnd->flags, BOX);
      kk_box_drop(hnd->box, _ctx);
    }
  }
}

// after close, we're done with the handle and can free it completely
__attribute__((unused))
static void kk_uv_close_cb(uv_handle_t* uvhnd) {
  kk_context_t* _ctx = kk_get_context();
  kk_uv_handle_t* kk_hnd = uv_handle_as_kk(uvhnd);
  kk_uv_drop_contents(kk_hnd, _ctx);
  kk_free(kk_hnd, _ctx);
}

// // drop a refcounted type, free (close) a nonrefcounted type
// static void kk_uv_drop(kk_uv_handle_t *hnd, kk_context_t *_ctx) {
//   if (is_refcounted(hnd->flags)) {
//     // if it's refcounted, just deref
//     kk_assert(has_box(hnd->flags));
//     clear_flag(hnd->flags, BOX);
//     kk_box_drop(hnd->box, _ctx);
//   } else {
//     // non-refcounted, close immediately
//     uv_close(&(hnd->uv), kk_uv_close_cb);
//   }
// }

// freeing a uv handle just closes it, the deallocation doesn't
// occur until the close callback is triggered
__attribute__((unused))
static void kk_uv_free_fn(void *p, kk_block_t *block, kk_context_t *_ctx) {
  kk_uv_handle_t* hnd = (kk_uv_handle_t*)p;
  uv_close(&hnd->uv, kk_uv_close_cb);
}

// event loop
static kk_decl_thread uv_loop_t* kk_uv_loop_default;

void kk_set_uv_loop(uv_loop_t* loop);
uv_loop_t* uvloop();
