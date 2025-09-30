
#if __EMSCRIPTEN__

#define kk_unit_callback(callback) \
  kk_function_call(kk_unit_t, (kk_function_t, kk_context_t*), callback, (callback, kk_context()), kk_context());

void kk_handle_free(void *p, kk_block_t *block, kk_context_t *_ctx) {
    kk_wasm_timer_t* hndcb = (kk_wasm_timer_t*)p;
    kk_function_drop(hndcb->callback, kk_context()); // Drop the callback
    kk_free(hndcb, kk_context()); // Free the memory used for the callback and box
    // p will be freed by uv.
}

#define kk_tm_to_uv(hnd) kk_owned_handle_to_uv_handle(kk_wasm_timer_t, hnd)
#define kk_uv_to_tm(hnd) handle_to_owned_kk_handle(hnd, kk_handle_free, timer, Timer)
#define kk_uv_to_tm_box(hnd) handle_to_owned_kk_handle_box(hnd, kk_handle_free, timer, timer, Timer)

EMSCRIPTEN_KEEPALIVE void wasm_timer_callback(kk_wasm_timer_t* timer_info){
  kk_context_t* _ctx = kk_get_context();
  kk_function_t callback = timer_info->callback;
  kk_box_t kkhandle = timer_info->kkhandle;
  if (timer_info->repeat_ms == 0) {
    kk_unit_callback(callback)
  } else {
    kk_function_dup(callback, _ctx);
    kk_box_dup(kkhandle, _ctx);
    kk_unit_callback(callback)
  }
}

EM_JS(int, start_timer, (int64_t timeout, int64_t repeat, kk_wasm_timer_t* timer_info), {
  function wasm_callback() {
    _wasm_timer_callback(timer_info);
  }
  const rp = Number(repeat);
  const msx = Number(timeout);
  if (rp != 0) {
    return setInterval(wasm_callback, rp);
  } else {
    return setTimeout(wasm_callback, msx);
  }
});

EM_JS(void, stop_timer, (int timer, bool repeating), {
  if (timer) {
    if (repeating) {
      clearInterval(timer);
    } else {
      clearTimeout(timer);
    }
  }
});

kk_uv_timer__timer kk_wasm_timer_init(kk_context_t* _ctx) {
  kk_wasm_timer_t* timer_info = kk_malloc(sizeof(kk_wasm_timer_t), _ctx);
  kk_uv_timer__timer hnd = kk_uv_to_tm(timer_info);
  timer_info->kkhandle = hnd.internal;
  return hnd;
}

kk_unit_t kk_wasm_timer_stop(kk_uv_timer__timer timer, kk_context_t* _ctx) {
  kk_wasm_timer_t* timer_info = kk_tm_to_uv(timer);
  if (timer_info == nullptr) return kk_Unit;
  if (timer_info->timer != 0) {
    stop_timer(timer_info->timer, timer_info->repeat_ms != 0);
  }
  kk_uv_timer__timer_drop(timer, _ctx);
  return kk_Unit;
}

kk_std_core_exn__error kk_wasm_timer_start(kk_uv_timer__timer timer, int64_t timeout, int64_t repeat, kk_function_t callback, kk_context_t* _ctx) {
  kk_wasm_timer_t* timer_info = kk_tm_to_uv(timer);
  timer_info->callback = callback;
  timer_info->repeat_ms = repeat;
  int ret = start_timer(timeout, repeat, timer_info);
  if (ret < 0) {
    kk_function_drop(callback, _ctx);
    kk_define_string_literal(, err_msg, 22, "Failed to start timer", _ctx);
    return kk_std_core_exn__new_Error(kk_std_core_exn__new_Exception( err_msg, kk_std_core_exn__new_ExnSystem(kk_reuse_null, 0, kk_integer_from_int(-1,_ctx), _ctx), _ctx), _ctx );
  } else {
    timer_info->timer = ret;
    return kk_std_core_exn__new_Ok(kk_unit_box(kk_Unit), _ctx);
  }
}

#else


// #define kk_tm_to_uv(hnd) kk_owned_handle_to_uv_handle(uv_timer_t, hnd)
// #define kk_uv_to_tm(hnd) handle_to_owned_kk_handle(hnd, kk_free_fun, timer, Timer)
// #define kk_uv_to_tm_box(hnd) handle_to_owned_kk_handle_box(hnd, kk_free_fun, timer, timer, Timer)

void kk_uv_timer_unit_callback(uv_timer_t* uv_timer) {
  kk_resolve_uv_callback((uv_handle_t*)uv_timer, NULL, kk_get_context(););
}

kk_std_core_exn__error kk_uv_timer_set_timeout(int64_t timeout, kk_function_t callback, kk_context_t* _ctx) {
  KK_ALLOC_INIT_BOX_HANDLE(timer);
  kk_assign_uv_callback(timer, callback, _ctx);
  int status = uv_timer_start(timer_uvhnd, kk_uv_timer_unit_callback, timeout, 0 /* no repeat */);

  // return the timer to the caller, so they can cancel it
  kk_box_t boxed_timer = kk_uv_timer__timer_box(kk_uv_timer__new_Timer(timer, _ctx), _ctx);
  kk_uv_check_return_err_drops(status, boxed_timer, {kk_box_drop(boxed_timer, _ctx);})
}

