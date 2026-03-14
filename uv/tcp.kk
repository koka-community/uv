module uv/tcp

import uv/handle
import uv/stream
import std/num/int32

extern import
  c file "inline/tcp.c"

abstract struct tcp { internal: any }

// Any uv can be a handle, so it's always safe to cast
pub fun uv-handle(^tcp: tcp): io-noexn uv-handle
  Uv-handle(tcp.internal)

// TCP is a stream
pub fun uv-stream(^tcp: tcp): uv-stream
  Uv-stream(tcp.internal)

// Socket types
pub type sock-type
  SOCK_ANY
  SOCK_STREAM
  SOCK_DGRAM

// Internet address families
pub type net-family
  AF_INET
  AF_INET6
  AF_ANY

pub fun net-family/show(family: net-family): string
  match family
    AF_INET -> "AF_INET"
    AF_INET6 -> "AF_INET6"
    AF_ANY -> "AF_ANY"

// Internet address info
pub struct addr-info
  flags: int32
  pub family: net-family
  pub socktype: sock-type
  protocol: int32
  pub addr: sock-addr
  pub canonName: string;

// Socket addresses
pub value struct sock-addr
  pub family: net-family
  pub data: string
  pub port: maybe<int32>

pub fun sock-addr/show(addr: sock-addr)
  "Sock-addr(" ++ addr.family.show ++ ", " ++ addr.data.show ++ ", " ++ addr.port.show ++ ")"

// // Initialize the handle. No socket is created as of yet.
pub extern tcp-init(): io-noexn error<tcp>
  c "kk_uv_tcp_init"

// Bind the handle to an address and port.
// When the port is 0, a random free port will be chosen to listen on (and can be read using get-sockname)
// When the port is already taken, you can expect to see an UV_EADDRINUSE error from `listen()`` or `connect()`.
// That is, a successful call to this function does not guarantee that the call to `listen()` or `connect()` will succeed as well.
pub extern tcp/bind(^tcp: tcp, addr: sock-addr, flags: int32): io-noexn uv-status-code
  c "kk_uv_tcp_bind"

pub extern get-sockname(^tcp: tcp): io-noexn error<sock-addr>
  c "kk_uv_tcp_getsockname"

// Establish an IPv4 or IPv6 TCP connection. Provide an initialized TCP handle.
// The callback is made when the connection has been established or when a connection error happened.
pub extern connect(^tcp: tcp, addr: sock-addr, callback: (uv-status-code) -> io-noexn ()): io-noexn ()
  c "kk_uv_tcp_connect_c"
