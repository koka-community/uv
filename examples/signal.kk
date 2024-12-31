/*----------------------------------------------------------------------------
   Copyright 2024, Koka-Community Authors

   Licensed under the MIT License ("The License"). You may not
   use this file except in compliance with the License. A copy of the License
   can be found in the LICENSE file at the root of this distribution.
----------------------------------------------------------------------------*/

import uv/signal

fun main()
  with default-event-loop
  val n = ref(0)
  println("Running main")
  signal-start-oneshot(sSIGINT) fn() 
    println("Got SIGINT!")
    signal-start(sSIGINT) fn(s)
      if !n == 5 then
        s.signal-stop()
        ()
      else
        println("Got SIGINT! x " ++ (!n).show)
        n := !n + 1
    ()
  ()
