#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>

void one_iter() {
  // Can do a render loop to the screen here, etc. (this is the tick..)
  // puts("one iteration");
  return;
}
void kk_emscripten_loop_run(kk_context_t* _ctx){
  emscripten_set_main_loop(one_iter, 0, true);
}
#else

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

static void kk_uv_handle_close_callback(uv_handle_t* handle){
  kk_uv_hnd_get_callback(handle, kk_hnd, callback)
  handle->data = NULL;
  kk_unit_callback(callback)
  kk_box_drop(kk_hnd, kk_context());
}

kk_box_t kk_set_timeout(kk_function_t cb, int64_t time, kk_context_t* _ctx) {
  kk_uv_timer__timer t = kk_uv_timer_timer_init(_ctx);
  kk_uv_timer_timer_start(t, time, 0, cb, _ctx);
  return kk_uv_timer__timer_box(t, _ctx);
}

kk_unit_t kk_clear_timeout(kk_box_t boxed_timer, kk_context_t* _ctx) {
  kk_uv_timer__timer timer = kk_uv_timer__timer_unbox(boxed_timer, KK_OWNED, _ctx);
  kk_uv_timer_timer_stop(timer, _ctx);
  return kk_Unit;
}


static inline void kk_uv_alloc_init(kk_context_t* _ctx){
  uv_replace_allocator(kk_malloc_ctx, kk_realloc_ctx, kk_calloc_ctx, kk_free_ctx);
}

static void kk_uv_loop_init(kk_context_t* _ctx) {
  uv_loop_t* loop = kk_malloc(sizeof(uv_loop_t), kk_context());
  kk_set_uv_loop(loop);
  uv_loop_init(loop);
  loop->data = kk_context();
}

void kk_uv_loop_run(kk_context_t* _ctx){
  // Run the event loop after the initial startup of the program
  int ret = uv_run(uvloop(), UV_RUN_DEFAULT);
  if (ret != 0){
    kk_warning_message("Event loop closed with status %s\n", uv_err_name(ret));
  }
}

static void kk_uv_loop_close(kk_context_t* _ctx) {
  int ret = uv_loop_close(uvloop());
  if (ret != 0) {
    kk_warning_message("Event loop closed %s\n", uv_err_name(ret));
  }
  kk_free(uvloop(), _ctx);
}

static void kk_uv_close(kk_uv_utils__uv_handle handle, kk_function_t callback, kk_context_t* _ctx) {
  kk_set_hnd_cb(uv_handle_t, handle, uvhnd, callback)
  return uv_close(uvhnd, kk_uv_handle_close_callback);
}

#endif
// UV allocator helpers, getting thread local context


static inline void kk_async_alloc_init(kk_context_t* _ctx){
  #if __EMSCRIPTEN__
    return kk_Unit;
  #else 
    return kk_uv_alloc_init(_ctx);
  #endif
}

static void kk_async_loop_init(kk_context_t* _ctx) {
  #if __EMSCRIPTEN__
    return kk_Unit;
  #else 
    return kk_uv_loop_init(_ctx);
  #endif
}

void kk_async_loop_run(kk_context_t* _ctx){
  // Run the event loop after the initial startup of the program
  #if __EMSCRIPTEN__
    return kk_emscripten_loop_run(_ctx);
  #else 
    return kk_uv_loop_run(_ctx);
  #endif
  
}

static void kk_async_loop_close(kk_context_t* _ctx) {
  #if __EMSCRIPTEN__
    return kk_Unit;
  #else 
    return kk_uv_loop_close(_ctx);
  #endif
}

static void kk_async_close(kk_uv_utils__uv_handle handle, kk_function_t callback, kk_context_t* _ctx) {
  #if __EMSCRIPTEN__
    return kk_Unit;
  #else
    return kk_uv_close(handle, callback, _ctx);
  #endif
}
