/*---------------------------------------------------------------------------
  Copyright 2023 Tim Whiting.

  This is free software; you can redistribute it and/or modify it under the
  terms of the Apache License, Version 2.0. A copy of the License can be
  found in the LICENSE file at the root of this distribution.
---------------------------------------------------------------------------*/

module uv/stream

import uv/handle
pub import std/core/bytes
pub import uv/handle
import uv/buffer

extern import
  c file "inline/stream"

pub value struct uv-stream { internal : any }

// Casts the stream to the underlying uv handle type
pub fun uv-handle(s: uv-stream): uv-handle
  Uv-handle(s.internal)

// Identity implicit
pub fun uv-stream(s: uv-stream): uv-stream
  s

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
pub extern write(^stream: uv-stream, data: vector<bytes>, callback: (uv-status-code) -> io-noexn ()): io-noexn ()
  c "kk_uv_write"

// Start listening for incoming connections.
// Backlog indicates the number of connections the kernel might queue, same as listen(2).
// When a new incoming connection is received the uv_connection_cb callback is called.
pub extern listen(^stream: uv-stream, backlog: int32, callback: (uv-status-code) -> io-noexn ()): io-noexn ()
  c "kk_uv_listen"

// This call is used in conjunction with `listen()` to accept incoming connections.
// Call this function after receiving a connection callback to accept the connection.
// Before calling this function the client handle must be initialized.
// When the `listen()` callback is called it is guaranteed that this function will complete successfully the first time.
// If you attempt to use it more than once, it may fail.
// It is suggested to only call this function once per connection callback.
pub extern accept(^server: uv-stream, ^client: uv-stream): io-noexn uv-status-code
  c "kk_uv_accept"

// Shutdown the outgoing (write) side of a duplex stream.
// It waits for pending write requests to complete.
// The stream handle should refer to a initialized stream.
// The `callback` is called after shutdown is complete.
pub extern shutdown(^hnd: uv-stream, callback: (uv-status-code) -> io-noexn ()): io-noexn ()
  c "kk_uv_shutdown"
