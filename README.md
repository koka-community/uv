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
Long-lived handles: Close when reference count reaches zero. Freeing of memory doesn't happen until the (C) close callback is invoked.
Callbacks, usually don't require anything. Move ownership of resources into Koka. 
- Since Koka can capture the handles given to callbacks in closures, they do not need to be passed back to Koka. Just pass the new data.

All operations with Koka callbacks (anything asynchronous), needs to store the callback in the C handle / request struct's data member. We should assert that there is not already something in the data member (concurrent requests from the same handle) - and return an error before allocating anything.

### Handles, callbacks and errors:

The macro `KK_ALLOC_AND_BOX_HANDLE(timer);` will allocate and box a uv_timer_t structure. You must use the uv API to initialize this handle before using it (the macro defines the variable `<typ>_uvhnd`).

After running any uv function returning a status, you'll likely want to use `kk_uv_check_bail_drops(status, {kk_box_drop(timer, _ctx);})`. You'll need to carefully account for all structures allocated so far in the function, and drop them all in the second argument (it's a C block).

To store the (koka) callback, use `kk_assign_uv_callback(timer, callback, _ctx);`. This will fail (assert) if there's a concurrent operation using the same handle.

Then trigger the uv operation, e.g. `int status = uv_timer_start(timer_uvhnd, kk_uv_timer_unit_callback, timeout, 0);`

Use one of the appropriate `kk_uv_check_return_*` macros to return the appropriate value.

In the (C) callback function, you can invoke the stored koka callback via e.g. `kk_resolve_uv_callback((uv_handle_t*)uv_timer, NULL, kk_get_context(););`

You can pass a non-NULL value if the callback accepts an argument.

This will invoke the callback, drop the captured reference to the handle, and reset the handle's `data` to `NULL`.

At the end of the function, you can use `kk_uv_check_return_drops`, although using the `bail` pattern and then just returning `kk_std_core_exn__new_Ok(result, _ctx)` is more consistent.

### Details for specific design decisions for special cases:
-- TODO 

### Details for multi-threading / multi-processing:
-- TODO

### Resource cleanup:

Long-lived handles are reference counted like any other koka value.

Each callback relating to a handle has a reference to the handle, which is dropped after the callback fires. This ensures a resource stays live as long as there are any pending callbacks for it.

uv requires that raw handles remain live until after the handle is closed, _and_ the close callback has been invoked. So the sequence of events for destruction is:
 - reference count drops to zero (no remaining references or pending callbacks)
 - the koka finalizer (kk_uv_free_handle) runs, calling close()
 - the on_close callback is invoked by libuv, and memory is deallocated

Note that due to `close` being part of an object's destruction, it's not possible to _also_ explicitly close a UV handle. If you need to dynamically close a resource while others may still be referencing it, you could place it behind an opaque `ref` to ensure the ref holds the only copy, and then replace it with e.g. `Nothing` to drop the resource.

You can also use scoped references, to ensure a handle can't outlive the scope.

// TODO when is koka guaranteed to drop an object?
