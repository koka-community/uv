/*----------------------------------------------------------------------------
   Copyright 2024, Koka-Community Authors

   Licensed under the MIT License ("The License"). You may not
   use this file except in compliance with the License. A copy of the License
   can be found in the LICENSE file at the root of this distribution.
----------------------------------------------------------------------------*/

import std/os/file-async
import std/os/path
import std/async

fun main()
  "Read file".println
  "---------".println
  val res = read-as-string("stack.yaml".path)
  res.println
  "---------".println
  "Done!".println
  "Write file".println
  "---------".println
  val file = open("scratch/test.txt".path, o_RDWR.or(o_CREAT), s_IRWXU)
  write-to-file(file, "Hello, world!")
  "---------".println
  "Done!".println