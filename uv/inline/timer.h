/*----------------------------------------------------------------------------
   Copyright 2024, Koka-Community Authors

   Licensed under the MIT License ("The License"). You may not
   use this file except in compliance with the License. A copy of the License
   can be found in the LICENSE file at the root of this distribution.
----------------------------------------------------------------------------*/

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

typedef struct kk_wasm_timer_s {
  kk_function_t callback;
  kk_box_t kkhandle;
  int64_t repeat_ms;
  int timer;
} kk_wasm_timer_t;

EMSCRIPTEN_KEEPALIVE void wasm_timer_callback(kk_wasm_timer_t* timer_info);
#else
#include <uv.h>
#endif
