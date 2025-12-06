import uv/event-loop
import uv/pipe
import uv/net
import uv/stream
import std/stream
import std/test
import std/async/async
import std/num/int32

fun error/show(e: error<t>, ?t/show: t -> string): string
  match e
    Ok(t) -> "Ok(" ++ t/show(t) ++ ")"
    Error(e) -> "Error(" ++ show(e) ++ ")"

fun uv/show(status: uv-status-code): string
  "uv: " ++ status.message

fun tapprint(t: t, desc: string, ?show: t -> string): <console> t
  println(desc ++ ": " ++ show(t))
  t

fun main()
  with default-event-loop
  with async/handle
  run-tests(suite)

fun suite()
  // effectful-test("reading & writing a pipe")
  //   expect("hello")
  //     val (r, w) = mkpipe().untry
  //     val ((), output) = interleaved(
  //       {
  //         w.write("hello")
  //         w.close()
  //       },
  //       {
  //         with finally { r.close() }
  //         r.read-all().string
  //       }
  //     )
  //     output

  effectful-test("reading & writing a socket")
    expect(("hello!", "pong: hello!"))
      val server: uv-tcp = tcp-init().untry
      val bind-addr = Sock-addr(
        family = AF_INET,
        data = "127.0.0.1",
        port = Nothing
      )
      println("binding to: " ++ bind-addr.show)
      server.bind(bind-addr, 0.int32).tapprint("bind").untry
      println("server: bound...")
      val server-addr = server.get-sockname.tapprint("get-sockname").untry
      val server-port = server-addr.port.unjust
      println("server: running on port " ++ server-port.show)
      val server-stream = server.stream()
      with finally { server-stream.close() }

      interleaved(
        {
          // Note: only works for 1 connection
          val () = await1(fn(cb) server-stream.listen(1.int32, cb)).tapprint("listen").untry
          println("server: saw connection")
          val client = tcp-init().untry
          val client-stream = client.stream()
          println("server: accepting")
          server-stream.accept(client-stream)
          println("server: reading client input")
          val input = client-stream.read-all()
          println("server: responding to: " ++ input.string)
          client-stream.write("pong: " ++ input.string)
          val () = await1(fn(cb) client-stream.shutdown(cb)).untry

          // is this necessary?
          client.stream.close()
          
          input.string
        },
        {
          val client = tcp-init().untry
          val client-stream = client.stream()
          println("client: connecting to port " ++ server-port.show ++ " ...")
          val () = await1(fn(cb) client.tcp/connect(server-addr, cb)).untry
          println("client: connected to server")
          client-stream.write("hello!")
          val () = await1(fn(cb) client-stream.shutdown(cb)).untry
          val output = client-stream.read-all()

          // is this necessary?
          client.stream.close()
          
          output.string
        }
      )
