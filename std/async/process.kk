module std/async/process

// Note: importing std/async causes clash with set-timeout / clear-timeout
import std/async/async
import uv/process
import uv/signal
import std/stream
import std/num/int32
import std/num/int64
import std/time/duration

// high-level wrappers for uv/process
// TODO: support passing in pipes, and using strings for input/output

// A command specification, which can be used to spawn a process
abstract struct command
  exe: string
  args: list<string>
  input: child-input
  output: child-output

pub fun command(
  exe,
  args: list<string> = [],
  input: child-input = default,
  output: child-output = default
  ) Command(exe, args, input, output)

pub fun command/show(command: command)
  "command("++command.exe.show++", "++command.args.show++")"

type child-input
  Ignore-input
  Inherit-input

pub val input/default = Inherit-input
pub val input/inherit: child-input = Inherit-input
pub val input/ignore: child-input = Ignore-input

pub fun command/with-output(cmd: command, output: child-output) cmd(output = output)

type child-output-stream
  Ignore-output
  Inerhit-output
  Capture-output
  // Output-to(pipe)

type child-output
  // Capture-merged
  // Merge-output-to(pipe)
  Split(stdout: child-output-stream, stderr: child-output-stream)

pub val output/default: child-output = Split(Inerhit-output, Inerhit-output)
pub val output/inherit: child-output = Split(Inerhit-output, Inerhit-output)
pub val output/ignore: child-output = Split(Ignore-output, Ignore-output)
pub val const/capture-stdout: child-output = Split(Capture-output, Inerhit-output)
pub val const/capture-outputs: child-output = Split(Capture-output, Capture-output)
// pub val const/capture-merged: child-output = Capture-merged

// fun update/capture-stdout(o: child-output): child-output
//   match o
//     Capture-merged -> Capture-merged // TODO error?
//     Split(_, stderr) -> Split(Capture-output, stderr)

fun update/ignore-stderr(o: child-output): child-output
  match o
    // Capture-merged -> Split(Capture-output, Ignore-output)
    Split(stdout, _) -> Split(stdout, Ignore-output)

abstract struct output-streams(stdout: maybe<io-stream>, stderr: maybe<io-stream>)

abstract struct process
  internal: uv-process
  exit-p: promise<exit>
  output-streams: output-streams

pub type exit
  Exit-code(code: int)
  Exit-signal(signal: int)

pub fun exit/show(exit: exit)
  match exit
    Exit-code(i) -> "status:" ++ i.show
    Exit-signal(s) -> "signal:" ++ s.show

pub fun exit/ok(exit: exit)
  match exit
    Exit-code(0) -> True
    _ -> False

pub fun exit/failed(exit: exit) !exit.ok

pub fun exit/check-exit(exit: exit): exn ()
  if exit.failed then
    throw("Command failed (" ++ show(exit) ++ ")")

pub fun exit/(==)(a: exit, b: exit): div bool
  match (a,b)
    (Exit-code(a), Exit-code(b)) -> a == b
    (Exit-signal(a), Exit-signal(b)) -> a == b
    _ -> False

fun stdio-stream-of-output(output: child-output-stream, inherit-fd): asyncx (stdio-stream, maybe<io-stream>)
  match output
    Ignore-output -> (Stream-ignore, Nothing)
    Inerhit-output -> (Stream-fd(inherit-fd), Nothing)
    Capture-output ->
      val (readable, writable) = mkpipe().untry
      (Stream-uv(writable), Just(readable))

pub type kill-strategy
  Signal-once(signal: int32)
  Signal-repeat(signal: int32, wait: duration)
  Signal-escalate(wait: duration)

pub val kill-strategy/default = Signal-escalate(20.seconds)

// TODO: move into async?
// Without cancelation, this likely causes a memory leak
// because it is never resolved.
fun never(): asyncx a
  await0 fn(cb) ()
  impossible("impossible: never() resumed")

fun do-kill(process: process, strategy: kill-strategy): <io,async> ()
  match strategy
    Signal-once -> process.signal(sSIGINT)
    Signal-repeat(signum, wait-time) ->
      while { True }
        process.signal(signum)
        // TODO: async/wait should work?
        duration/wait(wait-time)
    Signal-escalate(wait-time) ->
      [sSIGINT, sSIGQUIT, sSIGKILL].foreach fn(signum)
        process.signal(signum)
        duration/wait(wait-time)

