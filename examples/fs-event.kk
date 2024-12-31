/*----------------------------------------------------------------------------
   Copyright 2024, Koka-Community Authors

   Licensed under the MIT License ("The License"). You may not
   use this file except in compliance with the License. A copy of the License
   can be found in the LICENSE file at the root of this distribution.
----------------------------------------------------------------------------*/

import uv/fs-event
import uv/event-loop
import std/async

val scratch = "scratch"
fun main()
  val event = uv-fs-event-init().untry
  event.uv-fs-event-start(scratch, []) fn (res)
    match res
      Ok((path, kind)) -> 
        path.trace
        kind.show.trace
      Error(err) -> 
        err.show.trace
  wait(5.seconds)
  event.uv-fs-event-stop().untry
