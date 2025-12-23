
// allocation interop
static inline void* kk_malloc_ctx(size_t size) {
  return kk_malloc(size, kk_get_context());
}

static inline void* kk_realloc_ctx(void* p, size_t size) {
  return kk_realloc(p, size, kk_get_context());
}

static inline void* kk_calloc_ctx(size_t count, size_t size) {
  void* p = kk_malloc(count*size, kk_get_context());
  kk_memset(p, 0, count*size);
  return p;
}

static inline void kk_free_ctx(void* p) {
  kk_free(p, kk_get_context());
}

static inline void kk_uv_alloc_init(kk_context_t* _ctx){
  uv_replace_allocator(kk_malloc_ctx, kk_realloc_ctx, kk_calloc_ctx, kk_free_ctx);
}

// Event loop

void kk_set_uv_loop(uv_loop_t* loop) {
  kk_uv_loop_default = loop;
}

uv_loop_t* uvloop() {
  return kk_uv_loop_default;
}

static void kk_uv_loop_init(kk_context_t* _ctx) {
  uv_loop_t* loop = kk_malloc(sizeof(uv_loop_t), kk_context());
  kk_set_uv_loop(loop);
  uv_loop_init(loop);
  loop->data = kk_context();
}

kk_uv_status_code_t kk_uv_loop_run(kk_context_t* _ctx){
  // Run the event loop after the initial startup of the program
  return kk_uv_status_code(uv_run(uvloop(), UV_RUN_DEFAULT), _ctx);
}

static kk_uv_status_code_t kk_uv_loop_close(kk_context_t* _ctx) {
  kk_warning_message("[kk_uv_loop_close]\n");
  int ret = uv_loop_close(uvloop());
  kk_free(uvloop(), _ctx);
  return kk_uv_status_code(ret, _ctx);
}

// general handler utilities

static kk_uv_status_code_t kk_uv_handle_close(kk_uv_core__uv_handle handle, kk_context_t* _ctx) {
  kk_uv_handle_t* uv_hnd = kk_uv_handle_unbox_borrowed(handle.internal, _ctx);
  // kk_warning_message("has_box=%d\n", has_box(uv_hnd->flags));
  // kk_warning_message("before dropping box, current refcount is %d\n",
  //   kk_block_refcount(kk_box_to_ptr(handle.internal, kk_context()))
  // );

  // drop an internal handle which may be shared with pending operations
  kk_uv_handle_drop_references(uv_hnd, _ctx);

  kk_warning_message("[kk_uv_handle_close] current refcount of handle type %d is %d (expected 0)\n",
    uv_hnd->uv.type,
    kk_block_refcount(kk_box_to_ptr(handle.internal, kk_context()))
  );

  // handles are closed on drop. If `handle` is not the last unique reference,
  // return EBUSY to indicate a programmer error
  if (kk_block_is_unique(kk_box_to_ptr(handle.internal, kk_context()))) {
    kk_warning_message("Closing handle of type %d\n", uv_hnd->uv.type);
    return kk_uv_status_code(UV_OK, _ctx);
  } else {
    return kk_uv_status_code(UV_EBUSY, _ctx);
  }
}


// static void kk_uv_handle_close_callback(uv_handle_t* handle){
//   kk_uv_hnd_get_callback(handle, kk_hnd, callback)
//   handle->data = NULL;
//   kk_unit_callback(callback)
//   kk_box_drop(kk_hnd, kk_context());
// }
