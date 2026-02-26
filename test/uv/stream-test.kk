import uv/core
import uv/pipe
// import uv/net"
import uv/core
import uv/stream
import std/stream
import std/test
import std/async/async
import std/num/int32
import std/time/duration
import uv/timer // settimeout

fun error/show(e: error<t>, ?t/show: t -> string): string
  match e
    Ok(t) -> "Ok(" ++ t/show(t) ++ ")"
    Error(e) -> "Error(" ++ show(e) ++ ")"

fun tapprint(t: t, desc: string, ?show: t -> string): <expect> t
  hint(desc ++ ": " ++ show(t))
  t

fun main()
  with handle-loop
  with async/handle
  run-tests(suite)


extern get-ref-count(^box: any): io-noexn int32
  c inline "kk_block_refcount(kk_box_to_ptr(#1, kk_context()))"

fun borrow-stream(^w: uv-stream)
  println("I've borrowed a stream! its refcount is " ++ w.internal.get-ref-count.show)
  wait(1.milli-seconds)


fun noop-async(): async ()
  ()

fun suite()
  effectful-test("stdout")
    val stdout = stream/tty-init(1.int32).untry
    stdout.print-refcount()
    stdout.print-refcount()
    // w.write("hello")
    println("closing w")
    stdout.close().untry
    println("closed w")

    noop-async()
    // wait(1.milli-seconds)
    ()

  // effectful-test("reading & writing a pipe")
  //   expect("hello")
  //     val (r, w) = mkpipe().untry
  //     w.print-refcount()
  //     w.print-refcount()
  //     // w.write("hello")
  //     // wait(1.milli-seconds)
  //     // w.write("hello")
  //     // // println("after write, w refcount = " ++ w.internal.get-ref-count.show)
  //     println("closing w")
  //     w.close().untry
  //     println("closed w")

  //     // val output = r.read-all().string
  //     // println("TODO: close r with refcount=" ++ r.internal.get-ref-count.show)
  //     // use-stream(r)
  //     // copy-stream(r)
  //     // r.close().untry
  //     // // output
  //     // wait(1.milli-seconds)
  //     noop-async()
  //     ""

  // effectful-test("reading & writing a socket")
  //   expect(("hello!", "pong: hello!"))
  //     val server: uv-tcp = tcp-init().untry
  //     val bind-addr = Sock-addr(
  //       family = AF_INET,
  //       data = "127.0.0.1",
  //       port = Nothing
  //     )
  //     hint("binding to: " ++ bind-addr.show)
  //     server.bind(bind-addr, 0.int32).tapprint("bind").untry
  //     hint("server: bound...")
  //     val server-addr = server.get-sockname.tapprint("get-sockname").untry
  //     val server-port = server-addr.port.unjust
  //     hint("server: running on port " ++ server-port.show)
  //     val server-stream = server.stream()
  //     with finally { server-stream.close() }

  //     interleaved(
  //       {
  //         // Note: only works for 1 connection
  //         val () = await1(fn(cb) server-stream.listen(1.int32, cb)).tapprint("listen").untry
  //         hint("server: saw connection")
  //         val client = tcp-init().untry
  //         val client-stream = client.stream()
  //         hint("server: accepting")
  //         server-stream.accept(client-stream)
  //         hint("server: reading client input")
  //         val input = client-stream.read-all()
  //         hint("server: responding to: " ++ input.string)
  //         client-stream.write("pong: " ++ input.string)
  //         val () = await1(fn(cb) client-stream.shutdown(cb)).untry

  //         // is this necessary?
  //         client.stream.close()
          
  //         input.string
  //       },
  //       {
  //         val client = tcp-init().untry
  //         val client-stream = client.stream()
  //         hint("client: connecting to port " ++ server-port.show ++ " ...")
  //         val () = await1(fn(cb) client.tcp/connect(server-addr, cb)).untry
  //         hint("client: connected to server")
  //         client-stream.write("hello!")
  //         val () = await1(fn(cb) client-stream.shutdown(cb)).untry
  //         val output = client-stream.read-all()

  //         // is this necessary?
  //         client.stream.close()
          
  //         output.string
  //       }
  //     )
