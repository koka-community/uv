
#include "kklib/box.h"
#include "uv_event_dash_loop.h"

static kk_uv_file__fstat kk_uv_stat_from_uv_stat(uv_stat_t* uvstat, kk_context_t* _ctx) {
  return kk_uv_file__new_Fstat(
    kk_reuse_null,
    0, // cpath
    uvstat->st_dev,
    uvstat->st_mode,
    uvstat->st_nlink,
    uvstat->st_uid,
    uvstat->st_gid,
    uvstat->st_rdev,
    uvstat->st_ino,
    uvstat->st_size,
    uvstat->st_blksize,
    uvstat->st_blocks,
    uvstat->st_flags,
    kk_uv_file__new_Timespec(uvstat->st_atim.tv_sec, uvstat->st_atim.tv_nsec, _ctx),
    kk_uv_file__new_Timespec(uvstat->st_mtim.tv_sec, uvstat->st_mtim.tv_nsec, _ctx),
    kk_uv_file__new_Timespec(uvstat->st_ctim.tv_sec, uvstat->st_ctim.tv_nsec, _ctx),
    kk_uv_file__new_Timespec(uvstat->st_birthtim.tv_sec, uvstat->st_birthtim.tv_nsec, _ctx),
    _ctx
  );
}


// shortcut for operations that act on a path string
#define kk_uv_oneshot_fs_setup_string(path, uv_fn, uv_cb, ...) \
  kk_uv_oneshot_fs_setup(cb, NULL_BYTES, uv_fn, uv_cb, \
    { kk_string_drop(path, _ctx); }, \
    kk_string_cbuf_borrow(path, NULL, _ctx) __VA_OPT__(,) __VA_ARGS__);

// shortcut for operations that act on two path strings
#define kk_uv_oneshot_fs_setup_string_string(path1, path2, uv_fn, uv_cb, ...) \
  kk_uv_oneshot_fs_setup(cb, NULL_BYTES, uv_fn, uv_cb, \
    { \
      kk_string_drop(path1, _ctx); \
      kk_string_drop(path2, _ctx); \
    }, \
    kk_string_cbuf_borrow(path1, NULL, _ctx), \
    kk_string_cbuf_borrow(path2, NULL, _ctx) __VA_OPT__(,) __VA_ARGS__);

// shortcut for operations that act on a file object
#define kk_uv_oneshot_fs_setup_file(file, uv_fn, uv_cb, ...) \
  kk_uv_oneshot_fs_setup(cb, NULL_BYTES, uv_fn, uv_cb, \
    {}, \
    (uv_file)file.internal __VA_OPT__(,) __VA_ARGS__);

static void kk_std_os_fs_unit_cb(uv_fs_t* req) {
  kk_uv_oneshot_fs_callback(req);
}

static void kk_uv_fs_close(kk_uv_file__uv_file file, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_file(file, uv_fs_close, kk_std_os_fs_unit_cb);
}

static void kk_std_os_file_open_cb(uv_fs_t* req) {
  kk_uv_oneshot_fs_callback1(req,
    kk_uv_file__uv_file_box(
      kk_uv_file__new_Uv_file((intptr_t)req->result, _ctx),
      _ctx)
    );
}

static void kk_uv_fs_open(kk_string_t path, int32_t flags, int32_t mode, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_string(path, uv_fs_open, kk_std_os_file_open_cb, flags, mode);
}

static void kk_std_os_file_buff_cb(uv_fs_t* req) {
  kk_uv_oneshot_fs_callback1(req,
    kk_std_core_types__tuple2_box(
      kk_std_core_types__new_Tuple2(
        kk_bytes_box(
          // copy the callback struct's bytes and adjust its length to match
          // the bytes read
          kk_bytes_adjust_length(
            kk_bytes_dup(
              ((kk_hnd_callback_t*)req->data)->bytes,
              _ctx
            ),
            (kk_ssize_t)req->result, _ctx
          )
        ),
        kk_integer_box(kk_integer_from_ssize_t(req->result, _ctx), _ctx), _ctx
      ),
    _ctx)
  );
}

static void kk_uv_fs_read(kk_uv_file__uv_file file, kk_bytes_t bytes, ssize_t offset, kk_function_t cb, kk_context_t* _ctx) {
  // Create a single uv buffer that points to the memory address of `bytes`
  uv_buf_t uv_buffs[1];
  uv_buffs[0].base = (char*)kk_bytes_cbuf_borrow(bytes, (kk_ssize_t*) &uv_buffs[0].len, _ctx);
  
  kk_uv_oneshot_fs_setup(cb, bytes, uv_fs_read, kk_std_os_file_buff_cb, {},
    // uv_read args
    (uv_file)file.internal, uv_buffs, 1, offset);
}

