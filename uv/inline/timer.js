function _set_timeout(timer, ms, fcn){
  const msx = Number(ms)
  const timer = {
    id: setTimeout(fcn, msx)
  };
  return $std_core_exn.Ok(timer);
}

function _clear_timeout(timer){
  if (timer.id) {
    clearTimeout(timer.id);
    timer.id = null;
  }
  return $std_core_exn.Ok( $std_core_types._Unit_ );
}
