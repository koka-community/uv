#include "uv_event_dash_loop.h"

#define free_readable_and_writable { kk_free(readable, _ctx); kk_free(writable, _ctx); }

static kk_std_core_exn__error kk_uv_pipe(kk_context_t* _ctx) {
  uv_file files[2];

  uv_pipe_t* readable = kk_malloc(sizeof(uv_pipe_t), _ctx);;
  uv_pipe_t* writable = kk_malloc(sizeof(uv_pipe_t), _ctx);

  int status = uv_pipe(files, 0, 0);
  kk_uv_check_bail_drops(status, free_readable_and_writable);

  status = uv_pipe_init(uvloop(), readable, 0);
  kk_uv_check_bail_drops(status, free_readable_and_writable);

  status = uv_pipe_open(readable, files[0]);
  kk_uv_check_bail_drops(status, free_readable_and_writable);

  status = uv_pipe_init(uvloop(), writable, 0);
  kk_uv_check_bail_drops(status, free_readable_and_writable);

  status = uv_pipe_open(writable, files[1]);
  kk_uv_check_bail_drops(status, free_readable_and_writable);

  kk_std_core_types__tuple2 result = kk_std_core_types__new_Tuple2(
    uv_handle_to_owned_kk_handle_box(readable, kk_free_fun, stream, stream),
    uv_handle_to_owned_kk_handle_box(writable, kk_free_fun, stream, stream),
    _ctx);

  return kk_std_core_exn__new_Ok(kk_std_core_types__tuple2_box(result, _ctx), _ctx);
}