static void kk_uv_fs_unlink(kk_string_t path, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_string(path, uv_fs_unlink, kk_std_os_fs_unit_cb);
}

static void kk_std_os_fs_write_cb(uv_fs_t* req) {
  kk_uv_oneshot_fs_callback1(req, kk_integer_box(kk_integer_from_ssize_t(req->result, _ctx), _ctx));
}

static void kk_uv_fs_write(kk_uv_file__uv_file file, kk_bytes_t bytes, int64_t offset, kk_function_t cb, kk_context_t* _ctx) {
  uv_buf_t* uv_buffs = kk_bytes_to_uv_buffs(bytes, _ctx);
  kk_uv_oneshot_fs_setup(cb, NULL_BYTES, uv_fs_write, kk_std_os_fs_write_cb,
    { kk_bytes_drop(bytes, _ctx); },
    (uv_file)file.internal, uv_buffs, 1, offset);
}

static void kk_uv_fs_mkdir(kk_string_t path, int32_t mode, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_string(path, uv_fs_mkdir, kk_std_os_fs_unit_cb, mode);
}

static void kk_std_os_fs_mkdtemp_cb(uv_fs_t* req) {
  kk_uv_oneshot_fs_callback1(req,
    kk_string_box(kk_string_alloc_raw((const char*) req->path, true, _ctx))
  );
}

static void kk_uv_fs_mkdtemp(kk_string_t tpl, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_string(tpl, uv_fs_mkdtemp, kk_std_os_fs_mkdtemp_cb);
}

static void kk_std_os_fs_mkstemp_cb(uv_fs_t* req) {
  kk_uv_oneshot_fs_callback1(req,
    kk_std_core_types__tuple2_box(
      kk_std_core_types__new_Tuple2(
        kk_uv_file__uv_file_box(kk_uv_file__new_Uv_file((intptr_t)req->result, _ctx), _ctx),
        kk_string_box(kk_string_alloc_dup_valid_utf8((const char*) req->path, _ctx)),
      _ctx),
    _ctx)
  );
}

static void kk_uv_fs_mkstemp(kk_string_t tpl, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_string(tpl, uv_fs_mkstemp, kk_std_os_fs_mkstemp_cb);
}

static void kk_uv_fs_rmdir(kk_string_t path, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_string(path, uv_fs_rmdir, kk_std_os_fs_unit_cb);
}

void kk_std_os_fs_opendir_cb(uv_fs_t* req) {
  kk_uv_oneshot_fs_callback1(req,
    kk_uv_file__uv_dir_box(
      kk_uv_file__new_Uv_dir((intptr_t)req->ptr, _ctx),
    _ctx)
  );
}

static void kk_uv_fs_opendir(kk_string_t path, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_string(path, uv_fs_opendir, kk_std_os_fs_opendir_cb);
}

static void kk_uv_fs_closedir(kk_uv_file__uv_dir dir, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup(cb, NULL_BYTES, uv_fs_closedir, kk_std_os_fs_unit_cb, \
    {}, \
    (uv_dir_t*)dir.internal);
}

// TODO use handle instead?
typedef struct kk_uv_dir_callback_s {
  kk_function_t callback;
  uv_dir_t* uvdir;
} kk_uv_dir_callback_t;



void kk_free_dir(void* p, kk_block_t* block, kk_context_t* _ctx) {
  uv_dir_t* req = (uv_dir_t*)p;
  kk_free(req->dirents);
  kk_assert(req->data == NULL);
  kk_free(p, _ctx);
}

// TODO: inline, it's only used once
static inline kk_uv_dir_callback_t* kk_new_uv_dir_callback(kk_function_t cb, uv_handle_t* handle, uv_dir_t* uvdir, int32_t num_entries, kk_context_t* _ctx) {
  kk_uv_dir_callback_t* c = kk_malloc(sizeof(kk_uv_dir_callback_t), _ctx);
  c->callback = cb;
  uvdir->dirents = kk_malloc(sizeof(uv_dirent_t)*500, _ctx);
  uvdir->nentries = 500;
  uvdir->data = NULL;
  handle->data = c;
  return c;
}

