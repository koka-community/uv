
#include "kklib/box.h"

// static kk_uv_file__fstat kk_uv_stat_from_uv_stat(uv_stat_t* uvstat, kk_context_t* _ctx) {
//   return kk_uv_file__new_Fstat(
//     kk_reuse_null,
//     0, // cpath
//     uvstat->st_dev,
//     uvstat->st_mode,
//     uvstat->st_nlink,
//     uvstat->st_uid,
//     uvstat->st_gid,
//     uvstat->st_rdev,
//     uvstat->st_ino,
//     uvstat->st_size,
//     uvstat->st_blksize,
//     uvstat->st_blocks,
//     uvstat->st_flags,
//     kk_uv_file__new_Timespec(uvstat->st_atim.tv_sec, uvstat->st_atim.tv_nsec, _ctx),
//     kk_uv_file__new_Timespec(uvstat->st_mtim.tv_sec, uvstat->st_mtim.tv_nsec, _ctx),
//     kk_uv_file__new_Timespec(uvstat->st_ctim.tv_sec, uvstat->st_ctim.tv_nsec, _ctx),
//     kk_uv_file__new_Timespec(uvstat->st_birthtim.tv_sec, uvstat->st_birthtim.tv_nsec, _ctx),
//     _ctx
//   );
// }

#define kk_uv_oneshot_fs_setup(cb, bytes, uv_setup_fn, uv_callback_fn, drops, ...) \
  malloc_req(uv_fs, req, cb); \
  int status = uv_setup_fn(uvloop(), &req->uv __VA_OPT__(,) __VA_ARGS__, uv_callback_fn); \
  do drops while (0); \
  if (status != UV_OK) { \
    req->uv.result = status; \
    uv_callback_fn(&req->uv); \
  }

// shortcut for operations that act on a path string
#define kk_uv_oneshot_fs_setup_string(path, uv_fn, uv_cb, ...) \
  kk_uv_oneshot_fs_setup(cb, NULL_BYTES, uv_fn, uv_cb, \
    { kk_string_drop(path, _ctx); }, \
    kk_string_cbuf_borrow(path, NULL, _ctx) __VA_OPT__(,) __VA_ARGS__);

// // shortcut for operations that act on two path strings
// #define kk_uv_oneshot_fs_setup_string_string(path1, path2, uv_fn, uv_cb, ...) \
//   kk_uv_oneshot_fs_setup(cb, NULL_BYTES, uv_fn, uv_cb, \
//     { \
//       kk_string_drop(path1, _ctx); \
//       kk_string_drop(path2, _ctx); \
//     }, \
//     kk_string_cbuf_borrow(path1, NULL, _ctx), \
//     kk_string_cbuf_borrow(path2, NULL, _ctx) __VA_OPT__(,) __VA_ARGS__);

// shortcut for operations that act on a file object
#define kk_uv_oneshot_fs_setup_file(file, uv_fn, uv_cb, ...) \
  kk_uv_oneshot_fs_setup(cb, NULL_BYTES, uv_fn, uv_cb, \
    {}, \
    uv_file_unbox_borrowed(file.internal, _ctx) __VA_OPT__(,) __VA_ARGS__);

// Free function used for files - triggers a uv_fs_close
static void kk_uv_file_free_fn(void *p, kk_block_t *block, kk_context_t *_ctx) {
  uv_file f = (uv_file) (intptr_t) p;
  uv_fs_close(uvloop(), NULL, f, NULL); // no cb necessary currently
}

static kk_box_t uv_file_box(uv_file f, kk_context_t* _ctx) {
  return kk_cptr_raw_box(&kk_uv_file_free_fn, (void*) (intptr_t) f, _ctx);
}

static uv_file uv_file_unbox_borrowed(kk_box_t box, kk_context_t* _ctx) {
  return (uv_file) (intptr_t) kk_cptr_unbox_borrowed(box, _ctx);
}

declare_uv_req(uv_fs);

static void kk_std_os_file_open_cb(uv_fs_t* req) {
  kk_context_t* _ctx = kk_get_context();
  ssize_t result = req->result;
  kk_function_t cb = kk_uv_req_take_callback_and_free(uv_fs_as_kk_any(req), _ctx);

  kk_uv_error_callback(cb,
    kk_uv_error_or(
      result,
      kk_uv_file__uv_file_box(
        kk_uv_file__new_Uv_file(uv_file_box(result, _ctx), _ctx),
        _ctx),
      _ctx
    ),
    _ctx
  );
}

