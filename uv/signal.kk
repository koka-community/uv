import std/num/int32

pub val sig-int = sSIGINT()
extern sSIGINT(): int32
  c inline "SIGINT"

pub val sig-quit = sSIGQUIT()
extern sSIGQUIT(): int32
  c inline "SIGQUIT"

pub val sig-kill = sSIGKILL()
extern sSIGKILL(): int32
  c inline "SIGKILL"
