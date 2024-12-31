/*----------------------------------------------------------------------------
   Copyright 2024, Koka-Community Authors

   Licensed under the MIT License ("The License"). You may not
   use this file except in compliance with the License. A copy of the License
   can be found in the LICENSE file at the root of this distribution.
----------------------------------------------------------------------------*/

import uv/net
import std/os/net
import uv/stream
import std/num/int32
import std/async

fun handleStream(connection)
  val buf = connection.read
  connection.write(buf) 
  connection.shutdown()

fun handleConnections(connections)
  match connections.receive
    Ok(c) -> handleStream(c)
    _ -> throw("Error accepting connection")

fun main()
  val str = "GET / HTTP/1.1\r\n\r\n"
  val server = tcp()
  server.bind("0.0.0.0", 8000)
  val connections = server.listen
  interleaved({while({True}, {handleConnections(connections)})}) fn()
    val stream = tcp().connect("127.0.0.1", 8000)
    stream.write(str.bytes)
    val buf = stream.read
    buf.string.println
    server.shutdown()
    stream.shutdown() // also closes the tcp client handle
  println("Done")