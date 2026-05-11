// TODO: This is manually modified from the output of std/test/detect.
//       Remove it once std/test/detect can output a working async suite.

import test-uv-stream-test = test/uv/stream-test
import test-uv-event-loop-test = test/uv/event-loop-test
import test-uv-core-test = test/uv/core-test
import test-std-async-process-test = test/std/async/process-test
import test-std-async-fs-test = test/std/async/fs-test

// extra imports
import uv/event-loop
import std/test
import std/async/async

fun main()

  // extra handlers
  with default-event-loop
  with async/handle

  with run-tests
  group("test/std/async/fs-test.kk")
    test-std-async-fs-test/suite()

  group("test/std/async/process-test.kk")
    test-std-async-process-test/suite()

  group("test/uv/core-test.kk")
    test-uv-core-test/suite()

  group("test/uv/event-loop-test.kk")
    test-uv-event-loop-test/suite()

  group("test/uv/stream-test.kk")
    test-uv-stream-test/suite()