kk_box_t kk_uv_dirent_to_dirent(uv_dirent_t* dirent, kk_box_t* loc, kk_context_t* _ctx) {
  if (loc == NULL) {
    loc = kk_malloc(sizeof(kk_box_t*), _ctx);
  }
  kk_string_t name = kk_string_alloc_raw((const char*) dirent->name, true, _ctx);
  kk_uv_file__dirent_type type;
  switch (dirent->type) {
    case UV_DIRENT_FILE: type = kk_uv_file__new_FILE(_ctx); break;
    case UV_DIRENT_DIR: type = kk_uv_file__new_DIR(_ctx); break;
    case UV_DIRENT_LINK: type = kk_uv_file__new_LINK(_ctx); break;
    case UV_DIRENT_FIFO: type = kk_uv_file__new_FIFO(_ctx); break;
    case UV_DIRENT_SOCKET: type = kk_uv_file__new_SOCKET(_ctx); break;
    case UV_DIRENT_CHAR: type = kk_uv_file__new_CHAR(_ctx); break;
    case UV_DIRENT_BLOCK: type = kk_uv_file__new_BLOCK(_ctx); break;
    default: type = kk_uv_file__new_UNKNOWN__DIRECTORY__ENTRY(_ctx); break;
  }
  kk_box_t box = kk_uv_file__dirent_box(kk_uv_file__new_Dirent(name, type, _ctx), _ctx);
  *loc = box;
  return box;
}

kk_vector_t kk_uv_dirents_to_vec(uv_dir_t* uvdir, kk_ssize_t num_entries, kk_context_t* _ctx) {
  kk_box_t* dirs;
  kk_vector_t dirents = kk_vector_alloc_uninit(num_entries, &dirs, _ctx);
  for (kk_ssize_t i = 0; i < num_entries; i++){
    kk_uv_dirent_to_dirent(&(uvdir->dirents[i]), &dirs[i], _ctx);
  }
  return dirents;
}

// TODO
void kk_std_os_fs_readdir_cb(uv_fs_t* req) {
  kk_context_t* _ctx = kk_get_context();
  kk_uv_dir_callback_t* wrapper = (kk_uv_dir_callback_t*)req->data;
  kk_function_t callback = wrapper->callback;
  ssize_t result = req->result;
  uv_dir_t* uvdir = wrapper->uvdir;
  kk_free(wrapper, _ctx);
  uv_fs_req_cleanup(req);
  if (result < 0) {
    kk_free(uvdir->dirents, _ctx);
    kk_uv_error_callback(callback, result)
  } else {
    kk_vector_t dirents = kk_uv_dirents_to_vec(uvdir, (kk_ssize_t)result, _ctx);
    kk_free(uvdir->dirents, _ctx);
    kk_uv_okay_callback(callback, kk_vector_box(dirents, _ctx))
  }
}

// TODO
static kk_unit_t kk_uv_fs_readdir(kk_uv_file__uv_dir dir, kk_function_t cb, kk_context_t* _ctx) {
  uv_fs_t* fs_req = kk_malloc(sizeof(uv_fs_t), _ctx);
  // Read up to 500 entries in the directory
  kk_uv_dir_callback_t* wrapper = kk_new_uv_dir_callback(cb, (uv_handle_t*)fs_req, (uv_dir_t*) dir.internal, 500, _ctx);
  uv_fs_readdir(uvloop(), fs_req, wrapper->uvdir, kk_std_os_fs_readdir_cb);
  return kk_Unit;
}

// TODO: remove this, single use is incorrect?
void kk_free_fs(void* p, kk_block_t* block, kk_context_t* _ctx) {
  uv_fs_t* req = (uv_fs_t*)p;
  // kk_uv_hnd_data_free(req);
  uv_fs_req_cleanup(req);
  kk_info_message("Freeing fs request\n", kk_context());
  kk_free(p, _ctx);
}

static void kk_std_os_fs_scandir_cb(uv_fs_t* req) {
  kk_uv_oneshot_fs_callback1(req,
    // TODO: This is not correct, we don't want a new reference counted box here
    uv_handle_to_owned_kk_handle_box(req, kk_free_fs, file, fs_req)
  );
}

static void kk_uv_fs_scandir(kk_string_t path, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_string(path, uv_fs_scandir, kk_std_os_fs_scandir_cb, 0);
}

static kk_std_core_exn__error kk_uv_fs_scandir_next(kk_uv_file__uv_fs_req req, kk_context_t* _ctx) {
  uv_fs_t* uvhnd = kk_owned_handle_to_uv_handle(uv_fs_t, req);
  uv_dirent_t ent = {0};
  int status = uv_fs_scandir_next(uvhnd, &ent);
  kk_uv_check_return(status, kk_uv_dirent_to_dirent(&ent, NULL, _ctx))
}

static void kk_std_os_fs_stat_cb(uv_fs_t* req) {
  kk_uv_oneshot_fs_callback1(req,
    kk_uv_file__fstat_box(kk_uv_stat_from_uv_stat(&req->statbuf, _ctx), _ctx)
  );
}

static void kk_uv_fs_stat(kk_string_t path, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_string(path, uv_fs_stat, kk_std_os_fs_stat_cb);
}

static void kk_uv_fs_fstat(kk_uv_file__uv_file file, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_file(file, uv_fs_fstat, kk_std_os_fs_stat_cb);
}