// kill a process and wait for it to end
pub fun kill(process: process, strategy: kill-strategy = default): <io,async> exit
  fun perform()
    do-kill(process, strategy)
    never()
  firstof(perform, { process.await })

// send a signal to a process and immeditately return.
// See `kill` for a variant which waits for the process to exit
pub fun signal(process: process, sig: int32): <io,async> ()
  uv/process/signal(process.internal, sig)

fun finalize-process(p, strategy: kill-strategy)
  match p.exit-p.try-await
    Just(_) -> ()
    Nothing ->
      p.kill(strategy)
      ()

  p.output-streams.stdout.foreach(stream/close)
  p.output-streams.stderr.foreach(stream/close)

pub fun run-unit(command: command): <io,async|e> ()
  run(command) fn(_) ()

pub fun run-output(cmd: command): <async,io> string
  cmd.with-output(capture-stdout).run fn(p) p.output

pub fun run-outputs(cmd: command): <async,io> (string, string)
  cmd.with-output(capture-outputs).run fn(p) p.outputs

// TODO: add to core/maybe?
fun foreach(m: maybe<a>, action: a -> e ()): e ()
  match m
    Just(x) -> action(x)
    Nothing -> ()

pub fun run(
  command: command,
  action: (process) -> <io,async|e> a,
  // optional args
  check-exit: bool = True,
  kill-strategy: kill-strategy = default
  ): <io,async|e> a
  val uv-stdin = match command.input
    Ignore-input -> Stream-ignore
    Inherit-input -> Stream-fd(0.int32)

  val ((uv-stdout, stdout-stream), (uv-stderr, stderr-stream)) = match command.output
    Split(out, err) ->
      (
        stdio-stream-of-output(out, 1.int32),
        stdio-stream-of-output(err, 2.int32)
      )

  val exit-p = promise()
  fun on-exit(status: int64, signal: int32) ->
    val sig = signal.int
    val exit = if sig != 0 then Exit-signal(sig) else Exit-code(status.int)
    // TODO can we make this not throw?
    try {
      val _: bool = exit-p.try-resolve-io(exit)
      ()
    } fn(ex)
      impossible("RESOLVE: " ++ ex.show)

  val uv-cmd = Uv-command(
    file = command.exe,
    args = command.args,
    stdin = uv-stdin,
    stdout = uv-stdout,
    stderr = uv-stderr
  )
  val uv-process = spawn(uv-cmd, on-exit)
  val process = Process(uv-process, exit-p, Output-streams(stdout-stream, stderr-stream))
  with finally
    // this finalizer will kill the process,
    // but only if it hasn't already ended
    // (which the success path will wait for)
    finalize-process(process, kill-strategy)

  // close the half of any pipes that are only used by the child
  close-owned(uv-stdout)
  close-owned(uv-stderr)

  val result = action(process)

  // if block succeeded, wait for process and check for failure
  val exit = process.await
  if check-exit then
    exit/check-exit(exit)
  result

fun close-owned(stream: stdio-stream): asyncx ()
  match stream
    Stream-uv(s) -> s.close()
    _ -> ()

pub fun await(process: process): <io,async> exit
  process.exit-p.await

pub fun pid(process: process): <io-noexn> int
  process.internal.uv/process/pid.int

fun require(v: maybe<io-stream>, desc: string): exn io-stream
  match v
    Just(v) -> v
    Nothing -> throw("Process has no " ++ desc ++ " stream")

pub fun output-stream(process: process): exn io-stream
  process.output-streams.stdout.require("stdout")

pub fun stderr-stream(process: process): exn io-stream
  process.output-streams.stderr.require("stderr")

pub fun output(process: process): <asyncx> string
  process.output-stream.read-all()

pub fun stderr-output(process: process): <asyncx> string
  process.stderr-stream.read-all()

pub fun outputs(process: process): <asyncx> (string, string)
  val out = process.output-stream()
  val err = process.stderr-stream()
  interleaved({ out.read-all() }, { err.read-all() })
