
void kk_uv_alloc_callback(uv_handle_t* handle, size_t suggested_size, uv_buf_t* buf) {
  kk_context_t* _ctx = kk_get_context();
  kk_hnd_callback_t* uvcb = (kk_hnd_callback_t*)handle->data;
  
  // allocate a raw C buffer backing a kk_bytes struct,
  // and assign the kk_bytes struct to uvcb->bytes access in e.g. kk_uv_read_callback
  kk_hnd_callback_replace_bytes(&uvcb->bytes, kk_bytes_alloc_cbuf(suggested_size, &buf->base, _ctx), _ctx);
  buf->len = suggested_size;
}

static void kk_uv_shutdown_callback(uv_shutdown_t* req, int status){
  kk_uv_oneshot_req_callback(req, status);
}

static void kk_uv_shutdown(kk_uv_stream__uv_stream handle, kk_function_t callback, kk_context_t* _ctx){
  kk_uv_oneshot_req_setup(callback,
    uv_shutdown_t, uv_shutdown, uv_stream_t, handle, kk_uv_shutdown_callback);
}

static void kk_uv_connection_callback(uv_stream_t* server, int status){
  kk_uv_oneshot_hnd_callback(server, status);
}

static void kk_uv_listen(kk_uv_stream__uv_stream stream, int32_t backlog, kk_function_t callback, kk_context_t* _ctx){
  uv_stream_t* uvstream = kk_owned_handle_to_uv_handle(uv_stream_t, stream);
  int status = kk_uv_hnd_data_create(stream.internal, callback, _ctx);
  if (status == UV_OK) {
    status = uv_listen(uvstream, backlog, kk_uv_connection_callback);
  }
  if (status != UV_OK) {
    callback = kk_uv_hnd_wrapper_data_take_cb(stream);
    kk_uv_status_code_callback(callback, status);
  }
}

static kk_uv_status_code_t kk_uv_accept(kk_uv_stream__uv_stream server, kk_uv_stream__uv_stream client, kk_context_t* _ctx) {
  return kk_uv_status_code(
    uv_accept(kk_owned_handle_to_uv_handle(uv_stream_t,server), kk_owned_handle_to_uv_handle(uv_stream_t, client))
  );
}

static void kk_uv_read_callback(uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf){
  kk_context_t* _ctx = kk_get_context();
  kk_hnd_callback_t* hndcb = (kk_hnd_callback_t*)stream->data;

  // callback is either reused or still required after we drop hndcb
  kk_function_t callback = kk_function_dup(hndcb->callback, kk_context());

  kk_std_core_exn__error result;
  if (nread < 0) {
    if (nread == UV_EOF) {
      result = kk_std_core_exn__new_Ok(
        kk_std_core_types__maybe_box(
          kk_std_core_types__new_Nothing(_ctx),
          _ctx),
        _ctx);
    } else {
      result = kk_async_error_from_errno(nread, _ctx);
    }

    kk_uv_hnd_data_free(stream);
  } else {
    hndcb->bytes = kk_bytes_adjust_length(hndcb->bytes, (kk_ssize_t)nread, _ctx);

    result = kk_std_core_exn__new_Ok(
      kk_std_core_types__maybe_box(
        kk_std_core_types__new_Just(
          kk_bytes_box(hndcb->bytes),
        _ctx),
      _ctx),
    _ctx);
    hndcb->bytes = kk_bytes_empty(); // bytes are given to CB, replace with empty singleton
  }
  kk_function_call(void, (kk_function_t, kk_std_core_exn__error, kk_context_t*), callback, (callback, result, _ctx), _ctx);
}

static kk_uv_status_code_t kk_uv_read_start(kk_uv_stream__uv_stream stream, kk_function_t read_cb, kk_context_t* _ctx){
  int status = kk_uv_hnd_data_create(stream.internal, read_cb, _ctx);
  if (status != UV_OK) {
    return kk_uv_status_code(status);
  }

  uv_stream_t* uvstream = kk_owned_handle_to_uv_handle(uv_stream_t, stream);
  return kk_uv_status_code(
    uv_read_start(uvstream, kk_uv_alloc_callback, kk_uv_read_callback)
  );
}

static kk_uv_status_code_t kk_uv_read_stop(kk_uv_stream__uv_stream stream, kk_context_t* _ctx){
  kk_uv_hnd_cancel_return(uv_stream_t, stream, uv_read_stop);
}

static void kk_uv_write_callback(uv_write_t* write, int status){
  // TODO: free the buffers we allocated. Currently not accessible via `write` data
  kk_uv_oneshot_req_callback(write, status);
}

static void kk_uv_write(kk_uv_stream__uv_stream stream, kk_std_core_types__list buffs, kk_function_t cb, kk_context_t* _ctx){
  int list_len;
  const uv_buf_t* uv_buffs = kk_bytes_list_to_uv_buffs(buffs, &list_len, _ctx);
  
  // TODO: persist bufs somewhere the handler can free them
  kk_uv_oneshot_req_setup(cb,
    uv_write_t, uv_write,
    uv_stream_t, stream, uv_buffs, list_len, kk_uv_write_callback
  )
}
