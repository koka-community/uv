module std/async/async-file

import uv/file
import std/os/env
import std/os/path
import std/async/async

abstract value struct file-read<s::S> { internal: uv-file }
pub fun read/readable(f: file-read<s>): file-read<s> -> f

abstract value struct file-write<s::S> { internal: uv-file }
pub fun write/writable(f: file-write<s>): file-write<s> -> f

abstract value struct file-rw<s::S> { internal: uv-file }
pub fun rw/readable(f: file-rw<s>): file-read<s> -> File-read(f.internal)
pub fun rw/writable(f: file-rw<s>): file-write<s> -> File-write(f.internal)

abstract value struct tempfile<s::S>
  internal: uv-file
  path: string

pub fun path(f: tempfile<_>): string -> f.tempfile/path
pub fun tmp/readable(f: tempfile<s>): file-read<s> -> File-read(f.internal)
pub fun tmp/writable(f: tempfile<s>): file-write<s> -> File-write(f.internal)

type write-mode
  Truncate
  Append

fun open-raw(path: string, flags: int32, action: uv-file -> <async,io|e> a): <async,io|e> a
  val f = await1(fn(cb) -> uv/open(path, flags, 0.int32, cb)).untry
  // with finally { await1(fn(cb) -> uv/close(f, cb)).untry }
  action(f)

pub fun open-read(path: string, action: forall<s> (file-read<s>) -> <async,io|e> a): <async,io|e> a
  open-raw(path, o_RDONLY) fn(f) action(File-read(f))

pub fun read<f>(file: f, ?readable: (f) -> file-read<s>): <async,io> bytes
  val r = readable(file)
  var result: bytes := empty()
  while {
    match read-chunk(r)
      Just(chunk) ->
        result := result ++ chunk
        True
      Nothing -> False
  } { () }
  result

val current-offset = -1.int32

fun read-chunk<s>(f: file-read<s>): <asyncx> maybe<bytes>
  val (bytes, n) = await1(fn(cb) -> uv/read(f.internal, 1024.int32, offset = current-offset, cb = cb)).untry
  if n == 0 /* EOF */ then Nothing else Just(bytes)

fun write-flags(write-mode: write-mode)
  o_CREAT.and(match write-mode
    Truncate -> o_TRUNC
    Append -> o_APPEND
  )

pub fun open-write(path: string, action: forall<s> (file-write<s>) -> <async,io|e> a,
  write-mode: write-mode = Truncate
): <async,io|e> a
  var flags := o_RDONLY.and(write-flags(write-mode))
  open-raw(path, flags) fn(f) action(File-write(f))

pub fun tempfile(action: forall<s> (tempfile<s>) -> <async,io|e> a, name: string = "tmp-", dir: maybe<string> = Nothing): <async,io|e> a
  val base = match dir
    Just(d) -> path(d)
    Nothing -> tempdir()
  val template = base / (name ++ "XXXXXX")
  val (f, path) = await1(fn(cb) -> mkstemp(template.string, cb)).untry
  with finally { uv/close(f, fn(_) trace("here")) }
  action(Tempfile(f, path))

pub fun open-rw(path: string, action: forall<s> (file-rw<s>) -> <async,io|e> a,
  write-mode: write-mode = Truncate
): <async,io|e> a
  var flags := o_RDWR.and(write-flags(write-mode))
  open-raw(path, flags) fn(f) action(File-rw(f))

pub fun write<f>(file: f, bytes: bytes, ?writable: (f) -> file-write<s>): <asyncx> ()
  val w = writable(file)
  val _n = await1(fn(cb) -> write(w.internal, bytes, 0.int64, cb)).untry
  // TODO is n ever less than `bytes` len?
  ()

pub fun fsync<f>(file: f, ?writable: (f) -> file-write<s>): <asyncx> ()
  val w = writable(file)
  await1(fn(cb) -> fsync(w.internal, cb)).untry