static void kk_uv_fs_open(kk_string_t path, int32_t flags, int32_t mode, kk_function_t cb, kk_context_t* _ctx) {
  malloc_req(uv_fs, req, cb);

  int status = uv_fs_open(uvloop(), &req->uv, kk_string_cbuf_borrow(path, NULL, _ctx), flags, mode, kk_std_os_file_open_cb);
  kk_string_drop(path, _ctx);

  if (status != UV_OK) {
    // send error via callback
    kk_uv_any_t* kk_any = kk_uv_fs_as_any(req);
    cb = kk_uv_any_take_callback(kk_any, _ctx);
    kk_uv_req_free(kk_any, _ctx);
    kk_uv_error_status_callback(cb, status, _ctx);
  }
}

static void kk_uv_fs_status_code_cb(uv_fs_t* req) {
  kk_context_t* _ctx = kk_get_context();
  ssize_t result = req->result;
  kk_function_t cb = kk_uv_req_take_callback_and_free(uv_fs_as_kk_any(req), _ctx);
  kk_uv_status_code_callback(cb, result, _ctx);
}

static void kk_uv_fs_ssize_cb(uv_fs_t* req) {
  kk_context_t* _ctx = kk_get_context();
  ssize_t result = req->result;
  kk_function_t cb = kk_uv_req_take_callback_and_free(uv_fs_as_kk_any(req), _ctx);
  kk_uv_error_callback(cb,
    kk_uv_error_or(result, kk_ssize_box((kk_ssize_t)result, _ctx), _ctx),
    _ctx
  );
}

// Return a single uv buffer that points to the memory address of `bytes`.
// It's OK that this is stack-allocated; libuv makes a copy for read & write operations
static uv_buf_t kk_bytes_borrow_as_uv_buf(kk_bytes_t bytes, kk_context_t* _ctx) {
  uv_buf_t uv_buf = uv_buf;
  uv_buf.base = (char*)kk_bytes_cbuf_borrow(bytes, (kk_ssize_t*) &uv_buf.len, _ctx);
  return uv_buf;
}

static void kk_uv_fs_read(kk_uv_file__uv_file file, kk_bytes_t bytes, ssize_t offset, kk_function_t cb, kk_context_t* _ctx) {
  uv_buf_t uv_buf = kk_bytes_borrow_as_uv_buf(bytes, _ctx);
  malloc_req(uv_fs, req, cb);

  int status = uv_fs_read(uvloop(), &req->uv, uv_file_unbox_borrowed(file.internal, _ctx), &uv_buf, 1, offset, kk_uv_fs_ssize_cb);
  if (status != UV_OK) {
    // send error via callback
    req->uv.result = status;
    kk_uv_fs_ssize_cb(&req->uv);
  }
}

// static void kk_uv_fs_unlink(kk_string_t path, kk_function_t cb, kk_context_t* _ctx) {
//   kk_uv_oneshot_fs_setup_string(path, uv_fs_unlink, kk_uv_fs_status_code_cb);
// }

static void kk_uv_fs_write(kk_uv_file__uv_file file, kk_bytes_t bytes, int64_t offset, kk_function_t cb, kk_context_t* _ctx) {
  uv_buf_t uv_buf = kk_bytes_borrow_as_uv_buf(bytes, _ctx);
  kk_uv_oneshot_fs_setup(cb, NULL_BYTES, uv_fs_write, kk_uv_fs_ssize_cb,
    { kk_bytes_drop(bytes, _ctx); },
    uv_file_unbox_borrowed(file.internal, _ctx), &uv_buf, 1, offset);
}

// static void kk_uv_fs_mkdir(kk_string_t path, int32_t mode, kk_function_t cb, kk_context_t* _ctx) {
//   kk_uv_oneshot_fs_setup_string(path, uv_fs_mkdir, kk_uv_fs_status_code_cb, mode);
// }

// static void kk_std_os_fs_mkdtemp_cb(uv_fs_t* req) {
//   kk_uv_oneshot_fs_callback1(req,
//     kk_string_box(kk_string_alloc_raw((const char*) req->path, true, _ctx))
//   );
// }

