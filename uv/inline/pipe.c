declare_uv_struct(uv_pipe, kk_uv_free_fn);

static kk_uv_stream_t* kk_uv_pipe_as_stream(kk_uv_pipe_t* pipe) {
  // uv_pipe_t is a ‘subclass’ of uv_stream_t.
  return (kk_uv_stream_t*) pipe;
}

static kk_std_core_exn__error kk_uv_pipe(kk_context_t* _ctx) {
  uv_file files[2];

  int status = uv_pipe(files, 0, 0);
  if (status != UV_OK) {
    return kk_status_error(status, _ctx);
  }

  // readable
  malloc_and_init_handle(uv_pipe, readable, status, uvloop(), &readable->uv, 0);
  if (status == UV_OK) {
    status = uv_pipe_open(&readable->uv, files[0]);
  }

  if (status != UV_OK) {
    kk_uv_drop(kk_uv_pipe_as_handle(readable), _ctx);
    return kk_status_error(status, _ctx);
  }


  // writable
  malloc_and_init_handle(uv_pipe, writable, status, uvloop(), &writable->uv, 0);
  if (status == UV_OK) {
    status = uv_pipe_open(&writable->uv, files[1]);
  }

  if (status != UV_OK) {
    kk_uv_drop(kk_uv_pipe_as_handle(readable), _ctx);
    kk_uv_drop(kk_uv_pipe_as_handle(writable), _ctx);
    return kk_status_error(status, _ctx);
  }
  
  kk_uv_stream_t* readable_stream = kk_uv_pipe_as_stream(readable);
  kk_uv_stream_t* writable_stream = kk_uv_pipe_as_stream(writable);

  kk_std_core_types__tuple2 result = kk_std_core_types__new_Tuple2(
    kk_uv_stream_box(readable_stream, _ctx),
    kk_uv_stream_box(writable_stream, _ctx),
    _ctx);

  return kk_std_core_exn__new_Ok(kk_std_core_types__tuple2_box(result, _ctx), _ctx);
}
