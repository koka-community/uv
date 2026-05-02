declare_uv_handle(uv_tcp);
declare_uv_req(uv_connect);

static int fill_sockaddr(struct sockaddr_storage* dest, kk_uv_tcp__sock_addr addr, kk_context_t* _ctx){
  int p;
  kk_ssize_t len;
  if (kk_std_core_types__is_Just(addr.port, _ctx)){
    p = (int)kk_int32_unbox(addr.port._cons.Just.value, KK_BORROWED, _ctx);
  } else {
    p = 0; // any available port
  }
  if (kk_uv_tcp__is_AF__INET(addr.family, _ctx)){
    const char* str = kk_string_cbuf_borrow(addr.data, &len, _ctx);
    int status = uv_ip4_addr(str, p, (struct sockaddr_in*)dest);
    return status;
  } else if (kk_uv_tcp__is_AF__INET6(addr.family, _ctx)){
    const char* str = kk_string_cbuf_borrow(addr.data, &len, _ctx);
    return uv_ip6_addr(str, p, (struct sockaddr_in6*)dest);
  } else {
    return UV_EAI_ADDRFAMILY;
  }
}

static kk_uv_tcp__sock_addr to_kk_sockaddr(struct sockaddr* addr, kk_context_t* _ctx){
  enum kk_uv_tcp__net_family_e family = kk_uv_tcp_AF__ANY;
  
  kk_std_core_types__maybe portMaybe;
  if (addr->sa_family == AF_INET){
    family = kk_uv_tcp_AF__INET;
    int port = ntohs(((struct sockaddr_in*)addr)->sin_port);
    portMaybe = kk_std_core_types__new_Just(kk_int32_box(port, _ctx), _ctx);
  } else if (addr->sa_family == AF_INET6){
    family = kk_uv_tcp_AF__INET6;
    int port = ntohs(((struct sockaddr_in6*)addr)->sin6_port);
    portMaybe = kk_std_core_types__new_Just(kk_int32_box(port, _ctx), _ctx);
  } else {
    portMaybe = kk_std_core_types__new_Nothing(_ctx);
  }
  char ip[50]= "";
  inet_ntop(addr->sa_family, &addr->sa_data[2], ip, sizeof(ip));

  kk_string_t ipStr = kk_string_alloc_from_qutf8(ip, _ctx);
  return kk_uv_tcp__new_Sock_addr(family, ipStr, portMaybe, _ctx);
}


static kk_std_core_exn__error kk_uv_tcp_init(kk_context_t* _ctx) {
  int status;
  malloc_and_init_handle(uv_tcp, tcp, status, uvloop(), &tcp->uv);
  if (status != UV_OK) {
    kk_free(tcp, _ctx);
    return kk_uv_error(status, _ctx);
  } else {
    return kk_std_core_types__new_Ok(
      kk_uv_tcp__tcp_box(
        kk_uv_tcp__new_Tcp(
          kk_uv_tcp_box(tcp, _ctx),
          _ctx
        ),
        _ctx
      ),
      _ctx
    );
  }
}

static kk_uv_status_code_t kk_uv_tcp_bind(kk_uv_tcp__tcp tcp, kk_uv_tcp__sock_addr addr, int32_t flags, kk_context_t* _ctx) {
  kk_uv_tcp_t* kk_tcp = kk_uv_tcp_unbox_borrowed(tcp.internal, _ctx);
  struct sockaddr_storage sockaddr;
  int status = fill_sockaddr(&sockaddr, addr, _ctx);
  if (status == UV_OK) {
    status = uv_tcp_bind(&kk_tcp->uv, (struct sockaddr*)&sockaddr, flags);
  }
  return kk_uv_status_code(status, _ctx);
}


static kk_std_core_exn__error kk_uv_tcp_getsockname(kk_uv_tcp__tcp tcp, kk_context_t* _ctx) {
  kk_uv_tcp_t* kk_tcp = kk_uv_tcp_unbox_borrowed(tcp.internal, _ctx);
  struct sockaddr_storage sockaddr;
  int len = sizeof(sockaddr);
  struct sockaddr* sockaddr_ptr = (struct sockaddr*)&sockaddr;

  int status = uv_tcp_getsockname(&kk_tcp->uv, sockaddr_ptr, &len);
  return kk_uv_error_or(status,
    kk_uv_tcp__sock_addr_box(to_kk_sockaddr(sockaddr_ptr, _ctx), _ctx),
    _ctx
  );
}

static void kk_uv_tcp_connect_callback(uv_connect_t* uv_connect, int status) {
  kk_context_t* _ctx = kk_get_context();
  kk_uv_connect_t* kk_connect = uv_connect_as_kk(uv_connect);
  kk_function_t callback = kk_uv_any_take_callback(kk_uv_connect_as_any(kk_connect), _ctx);
  kk_uv_status_code_callback(callback, status, kk_get_context());
}

static void kk_uv_tcp_connect_c(kk_uv_tcp__tcp tcp, kk_uv_tcp__sock_addr addr, kk_function_t callback, kk_context_t* _ctx) {
  struct sockaddr_storage sockaddr;
  int status = fill_sockaddr(&sockaddr, addr, _ctx);
  if (status != UV_OK) {
    kk_uv_status_code_callback(callback, status, _ctx);
    return;
  }
  
  kk_uv_tcp_t* kk_tcp = kk_uv_tcp_unbox_borrowed(tcp.internal, _ctx);
  malloc_req(uv_connect, connect, callback);
  status = uv_tcp_connect(&connect->uv, &kk_tcp->uv, (struct sockaddr*)&sockaddr, kk_uv_tcp_connect_callback);
  if (status != UV_OK) {
    kk_uv_any_t* req = kk_uv_connect_as_any(connect);
    callback = kk_uv_req_take_callback_and_free(req, _ctx);
    kk_uv_status_code_callback(callback, status, _ctx);
  }
}
