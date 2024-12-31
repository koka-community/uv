/*----------------------------------------------------------------------------
   Copyright 2024, Koka-Community Authors

   Licensed under the MIT License ("The License"). You may not
   use this file except in compliance with the License. A copy of the License
   can be found in the LICENSE file at the root of this distribution.
----------------------------------------------------------------------------*/

import uv/tty
import uv/stream
import std/num/int32

fun main()
  with default-event-loop
  val uv = uv-tty-init(0.int32).untry
  uv.stream.try-write(["Hello, world!\n".bytes])
  val size = uv.uv-tty-get-winsize().untry
  println("Window size is " ++ size.show)