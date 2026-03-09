module std/stream

pub import uv/stream
import std/async/async
import std/core/bytes

pub alias io-stream = uv-stream

pub fun read-all(^stream): <io,async> bytes
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


pub fun write(^stream: io-stream, data: string): <asyncx> ()
  await1(fn(cb) stream.stream/write([data.bytes].vector, cb)).untry
