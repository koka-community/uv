pub import uv/utils
import std/core-extras
import std/num/int32

extern import
  c { conan="libuv[>=1.47.0]"; vcpkg="libuv"; library="uv" }

extern import
  c file "inline/handle"

// Drops the handle's internal reference, triggering
// closure of the underlying resource. Returns UV_EBUSY
// if there are still other references to the handle.
extern close-handle(hnd: uv-handle): io-noexn uv-status-code
  c "kk_uv_handle_close_impl"

// Close any resource with a `uv-handle` accessor
pub fun close(res: a, ?uv-handle: (a) -> uv-handle): io-noexn uv-status-code
  val hnd = res.uv-handle
  close-handle(hnd)
