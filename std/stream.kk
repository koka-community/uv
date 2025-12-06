module std/stream

import uv/stream
import uv/event-loop
import std/async/async
import std/core/bytes

pub alias io-stream = uv-stream

pub fun read-all(stream): <io,async> bytes
  val result: ref<_, bytes> = ref(empty())
  with finally
    // error only indicates delayed cleanup, not failure
    val _: uv-status-code = stream.read-stop()
    ()

  await1 fn(complete)
    stream.read-start fn(chunk)
      match chunk
        Error(e) -> complete(Error(e))
        Ok(Nothing) -> complete(Ok(!result))
        Ok(Just(chunk)) -> result := !result ++ chunk
  .untry


pub fun write(stream: io-stream, data: string): <asyncx> ()
  await1(fn(cb) stream.stream/write([data.bytes], cb)).untry

pub fun close(stream: io-stream): asyncx ()
  await0(fn(cb) stream.uv-handle.event-loop/close(cb))
