// kk_uv_timer_t:
declare_uv_struct(uv_timer, kk_uv_free_fn);

kk_std_core_exn__error kk_uv_timer_init(kk_context_t* _ctx) {
  int status;
  malloc_and_init_handle(uv_timer, t, status, /* init */ uvloop(), &t->uv);
  if (status != UV_OK) {
    return kk_uv_error(status, _ctx);
  }
  return kk_std_core_exn__new_Ok(
    kk_uv_timer__timer_box(
      kk_uv_timer__new_Timer(kk_uv_timer_box(t, _ctx), _ctx), _ctx
    ), _ctx
  );
}

kk_uv_status_code_t kk_uv_timer_stop(kk_uv_timer__timer t, kk_context_t* _ctx) {
  kk_uv_timer_t* kk_timer = kk_uv_timer_unbox_borrowed(t.internal, _ctx);
  int status = uv_timer_stop(&kk_timer->uv);
  return kk_uv_status_code(status);
}

void kk_uv_timer_unit_callback(uv_timer_t* uv_timer) {
  kk_context_t* _ctx = kk_get_context();
  kk_uv_handle_t* hnd = uv_timer_as_kk_handle(uv_timer);
  kk_function_t callback;
  if (uv_timer_get_repeat(uv_timer) == 0) {
    callback = kk_uv_handle_take_callback(hnd);
  } else {
    callback = kk_uv_handle_dup_callback(hnd, _ctx);
  }
  kk_status_callback(callback, UV_OK, _ctx);
}

void kk_uv_timer_start(kk_uv_timer__timer t, int64_t timeout, int64_t repeat, kk_function_t callback, kk_context_t* _ctx) {
  kk_uv_timer_t* timer = kk_uv_timer_unbox_borrowed(t.internal, _ctx);
  kk_uv_handle_t* hnd = kk_uv_timer_as_handle(timer);
  kk_uv_handle_set_callback(hnd, callback, _ctx);
  int status = uv_timer_start(&timer->uv, kk_uv_timer_unit_callback, timeout, repeat);
  if (status != UV_OK) {
    callback = kk_uv_handle_take_callback(hnd);
    kk_status_callback(callback, status, _ctx);
  }
}