static void kk_uv_fs_lstat(kk_string_t path, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_string(path, uv_fs_lstat, kk_std_os_fs_stat_cb);
}

static void kk_uv_fs_rename(kk_string_t path, kk_string_t new_path, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_string_string(path, new_path, uv_fs_rename, kk_std_os_fs_unit_cb);
}

static void kk_uv_fs_fsync(kk_uv_file__uv_file file, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_file(file, uv_fs_fsync, kk_std_os_fs_unit_cb);
}

static void kk_uv_fs_fdatasync(kk_uv_file__uv_file file, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_file(file, uv_fs_fdatasync, kk_std_os_fs_unit_cb);
}

static void kk_uv_fs_ftruncate(kk_uv_file__uv_file file, int64_t offset, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_file(file, uv_fs_ftruncate, kk_std_os_fs_unit_cb, offset);
}

static void kk_uv_fs_copyfile(kk_string_t path, kk_string_t new_path, int32_t flags, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_string_string(path, new_path, uv_fs_copyfile, kk_std_os_fs_unit_cb, flags);
}

static void kk_std_os_fs_int_cb(uv_fs_t* req) {
  kk_uv_oneshot_fs_callback1(req, kk_integer_box(kk_integer_from_ssize_t(req->result, _ctx), _ctx));
}

// TOOD
static kk_unit_t kk_uv_fs_sendfile(kk_uv_file__uv_file out_fd, kk_uv_file__uv_file in_fd, int64_t in_offset, kk_ssize_t length, kk_function_t cb, kk_context_t* _ctx) {
  uv_fs_t* fs_req = kk_malloc(sizeof(uv_fs_t), _ctx);
  uv_buf_t buf = uv_buf_init(NULL, 0);
  fs_req->data = kk_function_as_ptr(cb, _ctx);
  uv_fs_sendfile(uvloop(), fs_req, (uv_file)out_fd.internal, (uv_file)in_fd.internal, in_offset, (size_t)length, kk_std_os_fs_int_cb);
  kk_uv_file__uv_file_drop(out_fd, _ctx);
  kk_uv_file__uv_file_drop(in_fd, _ctx);
  return kk_Unit;
}

static void kk_uv_fs_access(kk_string_t path, int32_t mode, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_string(path, uv_fs_access, kk_std_os_fs_unit_cb, mode);
}

static void kk_uv_fs_chmod(kk_string_t path, int32_t mode, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_string(path, uv_fs_chmod, kk_std_os_fs_unit_cb, mode);
}

static void kk_uv_fs_fchmod(kk_uv_file__uv_file file, int32_t mode, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_file(file, uv_fs_fchmod, kk_std_os_fs_unit_cb, mode);
}

static void kk_uv_fs_utime(kk_string_t path, double atime, double mtime, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_string(path, uv_fs_utime, kk_std_os_fs_unit_cb, atime, mtime);
}

static void kk_uv_fs_futime(kk_uv_file__uv_file file, double atime, double mtime, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_file(file, uv_fs_futime, kk_std_os_fs_unit_cb, atime, mtime);
}

static void kk_uv_fs_lutime(kk_string_t path, double atime, double mtime, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_string(path, uv_fs_lutime, kk_std_os_fs_unit_cb, atime, mtime);
}

static void kk_uv_fs_link(kk_string_t path, kk_string_t new_path, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_string_string(path, new_path, uv_fs_link, kk_std_os_fs_unit_cb);
}

static void kk_uv_fs_symlink(kk_string_t path, kk_string_t new_path, int32_t flags, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_string_string(path, new_path, uv_fs_symlink, kk_std_os_fs_unit_cb, flags);
}

void kk_std_os_fs_string_cb(uv_fs_t* req) {
  kk_uv_oneshot_fs_callback1(req, kk_string_box(kk_string_alloc_raw((const char*)req->ptr, true, _ctx)));
}

static void kk_uv_fs_readlink(kk_string_t path, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_string(path, uv_fs_readlink, kk_std_os_fs_string_cb);
}

static void kk_uv_fs_realpath(kk_string_t path, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_string(path, uv_fs_realpath, kk_std_os_fs_string_cb);
}

static void kk_uv_fs_chown(kk_string_t path, int32_t uid, int32_t gid, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_string(path, uv_fs_chown, kk_std_os_fs_unit_cb, uid, gid);
}

static void kk_uv_fs_fchown(kk_uv_file__uv_file file, int32_t uid, int32_t gid, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_file(file, uv_fs_fchown, kk_std_os_fs_unit_cb, uid, gid);
}

static void kk_uv_fs_lchown(kk_string_t path, int32_t uid, int32_t gid, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_string(path, uv_fs_lchown, kk_std_os_fs_unit_cb, uid, gid);
}
