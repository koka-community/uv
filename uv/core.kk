// v2, using embedded structures
module uv/core
pub import uv/status-code
import std/core-extras

extern import
  c { conan="libuv[>=1.47.0]"; vcpkg="libuv"; library="uv" }

extern import
  c file "inline/core"

// Configures uv with Koka's allocators when importing this file
val @initialize = init-uv-alloc()

// initializes only the allocators (not an event loop)
// needed for some file operations which alloc - kk_uv_fs_mkdtemp, kk_uv_fs_mkstemp
pub extern init-uv-alloc(): ()
  c "kk_uv_alloc_init"

fun abort-unless-ok(status, desc)
  if !status.is-uv_OK then
    println("[uv/event-loop] " ++ desc ++ " error: " ++ status.show ++ " (" ++ status.message ++ ")")
    exit(1)

// Runs a UV loop. Aborts on error
pub fun handle-loop(action)
  init-loop()
  val res = action()
  abort-unless-ok(run-loop(), "run-loop")
  abort-unless-ok(close-loop(), "close-loop")
  res

// Closes a uv loop
extern close-loop(): io-noexn uv-status-code
  c "kk_uv_loop_close"

// Runs a uv loop (or the emscripten loop on wasm)
extern run-loop(): io-noexn uv-status-code
  c "kk_uv_loop_run"

// Initializes a uv loop
extern init-loop(): io-noexn ()
  c "kk_uv_loop_init"

// The base uv handle type
pub value struct uv-handle { internal: any }
