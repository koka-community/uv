/*---------------------------------------------------------------------------
  Copyright 2023 Tim Whiting.

  This is free software; you can redistribute it and/or modify it under the
  terms of the Apache License, Version 2.0. A copy of the License can be
  found in the LICENSE file at the root of this distribution.
---------------------------------------------------------------------------*/

// Provides low level file operations using libuv callbacks
module uv/fs

import uv/handle
pub import std/core/bytes
pub import std/num/int32
pub import std/num/int64

extern import
  c file "inline/fs.c"

// A Koka representation of the timespec struct
pub value struct timespec
  sec: int64
  nsec: int32;

pub fun timespec/show(t: timespec): string
  t.sec.show ++ "." ++ t.nsec.show.pad-left(9, '0')

// A Koka representation of the fstat struct
pub struct fstat
  dev: int64
  mode: int64
  nlink: int64
  uid: int64
  gid: int64
  rdev: int64
  ino: int64
  size: int64
  blksize: int64
  blocks: int64
  flags: int64
  atime: timespec
  mtime: timespec
  ctime: timespec
  birthtime: timespec

pub fun fstat/show(f: fstat): string
  "{dev=" ++ f.dev.show ++ ", mode=" ++ f.mode.show ++ ", nlink=" ++ f.nlink.show ++ ", uid=" ++ f.uid.show ++
   ", gid=" ++ f.gid.show ++ ", rdev=" ++ f.rdev.show ++ ", ino=" ++ f.ino.show ++ ", size=" ++ f.size.show ++
   ", blksize=" ++ f.blksize.show ++ ", blocks=" ++ f.blocks.show ++ ", flags=" ++ f.flags.show ++ ", atime=" ++
   f.atime.show ++ ", mtime=" ++ f.mtime.show ++ ", ctime=" ++ f.ctime.show ++ ", birthtime=" ++ f.birthtime.show ++ "}"

// A Koka representation of the statfs struct
pub struct statfs
  filetype: int64
  bsize: int64
  blocks: int64
  bfree: int64
  bavail: int64
  files: int64
  ffree: int64
  fspare: vector<int64> // Length 4 Convert to tuple?

pub fun statfs/show(f: statfs): string
  "{filetype=" ++ f.filetype.show ++ ", bsize=" ++ f.bsize.show ++ ", blocks=" ++ f.blocks.show ++ ", bfree=" ++
  f.bfree.show ++ ", bavail=" ++ f.bavail.show ++ ", files=" ++ f.files.show ++ ", ffree=" ++ f.ffree.show ++
  ", fspare=[" ++ f.fspare.vector/map(fn(s) s.int64/show).list.join(",") ++ "]}"

// Directory entry types
pub type dirent-type
  UNKNOWN_DIRECTORY_ENTRY
  DIRENT_FILE // TODO rename FILE, koka gets confused by FILE builtin function?
  DIR
  LINK
  FIFO
  SOCKET
  CHAR
  BLOCK

// A Koka representation of a directory entry
pub value struct dirent
  name: string
  entity-type: dirent-type;

pub alias create-mode = int32
// Octal 0o700 // Read, write, execute by owner.
pub val s_IRWXU = (7 * 64 + 0 + 0).int32
// Octal 0o400 // Read permission, owner.
pub val s_IRUSR = (4 * 64 + 0 + 0).int32
// Octal 0o200 // Write permission, owner.
pub val s_IWUSR = (2 * 64 + 0 + 0).int32
// Octal 0o100 // Execute/search permission, owner.
pub val s_IXUSR = (1 * 64 + 0 + 0).int32
// Octal 0o070 // Read, write, execute by group.
pub val s_IRWXG = (0 + 7 * 8 + 0).int32
// Octal 0o040 // Read permission, group
pub val s_IRGRP = (0 + 4 * 8 + 0).int32
// Octal 0o020 // Write permission, group..
pub val s_IWGRP = (0 + 2 * 8 + 0).int32
// Octal 0o010 // Execute/search permission, group.
pub val s_IXGRP = (0 + 1 * 8 + 0).int32
// Octal 0o007 // Read, write, execute by others.
pub val s_IRWXO = (0 + 0 + 7).int32
// Octal 0o004 // Read permission, others.
pub val s_IROTH = (0 + 0 + 4).int32
// Octal 0o002 // Write permission, others.
pub val s_IWOTH = (0 + 0 + 2).int32
// Octal 0o001 // Execute/search permission, others.
pub val s_IXOTH = (0 + 0 + 1).int32

