import uv/event-loop
import std/num/int32
import std/async/process
import std/stream
import std/test
import std/async/async
import uv/pipe
import uv/process

fun main()
  with default-event-loop
  with async/handle
  run-tests(suite)

fun error-message(action: () -> <exn|e> a): e maybe<string>
  match action.try
    Error(ex) -> Just(ex.message)
    Ok(_) -> Nothing

fun expect-error-message(message, action)
  expect(Just(message)) { error-message(action) }

fun expect-process-finished(pid: int)
  expect-error-message("no such process") { pid/signal(pid.int32, 0.int32).untry }

fun suite()
  effectful-test("success")
    expect(())
      command("true", []).run-unit()

  effectful-test("failure")
    expect-error-message("Command failed (status:1)")
      command("false", []).run-unit()

  effectful-test("allowable failure")
    expect(Exit-code(1))
      command("false", []).run(check-exit = False) fn(p)
        p.await()

  effectful-test("`run` does not return until the process ends")
    val p = command("sleep", ["0.3"]).run(fn(p) p.pid)
    expect-process-finished(p)

  effectful-test("`run` kills the process when the block throws")
    var process := Nothing
    val errmsg = "error in block"
    expect-error-message(errmsg)
      command("sleep", ["100"]).run fn(p)
        process := Just(p.pid)
        throw(errmsg)
    expect-process-finished(process.unjust)

  effectful-test("capture output")
    expect("1 2 words with     many spaces\n") {
      val c = command("echo", ["1", "2", "words with     many spaces"], output=capture-stdout)
      c.run(fn(p) p.output)
    }

  effectful-test("stdin/out pipes")
    expect("hello!")
      val (readable, writable) = mkpipe().untry
      val c = command("cat", [], output = capture-stdout).with-input(readable)
      c.run fn(p)
        val (_, output) = interleaved(
          {
            writable.write("hello!")
            writable.close()
          },
          { p.output }
        )
        output
