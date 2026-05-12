module std/async/fs

import uv/fs
import uv/status-code
import std/os/env
import std/os/path
import std/async/async
import std/num/int32

abstract value struct file-read<s::S> { internal: file }
pub fun read/readable(f: file-read<s>): file-read<s> -> f

abstract value struct file-write<s::S> { internal: file }
pub fun write/writable(f: file-write<s>): file-write<s> -> f

abstract value struct file-rw<s::S> { internal: file }
pub fun rw/readable(f: file-rw<s>): file-read<s> -> File-read(f.internal)
pub fun rw/writable(f: file-rw<s>): file-write<s> -> File-write(f.internal)

abstract value struct tempfile<s::S>
  internal: file
  path: string

pub fun path(f: tempfile<_>): string -> f.tempfile/path
pub fun tmp/readable(f: tempfile<s>): file-read<s> -> File-read(f.internal)
pub fun tmp/writable(f: tempfile<s>): file-write<s> -> File-write(f.internal)

type write-mode
  Truncate
  Append

// TODO: promote to core/hnd?
fun nonrepeatable(action: () -> e a): e a
  with initially fn(i)
    debug/assert("nonrepeatable action resumed more than once", i == 0)
  action()

fun open-raw(path: string, flags: int32, action: file -> <async,io|e> a): <async,io|e> a
  val mode = int32/zero
  val f = await1(fn(cb) -> uv/fs/open(path, flags, mode, cb)).untry
  with finally { uv/fs/close(f).untry }
  nonrepeatable { action(f) }

pub fun open-read(path: string, action: forall<s> (file-read<s>) -> <async,io|e> a): <async,io|e> a
  open-raw(path, o_RDONLY) fn(f) action(File-read(f))

pub fun read<f>(file: f, ?readable: (f) -> file-read<s>): <async,io> bytes
  val r = readable(file)
  var result: bytes := empty()
  val buf: bytes = alloc(1024.ssize_t)
  while {
    val nread: ssize_t = await1(fn(cb) -> uv/fs/read(r.internal, buf, offset = current-offset, cb = cb)).untry
    if nread.int == 0 then
      False
    else
      // TODO: this copies `nread` bytes into a temporary buffer, then
      // appends them to `result`. Copy `nbytes` directly? Or return a slice
      // of `nbytes`?
      result := result ++ buf.adjust-length(nread)
      True
  } { () }
  result

val current-offset = -1.ssize_t

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

// debugging
fun error(e: error<t>): maybe<exception>
  match e
    Ok(_) -> Nothing
    Error(e) -> Just(e)

pub fun tempfile(action: forall<s> (tempfile<s>) -> <async,io|e> a, name: string = "tmp-", dir: maybe<string> = Nothing): <async,io|e> a
  val base = match dir
    Just(d) -> path(d)
    Nothing -> tempdir()
  val template = base / (name ++ "XXXXXX")
  val (f, path) = await1(fn(cb) -> mkstemp(template.string, cb)).untry
  with finally { uv/fs/close(f).untry }
  nonrepeatable { action(Tempfile(f, path)) }

pub fun open-rw(path: string, action: forall<s> (file-rw<s>) -> <async,io|e> a,
  write-mode: write-mode = Truncate
): <async,io|e> a
  var flags := o_RDWR.and(write-flags(write-mode))
  open-raw(path, flags) fn(f) action(File-rw(f))

pub fun write<f>(file: f, bytes: bytes, ?writable: (f) -> file-write<s>): <async,io> ()
  val w = writable(file)
  val _n = await1(fn(cb) -> write(w.internal, bytes, 0.int64, cb)).untry
  // TODO is n ever less than `bytes` len?
  ()

pub fun fsync<f>(file: f, ?writable: (f) -> file-write<s>): <async,io> ()
  val w = writable(file)
  await1(fn(cb) -> uv/fs/fsync(w.internal, cb)).untry
