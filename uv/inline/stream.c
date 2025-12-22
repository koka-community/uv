declare_uv_req(uv_write);

void kk_uv_alloc_callback(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  kk_context_t* _ctx = kk_get_context();

  // allocate a raw C buffer backing a kk_bytes struct.
  // Always allocates one more byte than requested, for a null byte
  // TODO: is this necessary? should bytes->clen refer to the *real* length?
  kk_ssize_t full_size = suggested_size + 1;
  buf->base = kk_malloc(full_size, _ctx);
  buf->len = suggested_size;
  kk_memset(&buf->base, 0, full_size);
}

static void kk_uv_read_callback(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf){
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
    callback = kk_uv_handle_take_callback(kk_hnd); // not needed any more
    if (nread == UV_EOF) {
      result = kk_std_core_exn__new_Ok(
        kk_std_core_types__maybe_box(
          kk_std_core_types__new_Nothing(_ctx),
          _ctx),
        _ctx);
    } else {
      result = kk_status_error(nread, _ctx);
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
    kk_uv_error_status_callback(callback, status, _ctx);
  }

  status = uv_read_start(&stream->uv, kk_uv_alloc_callback, kk_uv_read_callback);
  if (status != UV_OK) {
    kk_uv_error_status_callback(kk_uv_handle_take_callback(kk_hnd), status, _ctx);
  }
}

static kk_uv_status_code_t kk_uv_read_stop(kk_uv_stream__uv_stream stream_s, kk_context_t* _ctx){
  kk_uv_stream_t* stream = kk_uv_stream_unbox_borrowed(stream_s.internal, _ctx);
  kk_uv_handle_t* kk_hnd = kk_uv_stream_as_handle(stream);

  int status = uv_read_stop(&stream->uv);
  // remove cb & bytes if set
  kk_uv_handle_drop_contents(kk_hnd, _ctx);
  return kk_uv_status_code(status, _ctx);
}

static void kk_uv_write_callback(uv_write_t* write, int status){
  kk_context_t* _ctx = kk_get_context();
  kk_uv_req_t* kk_hnd = uv_write_as_kk_req(write);
  kk_function_t callback = kk_uv_req_take_callback(kk_hnd);

  // free the input data and the request
  kk_uv_bufs_t* bufs = (kk_uv_bufs_t*)write->data;
  kk_uv_bufs_drop(bufs, _ctx);
  kk_warning_message("dropping write %p after status %d\n", kk_hnd, status);
  kk_uv_req_drop(kk_hnd, _ctx);

  kk_status_code_callback(callback, status, _ctx);
}

// Stores `kk_uv_bufs*` in write->data, freed by `kk_uv_write_callback`
static void kk_uv_write(kk_uv_stream__uv_stream stream, kk_std_core_types__vector bufs, kk_function_t cb, kk_context_t* _ctx){
  malloc_req(uv_write, write, cb);
  kk_uv_req_t* uv_hnd = kk_uv_write_as_req(write);

  kk_ssize_t num_bufs;
  kk_uv_bufs_t* kk_uv_bufs = kk_bytes_vec_to_uv_bufs(bufs, &num_bufs, _ctx);
  write->uv.data = (void*) kk_uv_bufs;

  kk_uv_stream_t* uv_stream = kk_uv_stream_unbox_borrowed(stream.internal, _ctx);
  int status = uv_write(&write->uv, &uv_stream->uv, kk_uv_bufs->uv_bufs, num_bufs, kk_uv_write_callback);
  if (status != UV_OK) {
    kk_warning_message("uv_write returned %d\n", status);
    kk_uv_write_callback(&write->uv, status);
  }
}
