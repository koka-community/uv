/*---------------------------------------------------------------------------
  Copyright 2023 Tim Whiting.

  This is free software; you can redistribute it and/or modify it under the
  terms of the Apache License, Version 2.0. A copy of the License can be
  found in the LICENSE file at the root of this distribution.
---------------------------------------------------------------------------*/

module uv/pipe
pub import uv/event-loop
pub import std/core/bytes
pub import uv/utils
import uv/stream

extern import
  c file "inline/pipe.c"

pub extern mkpipe(): error<(uv-stream, uv-stream)>
  c "kk_uv_pipe"
