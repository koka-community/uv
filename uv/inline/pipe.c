declare_uv_handle(uv_pipe);

static int kk_uv_attach_pipe(uv_file file, kk_uv_pipe_t** dest, kk_context_t* _ctx) {
  int status;
  malloc_and_init_handle(uv_pipe, pipe, status, uvloop(), &pipe->uv, 0);
  if (status != UV_OK) {
    return status;
  }

  kk_uv_handle_t* hnd = kk_uv_pipe_as_handle(pipe);
  status = uv_pipe_open(&pipe->uv, file);
  
  if (status != UV_OK) {
    kk_uv_handle_drop_references(hnd, _ctx);
    return status;
  }
  *dest = pipe;
  return UV_OK;
}


static kk_std_core_exn__error kk_uv_pipe(kk_context_t* _ctx) {
  uv_file files[2];

  int status = uv_pipe(files, 0, 0);
  if (status != UV_OK) {
    return kk_status_error(status, _ctx);
  }

  kk_uv_pipe_t* readable;
  kk_uv_pipe_t* writable;

  // readable
  kk_uv_attach_pipe(files[0], &readable, _ctx);
  kk_warning_message("readable attaching to fd %d\n", (int)files[0]);
  if (status != UV_OK) {
    return kk_status_error(status, _ctx);
  }
  kk_uv_handle_t* readable_hnd = kk_uv_pipe_as_handle(readable);

  // writable
  kk_uv_attach_pipe(files[1], &writable, _ctx);
  kk_warning_message("writable attaching to fd %d\n", (int)files[1]);
  if (status != UV_OK) {
    kk_uv_handle_drop_references(readable_hnd, _ctx);
    return kk_status_error(status, _ctx);
  }
  // kk_uv_handle_t* writable_hnd = kk_uv_pipe_as_handle(writable);

  kk_std_core_types__tuple2 result = kk_std_core_types__new_Tuple2(
    // kk_uv_stream__uv_stream_box(kk_uv_stream__new_Uv_stream(kk_uv_handle_dup_box(readable_hnd, _ctx), _ctx), _ctx),
    // kk_uv_stream__uv_stream_box(kk_uv_stream__new_Uv_stream(kk_uv_handle_dup_box(writable_hnd, _ctx), _ctx), _ctx),
    kk_uv_stream__uv_stream_box(kk_uv_stream__new_Uv_stream(kk_uv_pipe_box(readable, _ctx), _ctx), _ctx),
    kk_uv_stream__uv_stream_box(kk_uv_stream__new_Uv_stream(kk_uv_pipe_box(writable, _ctx), _ctx), _ctx),
    _ctx);

  return kk_std_core_exn__new_Ok(kk_std_core_types__tuple2_box(result, _ctx), _ctx);
}