// Octal 0o4000 // Set-user-ID
pub val s_ISUID = (4 * 512 + 0 +  0 + 0).int32
// Octal 0o2000 // Set-group-ID (see inode(7))
pub val s_ISGID = (0 + 2 * 512 + 0 + 0).int32
// Octal 0o1000 // Sticky bit (see inode(7))
pub val s_ISVTX = (0 + 0 + 1 * 512 + 0).int32

// Read-only access.
pub val o_RDONLY = oRDONLY()
extern oRDONLY(): int32
  c inline "UV_FS_O_RDONLY"
// Write-only access.
pub val o_WRONLY = oWRONLY()
extern oWRONLY(): int32
  c inline "UV_FS_O_WRONLY"
// Read-write access.
pub val o_RDWR = oRDWR()
extern oRDWR(): int32
  c inline "UV_FS_O_RDWR"
// Create the file if it does not exist.
pub val o_CREAT = oCREAT()
extern oCREAT(): int32
  c inline "UV_FS_O_CREAT"
// If pathname already exists, then fail the open with the error EEXIST.
pub val o_EXCL = oEXCL()
extern oEXCL(): int32
  c inline "UV_FS_O_EXCL"
// If pathname refers to a terminal device, don't allocate controlling terminal for this process.
pub val o_NOCTTY = oNOCTTY()
extern oNOCTTY(): int32
  c inline "UV_FS_O_NOCTTY"
// If the file exists and is a regular file, and the file is successfully opened O_RDWR or O_WRONLY, its length shall be truncated to 0. (Octal 0o1000)
pub val o_TRUNC = oTRUNC()
extern oTRUNC(): int32
  c inline "UV_FS_O_TRUNC"
// Before each write(2), the file offset is positioned at the end of the file, as if with lseek(2).
pub val o_APPEND = oAPPEND()
extern oAPPEND(): int32
  c inline "UV_FS_O_APPEND"
// Open the file in nonblocking mode if possible.
pub val o_NONBLOCK = oNONBLOCK()
extern oNONBLOCK(): int32
  c inline "UV_FS_O_NONBLOCK"
// The file is opened for synchronous I/O. Write operations will complete once all data and a minimum of metadata are flushed to disk.
pub val o_DSYNC = oDSYNC()
extern oDSYNC(): int32
  c inline "UV_FS_O_DSYNC"
// direct disk access hint
pub val o_DIRECT = oDIRECT()
extern oDIRECT(): int32
  c inline "UV_FS_O_DIRECT"
// must be a directory
pub val o_DIRECTORY = oDIRECTORY()
extern oDIRECTORY(): int32
  c inline "UV_FS_O_DIRECTORY"
// don't follow links
pub val o_NOFOLLOW = oNOFOLLOW()
extern oNOFOLLOW(): int32
  c inline "UV_FS_O_NOFOLLOW"
// Do not update the file access time when the file is read
pub val o_NOATIME = oNOATIME()
extern oNOATIME(): int32
  c inline "UV_FS_O_NOATIME"
// Create an unnamed temporary file. pathname specifies a directory
pub val o_TMPFILE = oTMPFILE()
extern oTMPFILE(): int32
  c inline "UV_FS_O_TEMPORARY"

// A uv filesystem request type
// pub value struct uv-fs-req { internal : any };

// A uv file type
pub value struct file { internal : any };

// A uv directory type
// pub value struct uv-dir { internal : intptr_t };

// Drops the handle's internal reference, triggering
// closure of the underlying resource. Returns UV_EBUSY
// if there are still other references to the handle.
pub extern close(file: file): io-noexn uv-status-code
  c "kk_uv_file_close_impl"

// Equivalent to open(2).
// WARNING: On Windows libuv uses CreateFileW and thus the file is always opened in binary mode. Because of this the O_BINARY and O_TEXT flags are not supported.
pub extern open(path: string, flags: int32, mode: int32, cb: (error<file>) -> io-noexn ()): io-noexn ()
  c "kk_uv_fs_open_impl"

pub extern read(^file: file, ^buf: bytes, offset: ssize_t, cb: (error<ssize_t>) -> io-noexn ()): io-noexn ()
  c "kk_uv_fs_read_impl"

pub extern write(^file: file, buf: bytes, offset: int64, cb: (error<ssize_t>) -> io-noexn ()): io-noexn ()
  c "kk_uv_fs_write_impl"

pub extern mkstemp(template: string, cb: (error<(file, string)>) -> io-noexn ()): io-noexn ()
  c "kk_uv_fs_mkstemp_impl"

pub extern fsync(^file: file, cb: (uv-status-code) -> io-noexn ()): io-noexn ()
  c "kk_uv_fs_fsync_impl"
