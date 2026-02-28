void kk_uv_bufs_drop(kk_uv_bufs_t* bufs, kk_context_t *_ctx) {
  kk_std_core_types__vector_drop(bufs->kk_bufs, _ctx);
  // uv_bufs memory is just part of `bufs` itself, no need to explicitly free
  kk_free(bufs, _ctx);
}


kk_uv_bufs_t* kk_bytes_vec_to_uv_bufs(kk_std_core_types__vector kk_bufs, kk_ssize_t* len, kk_context_t* _ctx) {
  ssize_t list_len = kk_vector_len_borrow(kk_bufs, _ctx);
  kk_uv_bufs_t* bufs = kk_malloc(sizeof(kk_uv_bufs_t) + (sizeof(uv_buf_t) * list_len), _ctx);

  for (ssize_t i = 0; i < list_len; i++){
    kk_bytes_t bytes = kk_bytes_unbox(kk_vector_at_borrow(kk_bufs, i, _ctx));
    kk_ssize_t nbytes;
    bufs->uv_bufs[i].base = (char*)kk_bytes_cbuf_borrow(bytes, &nbytes, _ctx);
    bufs->uv_bufs[i].len = nbytes;
  }
  *len = list_len;
  return bufs;
}
