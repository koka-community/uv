#include "uv_process.h"

#define kk_unbox_process_borrowed(kk_process) \
  (kk_uv_process_wrapper_t*)kk_cptr_raw_unbox_borrowed(kk_process.internal, _ctx)

typedef struct kk_uv_process_wrapper_s {
  uv_process_t process;
  kk_function_t on_exit;
  kk_uv_process__uv_process kprocess; // the boxed version of this struct, required to correctly drop
} kk_uv_process_wrapper_t;

// TODO: move into utils?
// make a copy of a string (and terminate it with a null byte)
char* kk_string_cstr_alloc(kk_string_t str, kk_context_t* _ctx) {
  kk_ssize_t len;
  const char* slice = kk_string_cbuf_borrow(str, &len, _ctx);
  char* result = kk_malloc(len + 1, _ctx);
  memcpy(result, slice, len);
  result[len] = '\0';
  return result;
}

// TODO: move into utils?
// Helper function to convert Koka list<string> to null-terminated C char** array.
static char** kk_list_string_to_nt_carray_borrow(kk_std_core_types__list list, kk_context_t* _ctx) {
  kk_std_core_types__list_dup(list, _ctx);
  kk_integer_t klist_len = kk_std_core_list_length(list, _ctx);
  int list_len = kk_integer_clamp32(klist_len, _ctx);

  char** arr = (char**)kk_malloc((list_len + 1) * sizeof(char*), _ctx);

  kk_std_core_types__list current = list;
  for (int i = 0; i < list_len; i++){
    struct kk_std_core_types_Cons* cons = kk_std_core_types__as_Cons(current, _ctx);
    kk_string_t str = kk_string_unbox(cons->head);
    arr[i] = kk_string_cstr_alloc(str, _ctx);
    current = cons->tail;
  }
  arr[list_len] = NULL;
  return arr;
}

// TODO: move into utils?
// Note: `arr` must be null terminated
static void kk_free_nt_carray(char** arr, kk_context_t* _ctx) {
  if (arr == NULL) return;
  char** current = arr;
  while(*current != NULL) {
    kk_free(*current, _ctx);
    current += 1;
  }
  kk_free(arr, _ctx);
}

void kk_uv_process_wrapper_free(void *p, kk_block_t *block, kk_context_t *_ctx) {
  kk_uv_process_wrapper_t* wrapper = (kk_uv_process_wrapper_t*)p;
  kk_function_drop(wrapper->on_exit, _ctx);
  kk_free(wrapper, _ctx);
}

static void kk_uv_process_on_close(uv_handle_t* process) {
  kk_uv_process_wrapper_t* wrapper = (kk_uv_process_wrapper_t*)process;
  kk_uv_process__uv_process_drop(wrapper->kprocess, kk_get_context());
}

static void kk_uv_process_exit_callback(uv_process_t* process, int64_t exit_status, int term_signal) {
  kk_context_t* _ctx = kk_get_context();
  kk_uv_process_wrapper_t* wrapper = (kk_uv_process_wrapper_t*)process;
  kk_function_call(kk_unit_t, (kk_function_t, int64_t, int32_t, kk_context_t*),
                   wrapper->on_exit,
                   (wrapper->on_exit, exit_status, (int32_t)term_signal, _ctx),
                   _ctx);

  uv_close((uv_handle_t*)process, kk_uv_process_on_close);
}

static uv_stdio_container_t kk_convert_to_stdio_container(
  kk_uv_process__stdio_stream stream,
  kk_context_t* _ctx
) {
  uv_stdio_container_t result = {0};
  if (kk_uv_process_is_stream_ignore(stream, _ctx)) {
    result.flags = UV_IGNORE;
  } else if (kk_uv_process_is_stream_fd(stream, _ctx)) {
    result.flags = UV_INHERIT_FD;
    struct kk_uv_process_Stream_fd* stream_fd = kk_uv_process__as_Stream_fd(stream, _ctx);
    result.data.fd = stream_fd->fd;
  } else {
    kk_fatal_error(EINVAL, "unknown stream type\n");
  }
  return result;
}

static kk_std_core_exn__error kk_uv_proc_spawn_c(
  kk_uv_process__uv_command kk_command,
  kk_function_t on_complete,
  kk_context_t* _ctx
) {
  struct kk_uv_process_Uv_command* command = kk_uv_process__as_Uv_command(kk_command, _ctx);

  uv_process_options_t options = {0};

  kk_ssize_t file_len;
  options.file = kk_string_cstr_alloc(command->file, _ctx);

  options.args = kk_list_string_to_nt_carray_borrow(command->args, _ctx);
  
  options.stdio_count = 3;
  uv_stdio_container_t child_stdio[3];
  child_stdio[0] = kk_convert_to_stdio_container(command->stdin, _ctx);
  child_stdio[1] = kk_convert_to_stdio_container(command->stdout, _ctx);
  child_stdio[2] = kk_convert_to_stdio_container(command->stderr, _ctx);
  options.stdio = child_stdio;

  kk_uv_process_wrapper_t* wrapper = kk_malloc(sizeof(kk_uv_process_wrapper_t), _ctx);
  wrapper->on_exit = on_complete; // wrapper now owns the cb
  options.exit_cb = &kk_uv_process_exit_callback;

  uv_process_t* process = &(wrapper->process); // type-safe noop

  int status = uv_spawn(uvloop(), process, &options);

  // free up options fields & drop command
  kk_free(options.file, _ctx);
  kk_free_nt_carray(options.args, _ctx);
  kk_uv_process__uv_command_drop(kk_command, _ctx);

  // box the wrapper pointer with custom free, and pass it as the value to Uv-process(...) constructor
  // uses kk_cptr_raw_box on the pointer
  //
  // A running process owns its own handle until it's ended + closed. We return a dup
  // so that if the user drops the handle before execution is complete, the handle won't be
  // freed before the process ends.
  kk_uv_process__uv_process kprocess = uv_handle_to_owned_kk_handle(wrapper, kk_uv_process_wrapper_free, process, process);
  wrapper->kprocess = kprocess;

  if (status < UV_OK) {
    kk_uv_process__uv_process_drop(kprocess, _ctx);
    return kk_async_error_from_errno(status, _ctx);
  } else {
    return kk_std_core_exn__new_Ok(
      kk_uv_process__uv_process_box(kk_uv_process__uv_process_dup(kprocess, _ctx), _ctx),
      _ctx);
  }
}

static int kk_uv_proc_pid(kk_uv_process__uv_process process, kk_context_t* _ctx) {
  kk_uv_process_wrapper_t* wrapper = kk_unbox_process_borrowed(process);
  return wrapper->process.pid;
}

static kk_std_core_exn__error kk_uv_proc_signal(kk_uv_process__uv_process process, int32_t kk_signal, kk_context_t* _ctx) {
  kk_uv_process_wrapper_t* wrapper = kk_unbox_process_borrowed(process);
  int status = uv_process_kill(&wrapper->process, kk_signal);
  kk_uv_check_return(status, kk_unit_box(kk_Unit));
}
