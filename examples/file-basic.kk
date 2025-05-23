/*----------------------------------------------------------------------------
   Copyright 2024, Koka-Community Authors

   Licensed under the MIT License ("The License"). You may not
   use this file except in compliance with the License. A copy of the License
   can be found in the LICENSE file at the root of this distribution.
----------------------------------------------------------------------------*/

import uv/file

val readCount = 1024
fun readFileX(fd: uv-file, offset: int = 0, str: ref<global, string>)
  read(fd, readCount.int32, offset.int32) fn(bytes1)
    match bytes1
      Ok((bytes, len)) ->
        str := !str ++ bytes.string
        if len == readCount then
          readFileX(fd, offset + readCount, str=str)
        else
          (!str).println
          println("\n\nDone!")
      _ -> ()

fun main()
  with default-event-loop
  // Low Level Async API example (see async-read-file for example using async library)
  // open("stack.yaml", o_RDONLY) fn(fd1)
  //   match fd1
  //     Ok(fd) ->
  //       println("Got fd " ++ fd.internal.int.show ++ "\n")
  //       readFileX(fd, 0, ref(""))
  //     _ -> ()
  // Sync API Example
  val stackerr = open-sync("stack.yaml", o_RDONLY, 0.int32)
  match stackerr
    Ok(stack) ->
      val reserr = stack.read-sync(2048.int32)
      match reserr
        Ok((bytes, _)) ->
          println("Got fd sync " ++ stack.internal.int.show ++ "\n")
          println(bytes.string)
          println("\n\nDone!")
        _ -> ()
    _ -> ()
  val dir = mkdir-sync("scratch/test", s_IRWXU)
  val dir2 = mkdtemp-sync("scratch/test/tmpXXXXXX")
  match dir2
    Ok(d) ->
      println("Created dir " ++ d ++ "\n")
      val tmpf = mkstemp-sync(d ++ "/tmpXXXXXX")
      val tmpfile = open-sync("scratch/test/tmpfile.txt", o_CREAT.or(o_RDWR), s_IRWXU)
      match tmpfile
        Ok(f) ->
          println("Created tmp file\n")
          match f.write-sync("Hello World!".bytes, 0.int64)
            Ok(n) -> ("Wrote " ++ n.show ++ " bytes").println
            Error(e) -> throw-exn(e)
        Error(e) -> throw-exn(e)
      match tmpf
        Ok((f, name)) ->
          println("Created tmp file " ++ name ++ "\n")
          match f.write-sync("Hello World!".bytes, 0.int64)
            Ok(n) ->
              ("Wrote " ++ n.show ++ " bytes").println
              unlink-sync(name)
              ()
            Error(e) -> throw-exn(e)
        Error(e) -> throw-exn(e)
      rmdir-sync(d)
      ()
    _ -> ()
  val x = stat-sync("stack.yaml")
  match x
    Ok(s) -> println(s.show)
    Error(e) -> throw-exn(e)
  rename-sync("scratch/test.kk", "scratch/test2.kk")
  copyfile-sync("scratch/test2.kk", "scratch/test.kk", uv_fs_COPYFILE_EXCL)
  unlink-sync("scratch/test2.kk")
  ()