// static void kk_uv_fs_mkdtemp(kk_string_t tpl, kk_function_t cb, kk_context_t* _ctx) {
//   kk_uv_oneshot_fs_setup_string(tpl, uv_fs_mkdtemp, kk_std_os_fs_mkdtemp_cb);
// }

static void kk_std_os_fs_mkstemp_cb(uv_fs_t* req) {
  kk_context_t* _ctx = kk_get_context();
  ssize_t status = req->result;
  if (status >= 0) {
    // copy what we need from req then drop it
    kk_box_t kk_file = uv_file_box(req->result, _ctx);
    kk_string_t kk_path = kk_string_alloc_dup_valid_utf8((const char*) req->path, _ctx);
    kk_function_t cb = kk_uv_req_take_callback_and_free(uv_fs_as_kk_any(req), _ctx);

    kk_uv_error_callback(
      cb,
      kk_std_core_exn__new_Ok(
        kk_std_core_types__tuple2_box(
          kk_std_core_types__new_Tuple2(
            kk_uv_file__uv_file_box(kk_uv_file__new_Uv_file(kk_file, _ctx), _ctx),
            kk_string_box(kk_path),
            _ctx
          ),
          _ctx
        ),
        _ctx
      ),
      _ctx
    );
  } else {
    kk_function_t cb = kk_uv_req_take_callback_and_free(uv_fs_as_kk_any(req), _ctx);
    kk_uv_error_callback(cb, kk_uv_error(status, _ctx), _ctx);
  }
}

static void kk_uv_fs_mkstemp(kk_string_t tpl, kk_function_t cb, kk_context_t* _ctx) {
  kk_warning_message("mkstemp setup\n");
  kk_uv_oneshot_fs_setup_string(tpl, uv_fs_mkstemp, kk_std_os_fs_mkstemp_cb);
}

// static void kk_uv_fs_rmdir(kk_string_t path, kk_function_t cb, kk_context_t* _ctx) {
//   kk_uv_oneshot_fs_setup_string(path, uv_fs_rmdir, kk_uv_fs_status_code_cb);
// }

// void kk_std_os_fs_opendir_cb(uv_fs_t* req) {
//   kk_uv_oneshot_fs_callback1(req,
//     kk_uv_file__uv_dir_box(
//       kk_uv_file__new_Uv_dir((intptr_t)req->ptr, _ctx),
//     _ctx)
//   );
// }

// static void kk_uv_fs_opendir(kk_string_t path, kk_function_t cb, kk_context_t* _ctx) {
//   kk_uv_oneshot_fs_setup_string(path, uv_fs_opendir, kk_std_os_fs_opendir_cb);
// }

// static void kk_uv_fs_closedir(kk_uv_file__uv_dir dir, kk_function_t cb, kk_context_t* _ctx) {
//   kk_uv_oneshot_fs_setup(cb, NULL_BYTES, uv_fs_closedir, kk_uv_fs_status_code_cb, \
//     {}, \
//     (uv_dir_t*)dir.internal);
// }

// kk_box_t kk_uv_dirent_to_dirent(uv_dirent_t* dirent, kk_box_t* loc, kk_context_t* _ctx) {
//   if (loc == NULL) {
//     // TODO: does this leak?
//     loc = kk_malloc(sizeof(kk_box_t*), _ctx);
//   }
//   kk_string_t name = kk_string_alloc_raw((const char*) dirent->name, true, _ctx);
//   kk_uv_file__dirent_type type;
//   switch (dirent->type) {
//     case UV_DIRENT_FILE: type = kk_uv_file__new_FILE(_ctx); break;
//     case UV_DIRENT_DIR: type = kk_uv_file__new_DIR(_ctx); break;
//     case UV_DIRENT_LINK: type = kk_uv_file__new_LINK(_ctx); break;
//     case UV_DIRENT_FIFO: type = kk_uv_file__new_FIFO(_ctx); break;
//     case UV_DIRENT_SOCKET: type = kk_uv_file__new_SOCKET(_ctx); break;
//     case UV_DIRENT_CHAR: type = kk_uv_file__new_CHAR(_ctx); break;
//     case UV_DIRENT_BLOCK: type = kk_uv_file__new_BLOCK(_ctx); break;
//     default: type = kk_uv_file__new_UNKNOWN__DIRECTORY__ENTRY(_ctx); break;
//   }
//   kk_box_t box = kk_uv_file__dirent_box(kk_uv_file__new_Dirent(name, type, _ctx), _ctx);
//   *loc = box;
//   return box;
// }

