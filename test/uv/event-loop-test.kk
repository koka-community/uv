import std/test
import std/async/async
import uv/event-loop
import std/os/process

fun expect-contains(s: string, f: () -> io string)
  val result = f()
  expect(True, details = result ++ " contains " ++ s) { result.contains(s) }

fun main(): io-noexn ()
  with default-event-loop
  with async/handle
  run-tests(suite)

fun suite(): <async,test<<io,async>>> ()
  effectful-test("test")
    expect(1) {
      wait(0.1)
      1
    }

  // catches errors during event loop finalization
  effectful-test("successful process")
    // TODO: use async process once that's stable
    expect(0) {
      run-system("koka -e test/uv/embedded-event-loop.kk")
    }

  // TODO: this doesn't fail, even though the above exit code is reported as 1
  // effectful-test("successful process (output)")
  //   expect-contains("hello, world!")
  //     run-system-read("koka -e test/uv/embedded-event-loop.kk").untry
