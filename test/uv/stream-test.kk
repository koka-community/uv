import uv/event-loop
import uv/pipe
import uv/stream
import std/stream
import std/test
import std/async/async
import std/num/int32
import std/time/duration
import uv/timer // settimeout
import uv/tcp

fun tapprint(t: t, desc: string, ?show: t -> string): <expect> t
  hint(desc ++ ": " ++ show(t))
  t

fun main()
  with default-event-loop
  with async/handle
  run-tests(suite)

fun suite() // : <io,async,test<<io,async>>> ()
  effectful-test("reading & writing a pipe")
    expect("hello")
      val (r, w) = mkpipe().untry
      w.write("hello")
      println("closing w")
      w.close().untry
      println("closed w")

      val output = r.read-all().string
      println("closing read side")
      r.close().untry
      println("returning output: " ++ output)
      output

  effectful-test("reading & writing a socket")
    expect(("hello!", "pong: hello!"))
      val server: tcp = tcp-init().untry
      val bind-addr = Sock-addr(
        family = AF_INET,
        data = "127.0.0.1",
        port = Nothing
      )
      hint("binding to: " ++ bind-addr.show)
      server.bind(bind-addr, 0.int32).tapprint("bind").untry
      hint("server: bound...")
      val server-addr = server.get-sockname.tapprint("get-sockname").untry
      val server-port = server-addr.port.unjust
      hint("server: running on port " ++ server-port.show)
      val server-stream = server.uv-stream()
      with finally {
        hint("closing server-stream")
        server-stream.close().untry
      }

      interleaved(
        {
          // Note: only works for 1 connection
          val () = await1(fn(cb) server-stream.listen(1.int32, cb)).tapprint("listen").untry
          hint("server: saw connection")
          val client = tcp-init().untry
          val client-stream = client.uv-stream()
          hint("server: accepting")
          server-stream.accept(client-stream)
          hint("server: reading client input")
          val input = client-stream.read-all()
          hint("server: responding to: " ++ input.string)
          client-stream.write("pong: " ++ input.string)
          hint("server: shutting down client-stream")
          val () = await1(fn(cb) client-stream.shutdown(cb)).untry

          hint("server: closing client stream")
          client.uv-stream.close().untry

          input.string
        },
        {
          val client = tcp-init().untry
          val client-stream = client.uv-stream()
          hint("client: connecting to port " ++ server-port.show ++ " ...")
          val () = await1(fn(cb) client.connect(server-addr, cb)).untry
          hint("client: connected to server")
          client-stream.write("hello!")
          hint("client: shutting down client-stream")
          val () = await1(fn(cb) client-stream.shutdown(cb)).untry
          hint("client: reading response")
          val output = client-stream.read-all()

          hint("client: closing client stream")
          client.uv-stream.close().untry
          
          output.string
        }
      )
