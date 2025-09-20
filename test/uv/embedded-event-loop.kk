import std/async/async
import uv/event-loop

fun main()
  with default-event-loop
  with async/handle
  try { wait(0.1) } fn(_) (impossible("exception in wait")) // TODO this seems unnecessary
  println("hello, world!")
