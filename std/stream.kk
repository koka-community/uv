module std/stream

pub import uv/stream
import uv/core
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
      println("READ CHUNK (error?): " ++ chunk.is-error.show)
      match chunk
        Error(e) -> complete(Error(e))
        Ok(Nothing) -> complete(Ok(!result))
        Ok(Just(chunk)) -> result := !result ++ chunk
  .untry


pub fun write(^stream: io-stream, data: string): <asyncx> ()
  await1(fn(cb)
    val cb2 = fn(result: uv-status-code)
      println("WRITE RESULT: " ++ result.show)
      cb(result)
    stream.stream/write([data.bytes].vector, cb2)
  ).untry
