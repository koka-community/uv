import uv/event-loop
import std/num/int32
import std/async/process
import std/test
import std/async/async

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

fun expect-process-finished(process)
  expect-error-message("no such process") { process.signal(0.int32) }

fun suite()
  effectful-test("success")
    expect(())
      command("true", []).run-unit()
    wait(0.2)

  effectful-test("failure")
    expect-error-message("Command failed (status:1)")
      command("false", []).run-unit()
    wait(0.2)

  effectful-test("allowable failure")
    expect(Exit-code(1))
      command("false", []).run(check-exit = False) fn(p)
        p.await()
    wait(0.2)

  effectful-test("`run` does not return until the process ends")
    val p = command("sleep", ["0.3"]).run(fn(p) p)
    expect-process-finished(p)
    wait(0.2)

  effectful-test("`run` kills the process when the block throws")
    var process := Nothing
    val errmsg = "error in block"
    expect-error-message(errmsg)
      command("sleep", ["100"]).run fn(p)
        process := Just(p)
        throw(errmsg)
    expect-process-finished(process.unjust)
    wait(0.2)

  // effectful-test("capture output")
  //   expect("echo 1 2 words with     many spaces\n") {
  //     val c = command("echo", ["1", "2", "words with     many spaces"], output=capture-stdout)
  //     c.run(fn(p) p.output)
  //   }
