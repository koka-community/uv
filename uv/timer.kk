/*---------------------------------------------------------------------------
  Copyright 2023 Tim Whiting.

  This is free software; you can redistribute it and/or modify it under the
  terms of the Apache License, Version 2.0. A copy of the License can be
  found in the LICENSE file at the root of this distribution.
---------------------------------------------------------------------------*/
module uv/timer
import std/num/int64
import uv/utils

extern import
  c  file "inline/timer"
  cs file "inline/timer.cs"
  js file "inline/timer.js"

pub value struct timer (
  internal: any
)

// timeout is in milliseconds
pub fun timer-set-timeout(timeout: int64, cb: () -> io-noexn ()): io timer
  timer-set-timeout-c(timeout, cb).untry

extern timer-set-timeout-c(timeout: int64, cb: () -> io-noexn ()): io-noexn error<timer>
  c "kk_uv_timer_set_timeout"
  js inline "_set_timeout()"

pub fun timer-clear-timeout(t: timer): io ()
  timer-clear-timeout-c(t).untry

extern timer-clear-timeout-c(t: timer): io-noexn error<unit>
  c "kk_uv_timer_clear_timeout"
  js inline "_clear_timeout()"

pub fun uv-handle(t: timer): uv-handle
  Uv-handle(t.internal)
