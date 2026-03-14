declare_uv_req(uv_write);
declare_uv_req(uv_shutdown);
declare_uv_req(uv_connect);

void kk_uv_alloc_callback(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  kk_context_t* _ctx = kk_get_context();

  // allocate a raw C buffer backing a kk_bytes struct.
  // The underlying buffer will contain one more than the size we report back to
  // libuv.
  // TODO: is this necessary? should bytes->clen refer to the *real* length?
  if (suggested_size >= 1024) {
    // libuv usually allocates 64kb at once, it probably doesn't need 64kb + 1b.
    suggested_size--;
  }
  kk_ssize_t full_size = suggested_size + 1;
  buf->base = kk_malloc(full_size, _ctx);
  buf->len = suggested_size;
  kk_memset(buf->base, 0, full_size);
}

static void kk_uv_read_callback(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf){
  // kk_warning_message("kk_uv_read_callback: nread=%d, stream=%p buf=%p\n", nread, stream, buf);
  kk_context_t* _ctx = kk_get_context();
  kk_uv_handle_t* kk_hnd = uv_stream_as_kk_handle(stream);

  kk_bytes_t bytes;
  if (buf->base == NULL) {
    bytes = kk_bytes_empty();
  } else {
    // wrap the full allocated buffer. There's always 1 more byte than we tell UV about,
    // for the terminating NULL
    bytes = kk_bytes_alloc_raw_len(buf->len + 1, (uint8_t*)buf->base, true /* free on drop */, _ctx);
  }

  kk_function_t callback;
  kk_std_core_exn__error result;
  if (nread < 0) {
    callback = kk_uv_handle_take_callback(kk_hnd, _ctx); // read_cb won't be called again after an error
    if (nread == UV_EOF) {
      result = kk_std_core_exn__new_Ok(
        kk_std_core_types__maybe_box(
          kk_std_core_types__new_Nothing(_ctx),
          _ctx),
        _ctx);
    } else {
      result = kk_uv_error(nread, _ctx);
    }
    kk_bytes_drop(bytes, _ctx);
  } else {
    callback = kk_uv_handle_dup_callback(kk_hnd, _ctx); // needed for subsequent reads

    // shrink bytes if necessary, and return via callback
    bytes = kk_bytes_adjust_length(bytes, (kk_ssize_t)nread, _ctx);
    result = kk_std_core_exn__new_Ok(
      kk_std_core_types__maybe_box(
        kk_std_core_types__new_Just(
          kk_bytes_box(bytes),
        _ctx),
      _ctx),
    _ctx);
  }

  kk_uv_error_callback(callback, result, _ctx);
}

static void kk_uv_read_start(kk_uv_stream__uv_stream stream_s, kk_function_t callback, kk_context_t* _ctx){
  kk_uv_stream_t* stream = kk_uv_stream_unbox_borrowed(stream_s.internal, _ctx);
  kk_uv_handle_t* kk_hnd = kk_uv_stream_as_handle(stream);

  int status = kk_uv_handle_try_set_callback(kk_hnd, callback, _ctx);
  if (status != UV_OK) {
    return kk_uv_error_status_callback(callback, status, _ctx);
  }

  status = uv_read_start(&stream->uv, kk_uv_alloc_callback, kk_uv_read_callback);
  if (status != UV_OK) {
    return kk_uv_error_status_callback(kk_uv_handle_take_callback(kk_hnd, _ctx), status, _ctx);
  }
}

static kk_uv_status_code_t kk_uv_read_stop(kk_uv_stream__uv_stream stream_s, kk_context_t* _ctx){
  kk_uv_stream_t* stream = kk_uv_stream_unbox_borrowed(stream_s.internal, _ctx);
  kk_uv_handle_t* kk_hnd = kk_uv_stream_as_handle(stream);

  int status = uv_read_stop(&stream->uv);
  if (has_callback(kk_uv_handle_as_any(kk_hnd), _ctx)) {
    // After an error or EOF the callback will have been removed,
    // but on cancelation it might not have been cleanly removed.
    //
    // TODO: Docs say it won't be called again, but
    // should we invoke it with a "STOPPED" sentinel somehow? Maybe ECANCEL?
    kk_function_drop(kk_uv_handle_take_callback(kk_hnd, _ctx), _ctx);
  }
  return kk_uv_status_code(status, _ctx);
}

// TODO remove this debugging hack
static void* stream_g;