kk_std_core_exn__error kk_uv_timer_clear_timeout(kk_uv_timer__timer timer, kk_context_t* _ctx) {
  uv_timer_t* t = (uv_timer_t*)kk_uv_get_raw_handle(timer.internal, _ctx);
  int status = uv_timer_stop(t);
  kk_uv_timer__timer_drop(timer, _ctx);
  kk_uv_check(status);
}

// kk_uv_timer__timer kk_uv_timer_init(kk_context_t* _ctx) {
//   kk_box_t timer = kk_alloc_uv_handle(_ctx);
//   uv_timer_t* t = kk_uv_get_raw_handle(timer, _ctx);
//   uv_timer_init(uvloop(), t);
//   return kk_uv_timer__new_Timer(timer, _ctx);
// }

// kk_unit_t kk_uv_timer_stop(kk_uv_timer__timer timer, kk_context_t* _ctx) {
//   uv_timer_t* t = kk_uv_get_raw_handle(timer->internal);
//   uv_timer_stop(uv_timer);
//   return kk_Unit;
// }

// kk_std_core_exn__error kk_uv_timer_start(kk_uv_timer__timer timer, int64_t timeout, int64_t repeat, kk_function_t callback, kk_context_t* _ctx) {
//   kk_set_hnd_cb(uv_timer_t, timer, uv_timer, callback)
//   int status = uv_timer_start(uv_timer, kk_uv_timer_unit_callback, timeout, repeat);
//   kk_uv_check_err_drops(status, {kk_function_drop(callback, _ctx);})
// }

// kk_std_core_exn__error kk_uv_timer_again(kk_uv_timer__timer timer, kk_context_t* _ctx) {
//   int status = uv_timer_again(kk_tm_to_uv(timer));
//   kk_uv_check(status)
// }

// kk_unit_t kk_uv_timer_set_repeat(kk_uv_timer__timer timer, int64_t repeat, kk_context_t* _ctx) {
//   uv_timer_set_repeat(kk_tm_to_uv(timer), repeat);
//   return kk_Unit;
// }

// int64_t kk_uv_timer_get_repeat(kk_uv_timer__timer timer, kk_context_t* _ctx) {
//   uint64_t repeat = uv_timer_get_repeat(kk_tm_to_uv(timer));
//   return repeat;
// }

// int64_t kk_uv_timer_get_due_in(kk_uv_timer__timer timer, kk_context_t* _ctx) {
//   uint64_t due_in = uv_timer_get_due_in(kk_tm_to_uv(timer));
//   return due_in;
// }
#endif


// kk_uv_timer__timer kk_timer_init(kk_context_t* _ctx) {
//   #ifdef __EMSCRIPTEN__
//     return kk_wasm_timer_init(_ctx);
//   #else
//     return kk_uv_timer_init(_ctx);
//   #endif
// }

// kk_unit_t kk_timer_stop(kk_uv_timer__timer timer, kk_context_t* _ctx) {
//   #ifdef __EMSCRIPTEN__
//     return kk_wasm_timer_stop(timer, _ctx);
//   #else
//     return kk_uv_timer_stop(timer, _ctx);
//   #endif
// }

// kk_std_core_exn__error kk_timer_start(kk_uv_timer__timer timer, int64_t timeout, int64_t repeat, kk_function_t callback, kk_context_t* _ctx) {
//   #ifdef __EMSCRIPTEN__
//     return kk_wasm_timer_start(timer, timeout, repeat, callback, _ctx);
//   #else
//     return kk_uv_timer_start(timer, timeout, repeat, callback, _ctx);
//   #endif
// }

// kk_std_core_exn__error kk_timer_again(kk_uv_timer__timer timer, kk_context_t* _ctx) {
//   #ifdef __EMSCRIPTEN__
//     return kk_std_core_exn__new_Ok(kk_unit_box(kk_Unit), kk_context());
//   #else
//     return kk_uv_timer_again(timer, _ctx);
//   #endif
// }

// kk_unit_t kk_timer_set_repeat(kk_uv_timer__timer timer, int64_t repeat, kk_context_t* _ctx) {
//   #ifdef __EMSCRIPTEN__
//     return kk_Unit;
//   #else
//     return kk_uv_timer_set_repeat(timer, repeat, _ctx);
//   #endif
// }

// int64_t kk_timer_get_repeat(kk_uv_timer__timer timer, kk_context_t* _ctx) {
//   #ifdef __EMSCRIPTEN__
//     return -1;
//   #else
//     return kk_uv_timer_get_repeat(timer, _ctx);
//   #endif
// }

// int64_t kk_timer_get_due_in(kk_uv_timer__timer timer, kk_context_t* _ctx) {
//   #ifdef __EMSCRIPTEN__
//     return -1;
//   #else
//     return kk_uv_timer_get_due_in(timer, _ctx);
//   #endif
// }