// kk_vector_t kk_uv_dirents_to_vec(uv_dir_t* uvdir, kk_ssize_t num_entries, kk_context_t* _ctx) {
//   kk_box_t* dirs;
//   kk_vector_t dirents = kk_vector_alloc_uninit(num_entries, &dirs, _ctx);
//   for (kk_ssize_t i = 0; i < num_entries; i++){
//     kk_uv_dirent_to_dirent(&(uvdir->dirents[i]), &dirs[i], _ctx);
//   }
//   return dirents;
// }

// void kk_std_os_fs_readdir_cb(uv_fs_t* req) {
//   // awkwardly named to avoid the definition from the callback macro.
//   // TODO move out of macro?
//   kk_context_t* _ctx_unshadow = kk_get_context();
//   kk_hnd_callback_t* uvhnd_cb = (kk_hnd_callback_t*)req->data;
//   kk_box_t uvdir_box = uvhnd_cb->hnd;
//   ssize_t result = req->result;
//   kk_uv_file__uv_dir uvdir = kk_uv_file__uv_dir_unbox(uvdir_box, KK_BORROWED, _ctx_unshadow);
//   uv_dir_t* uvdir_ptr = (uv_dir_t*)uvdir.internal;

//   kk_vector_t dirents;
//   if (req->result >= 0) {
//    dirents = kk_uv_dirents_to_vec(uvdir_ptr, (kk_ssize_t)req->result, _ctx_unshadow);
//   }

//   if(uvdir_ptr->dirents != NULL) {
//     kk_free(uvdir_ptr->dirents, _ctx_unshadow);
//     uvdir_ptr->dirents = NULL;
//   }

//   kk_uv_oneshot_fs_callback1(req, kk_vector_box(dirents, _ctx));
// }

// // TODO what happens if there are more than 500 dirents?
// static void kk_uv_fs_readdir(kk_uv_file__uv_dir dir, kk_function_t cb, kk_context_t* _ctx) {
//   uv_dir_t* uvdir = (uv_dir_t*) dir.internal;
//   if (uvdir->dirents != NULL) {
//     kk_uv_error_callback(cb, UV_EBUSY);
//   } else {
//     // Read up to 500 entries in the directory
//     uvdir->dirents = kk_malloc(sizeof(uv_dirent_t)*500, _ctx);
//     uvdir->nentries = 500;

//     kk_box_t dir_box = kk_uv_file__uv_dir_box(dir, _ctx);

//     kk_uv_oneshot_fs_setup_box(dir_box, cb, uv_fs_readdir, kk_std_os_fs_readdir_cb, {}, uvdir);
//   }
// }

// void kk_free_fs(void* p, kk_block_t* block, kk_context_t* _ctx) {
//   uv_fs_t* req = (uv_fs_t*)p;
//   kk_uv_hnd_data_free(req);
//   uv_fs_req_cleanup(req);
//   kk_free(p, _ctx);
// }

// static void kk_std_os_fs_scandir_cb(uv_fs_t* req) {
//   kk_uv_oneshot_fs_callback1(req,
//     // return a reference to the original request, stored
//     // as the handle
//     kk_box_dup(((kk_hnd_callback_t*)req->data)->hnd, _ctx)
//   );
// }

// static void kk_uv_fs_scandir(kk_string_t path, kk_function_t cb, kk_context_t* _ctx) {
//   kk_uv_oneshot_fs_setup_box(
//     uv_handle_to_owned_kk_handle_box(req, kk_free_fs, file, fs_req),
//     cb, uv_fs_scandir, kk_std_os_fs_scandir_cb,
//     { kk_string_drop(path, _ctx); },
//     kk_string_cbuf_borrow(path, NULL, 0), 0
//   );
// }

