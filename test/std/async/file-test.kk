import std/test
import std/async/async
import std/async/async-file
import uv/event-loop
import std/core/bytes
import std/core/int

fun main(): io-noexn ()
  with default-event-loop
  with async/handle
  run-tests(suite)

fun suite(): <async,test<<io,async>>> ()
  val contents = "hello, world!"

  // effectful-test("write & read a file")
  //   val path = "/tmp/kktest-1"
  //   open-write(path) fn(f)
  //     f.write(contents.bytes)

  //   expect(contents)
  //     open-read(path) fn(f)
  //       f.read().string()

  effectful-test("tempfile")
    println("start")
    val (_path, file-contents) = tempfile(name="mytmp-") fn(f)
      println("file at " ++ f.path ++ "...")
      f.write(contents.bytes)
      println("reading...")
      (f.path, f.read())

    expect(contents) { file-contents.string }
    // expect(path) { "TODO" }
