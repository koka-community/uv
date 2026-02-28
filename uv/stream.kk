/*---------------------------------------------------------------------------
  Copyright 2023 Tim Whiting.

  This is free software; you can redistribute it and/or modify it under the
  terms of the Apache License, Version 2.0. A copy of the License can be
  found in the LICENSE file at the root of this distribution.
---------------------------------------------------------------------------*/

module uv/stream
pub import std/core/bytes
pub import uv/handle
import uv/buffer

extern import
  c file "inline/stream"

pub value struct uv-stream { internal : any };

// Casts the stream to the underlying uv handle type
pub fun uv-handle(s: uv-stream): uv-handle
  Uv-handle(s.internal)

// Read data from an incoming stream. The `callback` will be made several times until there is no more data to read or `read-stop()` is called.
// Returns `UV_EALREADY` when called twice, and `UV_EINVAL` when the stream is closing.
pub extern read-start(^stream: uv-stream, callback: (error<maybe<bytes>>) -> io-noexn ()): io-noexn ()
  c "kk_uv_read_start"

// Stop reading data from the stream. The read callback will no longer be called.
// This function is idempotent and may be safely called on a stopped stream.
// A non-zero return indicates that finishing releasing resources may be pending on the next input event on that TTY on Windows, and does not indicate failure.
pub extern read-stop(^stream: uv-stream): io-noexn uv-status-code
  c "kk_uv_read_stop"

// Write data to stream. Buffers are written in order.
pub extern stream/write(^stream: uv-stream, data: vector<bytes>, callback: (uv-status-code) -> io-noexn ()): io-noexn ()
  c "kk_uv_write"
