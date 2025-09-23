NOTE: Work in Progress, also, the handler runtime might drastically change in a future version of Koka (which might cause https://github.com/koka-lang/libmprompt and https://github.com/koka-lang/nodec to make a comeback). This might obsolete this work.

As such, this work should be considered a fun project, learning opportunity, and temporary solution. Reference counting, representing C values in Koka, etc will be generally useful and informative to the future design as well. 

# Example

## For whatever reason Koka doesn't like to pick up homebrew includes on MacOS, and vcpkg doesn't resolve right
koka -e examples/file-async.kk -i../std --ccincdir=/opt/homebrew/include

## Design Notes
We have utils.c/utils.h which contain helper macros so that we can be consistent with how we wrap and deal with handles. 

Some handles are long-lived (e.g. uv_timer, uv_stream), and some are one off (uv_read_callback), and we need to handle the disposal of resources properly along with reference counts. Typically LibUV refers to the former as handles, and the latter as requests or callbacks. Many callbacks have the long lived handles as a first argument to the C callback. 
Long lived handles typically have Koka references to them, and can have Koka operations to update the state / status of them.
Short lived handles typically are only exposed to the C runtime, and Koka only sees the data in the callback.

Additionally some file-system operations are synchronous. 
These operations respect C stack discipline so the uv structs are stack allocated for these.

All operations can fail, and all callbacks can have an error callback.

All handles need to be closed when no longer in use.

One-off requests: Close in the callback.
Long-lived handles: Close when reference count reaches zero.
Callbacks, usually don't require anything. Move ownership of resources into Koka. 
- Since Koka can capture the handles given to callbacks in closures, they do not need to be passed back to Koka. Just pass the new data.

All operations with Koka callbacks (anything asynchronous), needs to store the callback in the C handle / request struct's data member. We should assert that there is not already something in the data member (concurrent requests from the same handle) - and return an error before allocating anything.

### Details for specific design decisions for special cases:
-- TODO 

### Details for multi-threading / multi-processing:
-- TODO