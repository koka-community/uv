
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
  return kk_uv_status_code(uv_run(uvloop(), UV_RUN_DEFAULT));
}

static kk_uv_status_code_t kk_uv_loop_close(kk_context_t* _ctx) {
  int ret = uv_loop_close(uvloop());
  kk_free(uvloop(), _ctx);
  return kk_uv_status_code(ret);
}

// general handler utilities

// static void kk_uv_close(kk_uv_utils__uv_handle handle, kk_function_t callback, kk_context_t* _ctx) {
//   uv_handle_t* uvhnd_dbg = kk_owned_handle_to_uv_handle(uv_handle_t, handle);
//   // kk_warning_message("Closing handle of type %d with old_data=%p\n", uvhnd_dbg->type, uvhnd_dbg->data);
//   kk_set_hnd_cb(uv_handle_t, handle, uvhnd, callback);
//   uv_close(uvhnd, kk_uv_handle_close_callback);
// }


// static void kk_uv_handle_close_callback(uv_handle_t* handle){
//   kk_uv_hnd_get_callback(handle, kk_hnd, callback)
//   handle->data = NULL;
//   kk_unit_callback(callback)
//   kk_box_drop(kk_hnd, kk_context());
// }
