// test/test_burst_grouper.cpp
#include "doctest.h"
#include "BurstGrouper.h"

TEST_CASE("a two-pulse burst completes once the next burst's first edge arrives") {
  BurstGrouper g;
  int size = -1;

  CHECK_FALSE(g.pulse(0, &size));      // first edge of burst 1
  CHECK_FALSE(g.pulse(1, &size));      // 1us gap -- still burst 1

  CHECK(g.pulse(500, &size));          // 499us gap -- burst 1 just completed
  CHECK(size == 2);
}
