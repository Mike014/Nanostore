#include "catch_amalgamated.hpp"

int main(int argc, char* argv[]) {
    return Catch::Session().run(argc, argv);
}

TEST_CASE("minimal") {
    REQUIRE(1 + 1 == 2);
}