// static kk_std_core_exn__error kk_uv_fs_scandir_next(kk_uv_file__uv_fs_req req, kk_context_t* _ctx) {
//   uv_fs_t* uvhnd = kk_owned_handle_to_uv_handle(uv_fs_t, req);
//   uv_dirent_t ent = {0};
//   int status = uv_fs_scandir_next(uvhnd, &ent);
//   kk_uv_check_return(status, kk_uv_dirent_to_dirent(&ent, NULL, _ctx))
// }

// static void kk_std_os_fs_stat_cb(uv_fs_t* req) {
//   kk_uv_oneshot_fs_callback1(req,
//     kk_uv_file__fstat_box(kk_uv_stat_from_uv_stat(&req->statbuf, _ctx), _ctx)
//   );
// }

// static void kk_uv_fs_stat(kk_string_t path, kk_function_t cb, kk_context_t* _ctx) {
//   kk_uv_oneshot_fs_setup_string(path, uv_fs_stat, kk_std_os_fs_stat_cb);
// }

// static void kk_uv_fs_fstat(kk_uv_file__uv_file file, kk_function_t cb, kk_context_t* _ctx) {
//   kk_uv_oneshot_fs_setup_file(file, uv_fs_fstat, kk_std_os_fs_stat_cb);
// }

// static void kk_uv_fs_lstat(kk_string_t path, kk_function_t cb, kk_context_t* _ctx) {
//   kk_uv_oneshot_fs_setup_string(path, uv_fs_lstat, kk_std_os_fs_stat_cb);
// }

// static void kk_uv_fs_rename(kk_string_t path, kk_string_t new_path, kk_function_t cb, kk_context_t* _ctx) {
//   kk_uv_oneshot_fs_setup_string_string(path, new_path, uv_fs_rename, kk_uv_fs_status_code_cb);
// }

static void kk_uv_fs_fsync(kk_uv_file__uv_file file, kk_function_t cb, kk_context_t* _ctx) {
  kk_uv_oneshot_fs_setup_file(file, uv_fs_fsync, kk_uv_fs_status_code_cb);
}

// static void kk_uv_fs_fdatasync(kk_uv_file__uv_file file, kk_function_t cb, kk_context_t* _ctx) {
//   kk_uv_oneshot_fs_setup_file(file, uv_fs_fdatasync, kk_uv_fs_status_code_cb);
// }

// static void kk_uv_fs_ftruncate(kk_uv_file__uv_file file, int64_t offset, kk_function_t cb, kk_context_t* _ctx) {
//   kk_uv_oneshot_fs_setup_file(file, uv_fs_ftruncate, kk_uv_fs_status_code_cb, offset);
// }

// static void kk_uv_fs_copyfile(kk_string_t path, kk_string_t new_path, int32_t flags, kk_function_t cb, kk_context_t* _ctx) {
//   kk_uv_oneshot_fs_setup_string_string(path, new_path, uv_fs_copyfile, kk_uv_fs_status_code_cb, flags);
// }

// static void kk_std_os_fs_int_cb(uv_fs_t* req) {
//   kk_uv_oneshot_fs_callback1(req, kk_integer_box(kk_integer_from_ssize_t(req->result, _ctx), _ctx));
// }

// // TOOD
// static kk_unit_t kk_uv_fs_sendfile(kk_uv_file__uv_file out_fd, kk_uv_file__uv_file in_fd, int64_t in_offset, kk_ssize_t length, kk_function_t cb, kk_context_t* _ctx) {
//   uv_fs_t* fs_req = kk_malloc(sizeof(uv_fs_t), _ctx);
//   uv_buf_t buf = uv_buf_init(NULL, 0);
//   fs_req->data = kk_function_as_ptr(cb, _ctx);
//   uv_fs_sendfile(uvloop(), fs_req, (uv_file)out_fd.internal, (uv_file)in_fd.internal, in_offset, (size_t)length, kk_std_os_fs_int_cb);
//   kk_uv_file__uv_file_drop(out_fd, _ctx);
//   kk_uv_file__uv_file_drop(in_fd, _ctx);
//   return kk_Unit;
// }

// static void kk_uv_fs_access(kk_string_t path, int32_t mode, kk_function_t cb, kk_context_t* _ctx) {
//   kk_uv_oneshot_fs_setup_string(path, uv_fs_access, kk_uv_fs_status_code_cb, mode);
// }

