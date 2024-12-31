/*----------------------------------------------------------------------------
   Copyright 2024, Koka-Community Authors

   Licensed under the MIT License ("The License"). You may not
   use this file except in compliance with the License. A copy of the License
   can be found in the LICENSE file at the root of this distribution.
----------------------------------------------------------------------------*/

import std/async
import uv/timer
import uv/event-loop
import std/time/duration
import std/time/timestamp

fun main()
  val x = ref(0)
  timer(1000.milli-seconds) fn(t)
    "Hello From Timer".println;
    x := !x + 1
    if !x == 3 then False else True
  wait(1.seconds)
  "Before timeout".println
  wait(10.seconds)
  "Hello After Timeout".println