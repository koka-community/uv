import std/test
import std/async/async
import std/async/async-file
import uv/event-loop
import std/core/bytes
import std/core/int

fun main(): io-noexn ()
  with default-event-loop
  with async/handle
  run-tests(suite)

fun suite(): <async,test<<io,async>>> ()
  val contents = "hello, world!"

  effectful-test("tempfile")
    val (path, file-contents) = tempfile(name="mytmp-") fn(f)
      f.write(contents.bytes)
      f.fsync()

      // read via the handle itself and also by reopening its path
      val from-tmp = f.read().string
      val from-new = open-read(f.path) fn(r) -> r.read().string
      (f.path, (from-tmp, from-new))

    expect((contents, contents)) { file-contents }
    expect(True) { path.contains("mytmp-") }