static void kk_uv_write_callback(uv_write_t* write, int status){
  kk_context_t* _ctx = kk_get_context();
  kk_uv_write_t* kk_uv_write = uv_write_as_kk(write);
  kk_uv_any_t* kk_any = kk_uv_write_as_any(kk_uv_write);
  kk_function_t callback = kk_uv_any_take_callback(kk_any, _ctx);

  // free the input data and the request
  kk_uv_bufs_t* bufs = (kk_uv_bufs_t*)write->data;
  kk_uv_bufs_drop(bufs, _ctx);
  // kk_warning_message("[kk_uv_write_callback] dropping write %p after status %d\n", kk_hnd, status);

  // kk_warning_message("[kk_uv_write_callback] stream(%p) refcount = %d\n",
  //     stream_g,
  //   kk_block_refcount(stream_g)
  // );

  kk_uv_req_free(kk_any, _ctx);

  kk_uv_status_code_callback(callback, status, _ctx);
}

// Stores `kk_uv_bufs*` in write->data, freed by `kk_uv_write_callback`
static void kk_uv_write(kk_uv_stream__uv_stream stream, kk_std_core_types__vector bufs, kk_function_t cb, kk_context_t* _ctx){
  malloc_req(uv_write, write, cb);
  kk_uv_any_t* uv_hnd = kk_uv_write_as_any(write);

  kk_ssize_t num_bufs;
  kk_uv_bufs_t* kk_uv_bufs = kk_bytes_vec_to_uv_bufs(bufs, &num_bufs, _ctx);
  write->uv.data = (void*) kk_uv_bufs;

  stream_g = kk_box_to_ptr(stream.internal, _ctx);
  // kk_warning_message("uv_write start, stream (%p) refcount = %d\n",
  //   stream_g,
  //   kk_block_refcount(kk_box_to_ptr(stream.internal, kk_context()))
  // );

  kk_uv_stream_t* uv_stream = kk_uv_stream_unbox_borrowed(stream.internal, _ctx);
  int status = uv_write(&write->uv, &uv_stream->uv, kk_uv_bufs->uv_bufs, num_bufs, kk_uv_write_callback);
  // kk_warning_message("uv_write returned %d\n", status);
  if (status != UV_OK) {
    kk_uv_bufs_drop(kk_uv_bufs, _ctx);
    cb = kk_uv_any_take_callback(kk_uv_write_as_any(write), _ctx);
    kk_free(write, _ctx);
    kk_uv_status_code_callback(cb, status, _ctx);
  }
}

static void kk_uv_connection_callback(uv_stream_t* uv_stream, int status){
  kk_context_t* _ctx = kk_get_context();
  kk_uv_stream_t* kk_stream = uv_stream_as_kk(uv_stream);
  kk_function_t callback = kk_uv_any_take_callback(kk_uv_stream_as_any(kk_stream), _ctx);
  kk_uv_status_code_callback(callback, status, _ctx);
}

static void kk_uv_listen(kk_uv_stream__uv_stream stream, int32_t backlog, kk_function_t callback, kk_context_t* _ctx){
  kk_uv_stream_t* kk_stream = kk_uv_stream_unbox_borrowed(stream.internal, _ctx);
  kk_uv_handle_t* kk_handle = kk_uv_stream_as_handle(kk_stream);
  int status = kk_uv_handle_try_set_callback(kk_handle, callback, _ctx);
  if (status == UV_OK) {
    status = uv_listen(&kk_stream->uv, backlog, kk_uv_connection_callback);
  }
  if (status != UV_OK) {
    callback = kk_uv_handle_take_callback(kk_handle, _ctx);
    kk_uv_status_code_callback(callback, status, _ctx);
  }
}

static kk_uv_status_code_t kk_uv_accept(kk_uv_stream__uv_stream server, kk_uv_stream__uv_stream client, kk_context_t* _ctx) {
  kk_uv_stream_t* kk_server = kk_uv_stream_unbox_borrowed(server.internal, _ctx);
  kk_uv_stream_t* kk_client = kk_uv_stream_unbox_borrowed(client.internal, _ctx);
  return kk_uv_status_code(
    uv_accept(&kk_server->uv, &kk_client->uv),
    _ctx
  );
}

static void kk_uv_shutdown_callback(uv_shutdown_t* uv_shutdown, int status){
  kk_context_t* _ctx = kk_get_context();
  kk_uv_shutdown_t* kk_shutdown = uv_shutdown_as_kk(uv_shutdown);
  kk_uv_any_t* kk_req = kk_uv_shutdown_as_any(kk_shutdown);
  kk_function_t callback = kk_uv_any_take_callback(kk_req, _ctx);
  kk_uv_req_free(kk_req, _ctx);
  kk_uv_status_code_callback(callback, status, _ctx);
}

static void kk_uv_shutdown(kk_uv_stream__uv_stream stream, kk_function_t callback, kk_context_t* _ctx){
  kk_uv_stream_t* kk_stream = kk_uv_stream_unbox_borrowed(stream.internal, _ctx);
  malloc_req(uv_shutdown, kk_shutdown, callback);
  uv_shutdown(&kk_shutdown->uv, &kk_stream->uv, kk_uv_shutdown_callback);
}
