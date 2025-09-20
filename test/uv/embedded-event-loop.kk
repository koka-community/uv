import std/async/async
import uv/event-loop

fun main()
  with default-event-loop
  with async/handle
  try { 
    trace("Before wait")
    wait(0.1)
    trace("After wait")
   } fn(_) (impossible("exception in wait")) // TODO this seems unnecessary
  println("hello, world!")
