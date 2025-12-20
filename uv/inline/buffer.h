// A structure allowing uv buffers to share the underlying
// memory of a vector<bytes> (by keeping a reference to it)
typedef struct kk_uv_bufs_s {
  kk_std_core_types__vector kk_bufs;
  uv_buf_t uv_bufs[]; // c99 flexible array member
} kk_uv_bufs_t;

void kk_uv_bufs_drop(kk_uv_bufs_t* bufs, kk_context_t *_ctx);

kk_uv_bufs_t* kk_bytes_vec_to_uv_bufs(kk_std_core_types__vector kk_bufs, kk_ssize_t* len, kk_context_t* _ctx);
