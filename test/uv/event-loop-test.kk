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

fun wait-in-finally(action: () -> <io,async|e> a): <io,async|e> a
  with finally { wait(0.1) }
  action()

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

  // TODO: group doesn't seem to work with effectful-test
  // group("sleep in finalize")
  effectful-test("single value")
    val result = wait-in-finally { 1 }
    expect(1) { result }

  effectful-test("single string")
    val (result) = wait-in-finally { "some long string" }
    expect("some long string") { result }

  effectful-test("tuple")
    val (one, _) = wait-in-finally { (1, 2) }
    println("got result")
    expect(1) { one }
