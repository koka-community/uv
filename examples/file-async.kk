import std/os/file-async
import std/os/path
import std/async/async

fun main()
  default-async-uv
    "Read file".println
    "---------".println
    val res = read-as-string("../uv/examples/file-async.kk".path)
    res.println
    "---------".println
    "Done!".println
    "Stream from file".println
    var done := False
    val s = stream("../uv/examples/file-async.kk".path, chunkSize=20.int32) // Small to really test this
    while {!done} 
      match s.receive
        Error(Exception("EOF", ExnInternal("EOF"))) -> done := True
        Ok(bytes) -> bytes.string.print
    "---------".println
    "Done!".println
    "Write file".println
    "---------".println
    create-dir("../uv/scratch".path, s_IRWXU)
    with file = open("../uv/scratch/test.txt".path, o_RDWR.or(o_CREAT), s_IRWXU)
    write-to-file(file, "Hello, world!")
    "---------".println
    "Done!".println
  