// static void kk_uv_fs_chmod(kk_string_t path, int32_t mode, kk_function_t cb, kk_context_t* _ctx) {
//   kk_uv_oneshot_fs_setup_string(path, uv_fs_chmod, kk_uv_fs_status_code_cb, mode);
// }

// static void kk_uv_fs_fchmod(kk_uv_file__uv_file file, int32_t mode, kk_function_t cb, kk_context_t* _ctx) {
//   kk_uv_oneshot_fs_setup_file(file, uv_fs_fchmod, kk_uv_fs_status_code_cb, mode);
// }

// static void kk_uv_fs_utime(kk_string_t path, double atime, double mtime, kk_function_t cb, kk_context_t* _ctx) {
//   kk_uv_oneshot_fs_setup_string(path, uv_fs_utime, kk_uv_fs_status_code_cb, atime, mtime);
// }

// static void kk_uv_fs_futime(kk_uv_file__uv_file file, double atime, double mtime, kk_function_t cb, kk_context_t* _ctx) {
//   kk_uv_oneshot_fs_setup_file(file, uv_fs_futime, kk_uv_fs_status_code_cb, atime, mtime);
// }

// static void kk_uv_fs_lutime(kk_string_t path, double atime, double mtime, kk_function_t cb, kk_context_t* _ctx) {
//   kk_uv_oneshot_fs_setup_string(path, uv_fs_lutime, kk_uv_fs_status_code_cb, atime, mtime);
// }

// static void kk_uv_fs_link(kk_string_t path, kk_string_t new_path, kk_function_t cb, kk_context_t* _ctx) {
//   kk_uv_oneshot_fs_setup_string_string(path, new_path, uv_fs_link, kk_uv_fs_status_code_cb);
// }

// static void kk_uv_fs_symlink(kk_string_t path, kk_string_t new_path, int32_t flags, kk_function_t cb, kk_context_t* _ctx) {
//   kk_uv_oneshot_fs_setup_string_string(path, new_path, uv_fs_symlink, kk_uv_fs_status_code_cb, flags);
// }

// void kk_std_os_fs_string_cb(uv_fs_t* req) {
//   kk_uv_oneshot_fs_callback1(req, kk_string_box(kk_string_alloc_raw((const char*)req->ptr, true, _ctx)));
// }

// static void kk_uv_fs_readlink(kk_string_t path, kk_function_t cb, kk_context_t* _ctx) {
//   kk_uv_oneshot_fs_setup_string(path, uv_fs_readlink, kk_std_os_fs_string_cb);
// }

// static void kk_uv_fs_realpath(kk_string_t path, kk_function_t cb, kk_context_t* _ctx) {
//   kk_uv_oneshot_fs_setup_string(path, uv_fs_realpath, kk_std_os_fs_string_cb);
// }

// static void kk_uv_fs_chown(kk_string_t path, int32_t uid, int32_t gid, kk_function_t cb, kk_context_t* _ctx) {
//   kk_uv_oneshot_fs_setup_string(path, uv_fs_chown, kk_uv_fs_status_code_cb, uid, gid);
// }

// static void kk_uv_fs_fchown(kk_uv_file__uv_file file, int32_t uid, int32_t gid, kk_function_t cb, kk_context_t* _ctx) {
//   kk_uv_oneshot_fs_setup_file(file, uv_fs_fchown, kk_uv_fs_status_code_cb, uid, gid);
// }

// static void kk_uv_fs_lchown(kk_string_t path, int32_t uid, int32_t gid, kk_function_t cb, kk_context_t* _ctx) {
//   kk_uv_oneshot_fs_setup_string(path, uv_fs_lchown, kk_uv_fs_status_code_cb, uid, gid);
// }

static kk_uv_status_code_t kk_uv_file_close_impl(kk_uv_file__uv_file f, kk_context_t* _ctx) {
  // files, like handles, are closed on drop. If `f` is not the last unique reference,
  // return EBUSY to indicate a programmer error
  int status = UV_OK;
  void* ptr = kk_box_to_ptr(f.internal, _ctx);
  if (!kk_block_is_unique(ptr)) {
    kk_warning_message(
      "Handle of type FILE is still referenced (refcount %d); not dropping\n",
      kk_block_refcount(ptr)
    );
    status = UV_EBUSY;
  }
  kk_uv_file__uv_file_drop(f, _ctx);
  return kk_uv_status_code(status, _ctx);
}
