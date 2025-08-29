/*---------------------------------------------------------------------------
  Copyright 2025 Tim Cuthbertson.

  This is free software; you can redistribute it and/or modify it under the
  terms of the Apache License, Version 2.0. A copy of the License can be
  found in the LICENSE file at the root of this distribution.
---------------------------------------------------------------------------*/

// A Koka wrapper around the libuv process API
module uv/process

import uv/utils
import std/num/int32
import std/num/int64

extern import
  c file "inline/process.c"

// A command specification, which can be used to spawn a process
pub struct uv-command
  file: string
  args: list<string>
  stdin: stdio-stream
  stdout: stdio-stream
  stderr: stdio-stream

pub type stdio-stream
  Stream-ignore
  // stream-mkpipe // TODO
  Stream-fd(fd: int32)

// pub fun pipe-stdout(pipe): child-output
//   Split(Output-to(pipe), Inerhit-output-stream)

// A running process
// https://docs.libuv.org/en/v1.x/process.html
pub value struct uv-process { internal: any }

pub alias uv-exit-cb = (status: int64, signal: int32) -> io-noexn ()

pub fun spawn(command: uv-command, on-complete: uv-exit-cb): io uv-process
  // https://docs.libuv.org/en/v1.x/process.html
  // Command line arguments. args[0] should be the path to the program.
  val raw-cmd = command(args = Cons(command.file, command.args))
  untry(spawn-c(raw-cmd, on-complete))

extern spawn-c(
  command: uv-command,
  on-complete: uv-exit-cb
): io-noexn error<uv-process>
  c "kk_uv_proc_spawn_c"

pub extern pid(^process: uv-process): int32
  c "kk_uv_proc_pid"

pub fun signal(^process: uv-process, signal: int32): exn ()
  process.try-signal(signal).untry

extern try-signal(^process: uv-process, signal: int32): error<()>
  c "kk_uv_proc_signal"
