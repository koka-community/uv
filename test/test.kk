/*----------------------------------------------------------------------------
   Copyright 2024, Koka-Community Authors

   Licensed under the MIT License ("The License"). You may not
   use this file except in compliance with the License. A copy of the License
   can be found in the LICENSE file at the root of this distribution.
----------------------------------------------------------------------------*/
import std/test

// Some simple examples
fun test-tests()
  basic/scope("Basics")
    with basic-nested/scope("Another")
    scoped/test("Returns continue tests")
      val res = expect-result(1) fn() 2
      println("Continued!")
      expect(2) fn() 1 + res
      expect(2) fn() 1
  basic/test("Wrong expect")
    expect(1) fn() 2
  basic/test("Additional info")
    expect(1, {throw("Some error somewhere")}, details="Really expected 1 because I proved it!") 