module std/stream

import uv/stream
import uv/event-loop
import std/async/async

pub alias io-stream = uv-stream

pub fun read-all(stream): <asyncx> string
  "TODO: read-all implementation"

pub fun close(stream: io-stream): asyncx ()
  await0(fn(cb) stream.uv-handle.event-loop/close(cb))
