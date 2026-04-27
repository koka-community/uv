declare_uv_handle(uv_process);

// make a copy of a string (and terminate it with a null byte)
// TODO: move into utils?
// TODO: I think strings are already null-terminated internally,
//       we can skip this function if that is a stable guarantee.
char* kk_string_cstr_alloc(kk_string_t str, kk_context_t* _ctx) {
  kk_ssize_t len;
  const char* slice = kk_string_cbuf_borrow(str, &len, _ctx);
  char* result = kk_malloc(len + 1, _ctx);
  memcpy(result, slice, len);
  result[len] = '\0';
  return result;
}

// Helper function to convert Koka list<string> to null-terminated C char** array.
// TODO: move into utils?
static char** kk_list_string_to_nt_carray_borrow(kk_std_core_types__list list, kk_context_t* _ctx) {
  list = kk_std_core_types__list_dup(list, _ctx);
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

// Note: `arr` must be null terminated
// TODO: move into utils?
static void kk_free_nt_carray(char** arr, kk_context_t* _ctx) {
  if (arr == NULL) return;
  char** current = arr;
  while(*current != NULL) {
    kk_free(*current, _ctx);
    current += 1;
  }
  kk_free(arr, _ctx);
}

static void kk_uv_process_exit_callback(uv_process_t* process, int64_t exit_status, int term_signal) {
  kk_context_t* _ctx = kk_get_context();
  kk_uv_process_t* kk_process = uv_process_as_kk(process);
  kk_uv_handle_t* kk_handle = kk_uv_process_as_handle(kk_process);
  kk_function_t callback = kk_uv_handle_take_callback(kk_uv_process_as_handle(kk_process), _ctx);
  kk_function_call(kk_unit_t, (kk_function_t, int64_t, int32_t, kk_context_t*),
                   callback,
                   (callback, exit_status, (int32_t)term_signal, _ctx),
                   _ctx);

  kk_uv_handle_drop_references(kk_handle, _ctx);
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
  } else if (kk_uv_process_is_stream_uv(stream, _ctx)) {
    /*
    * The child process will be given a duplicate of the parent's
    * file descriptor being used by the stream handle given by
    * `data.stream`.
    *
    * Note that uv/process does not keep these streams alive automatically,
    * however async/process does (they are put in `output-streams` and
    * closed on process exit).
    */
    result.flags = UV_INHERIT_STREAM;
    struct kk_uv_process_Stream_uv* stream_uv = kk_uv_process__as_Stream_uv(stream, _ctx);
    result.data.stream = &kk_uv_stream_unbox_borrowed(stream_uv->value.internal, _ctx)->uv;
  } else {
    kk_fatal_error(EINVAL, "unknown stream type\n");
  }
  return result;
}

// Note that the returned process MUST NOT be dropped until after the process has
// exited; that would result in use-after-free when UV callbacks are invoked.
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
  options.exit_cb = &kk_uv_process_exit_callback;

  // A process is really a handle, but it has no init() (spawn does that)
  malloc_req(uv_process, kk_process, on_complete);
  int status = uv_spawn(uvloop(), &kk_process->uv, &options);

  // free up options fields & drop command
  kk_free(options.file, _ctx);
  kk_free_nt_carray(options.args, _ctx);
  kk_uv_process__uv_command_drop(kk_command, _ctx);

  if (status < UV_OK) {
    // process was not initialized/spawned; free it (no close needed)
    kk_uv_handle_drop_references(kk_uv_process_as_handle(kk_process), _ctx);
    kk_free(kk_process, _ctx);
    return kk_uv_error(status, _ctx);
  } else {
    // A running process owns its own handle until it's exited.
    // so that if the user drops the handle before execution is complete, the handle won't be
    // freed before the process ends.
    return kk_std_core_types__new_Ok(
      kk_uv_process__uv_process_box(
        kk_uv_process__new_Uv_process(kk_uv_process_box(kk_process, _ctx), _ctx),
        _ctx),
      _ctx
    );
  }
}

static int kk_uv_proc_pid(kk_uv_process__uv_process process, kk_context_t* _ctx) {
  kk_uv_process_t* kk_process = kk_uv_process_unbox_borrowed(process.internal, _ctx);
  return kk_process->uv.pid;
}

static kk_std_core_exn__error kk_uv_proc_signal(kk_uv_process__uv_process process, int32_t kk_signal, kk_context_t* _ctx) {
  kk_uv_process_t* kk_process = kk_uv_process_unbox_borrowed(process.internal, _ctx);
  int status = uv_process_kill(&kk_process->uv, kk_signal);
  return kk_uv_error_or(status, kk_unit_box(kk_Unit), _ctx);
}

static kk_std_core_exn__error kk_uv_pid_signal(int32_t pid, int32_t kk_signal, kk_context_t* _ctx) {
  int status = uv_kill(pid, kk_signal);
  return kk_uv_error_or(status, kk_unit_box(kk_Unit), _ctx);
}
