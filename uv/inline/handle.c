
// general handler utilities

static kk_uv_status_code_t kk_uv_handle_close_impl(kk_uv_utils__uv_handle handle, kk_context_t* _ctx) {
  kk_uv_handle_t* uv_hnd = kk_uv_handle_unbox_borrowed(handle.internal, _ctx);
  // drop references held by the handle (e.g. callbacks)
  // TODO: if handle is not unique, the callbacks might still be required?
  kk_uv_handle_drop_references(uv_hnd, _ctx);

  // handles are closed on drop. If `handle` is not the last unique reference,
  // return EBUSY to indicate a programmer error
  int status = UV_OK;
  if (!kk_block_is_unique(kk_box_to_ptr(handle.internal, kk_context()))) {
    kk_warning_message(
      "Handle of type %d @ %p is still referenced (refcount %d); not dropping\n",
      uv_hnd->uv.type,
      kk_box_to_ptr(handle.internal, _ctx),
      kk_block_refcount(kk_box_to_ptr(handle.internal, kk_context()))
    );
    status = UV_EBUSY;
  // } else {
    // kk_warning_message("Dropping last reference to handle of type %d\n", uv_hnd->uv.type);
  }
  kk_uv_utils__uv_handle_drop(handle, _ctx);
  return kk_uv_status_code(status, _ctx);
}
