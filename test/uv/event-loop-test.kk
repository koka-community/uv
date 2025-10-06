import std/test
import std/async/async
import uv/event-loop
import std/os/process
import std/num/int32
import std/num/int64
import uv/timer

fun error-message(e: error<_>)
  match e
    Error(e) -> Just(e.message)
    Ok(_) -> Nothing

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

  effectful-test("clear-timeout")
    val result: ref<global,_> = ref(Nothing)
    fun cb()
      result.set(Just("done"))
    val timeout = set-timeout(cb, 50.int32)
    clear-timeout(timeout)
    wait(0.1)
    expect(Nothing) { !result }

  effectful-test("close with non-unique reference")
    // pending timeout has its own reference
    val timeout: timer = timer-set-timeout(50.int64, { () })
    expect(Just("uv_close(): resource is still referenced")) {
      try { close(timeout.uv-handle) }.error-message
    }

  effectful-test("close with unique reference")
    val timeout = timer-set-timeout(500.int64, { () })
    timeout.timer-clear-timeout()
    expect(()) { close(timeout.uv-handle) }

  // catches errors during event loop finalization
  effectful-test("successful process")
    // TODO: use async process once that's stable
    expect(0) {
      run-system("koka -e test/uv/embedded-event-loop.kk >/dev/null")
    }

  effectful-test("successful process (output)")
    expect-contains("hello, world!")
      run-system-read("koka -e test/uv/embedded-event-loop.kk").untry

