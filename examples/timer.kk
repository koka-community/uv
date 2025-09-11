import std/async/async
import uv/timer
import uv/event-loop
import std/time/duration
import std/time/timestamp

fun main()
  with default-event-loop
  with @default-async
  with err <- try
    val x = ref(0)
    timer(1000.milli-seconds) fn(t)
      "Hello From Timer".println;
      x := !x + 1
      if !x == 3 then False else True
    wait(1.seconds)
    "Before timeout".println
    wait(10.seconds)
    "Hello After Timeout".println
  err.println