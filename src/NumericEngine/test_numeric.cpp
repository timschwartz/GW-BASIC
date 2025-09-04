#include <catch2/catch_all.hpp>
#include "NumericEngine.hpp"

TEST_CASE("NumericEngine basic arithmetic", "[numeric]") {
    NumericEngine engine;
    
    SECTION("Integer addition") {
        auto result = engine.add(Int16{5}, Int16{3});
        REQUIRE(result);
        REQUIRE(std::holds_alternative<Int16>(result.value));
        REQUIRE(std::get<Int16>(result.value).v == 8);
    }
    
    SECTION("Integer overflow promotion to single") {
        auto result = engine.add(Int16{32000}, Int16{1000});
        REQUIRE(result);
        REQUIRE(std::holds_alternative<Single>(result.value));
    }
    
    SECTION("Floating point addition") {
        auto result = engine.add(Single{1.5f}, Single{2.5f});
        REQUIRE(result);
        REQUIRE(std::holds_alternative<Single>(result.value));
        REQUIRE(std::get<Single>(result.value).v == 4.0f);
    }
}

TEST_CASE("NumericEngine math functions", "[numeric]") {
    NumericEngine engine;
    
    SECTION("Square root") {
        auto result = engine.sqrt(Single{16.0f});
        REQUIRE(result);
        REQUIRE(std::holds_alternative<Single>(result.value));
        REQUIRE(std::get<Single>(result.value).v == 4.0f);
    }
    
    SECTION("Square root of negative number") {
        auto result = engine.sqrt(Single{-4.0f});
        REQUIRE_FALSE(result);
        REQUIRE(result.error == NumericError::IllegalFunctionCall);
    }
}

TEST_CASE("NumericEngine comparisons", "[numeric]") {
    NumericEngine engine;
    
    SECTION("Equality - true case") {
        auto result = engine.equals(Int16{5}, Int16{5});
        REQUIRE(result);
        REQUIRE(result.value.v == -1); // GW-BASIC true = -1
    }
    
    SECTION("Equality - false case") {
        auto result = engine.equals(Int16{5}, Int16{3});
        REQUIRE(result);
        REQUIRE(result.value.v == 0); // GW-BASIC false = 0
    }
}

TEST_CASE("NumericEngine division by zero", "[numeric]") {
    NumericEngine engine;
    
    auto result = engine.divide(Int16{5}, Int16{0});
    REQUIRE_FALSE(result);
    REQUIRE(result.error == NumericError::DivisionByZero);
}

TEST_CASE("NumericEngine formatting", "[numeric]") {
    NumericEngine engine;
    
    SECTION("Integer formatting") {
        std::string formatted = engine.formatNumber(Int16{42});
        REQUIRE(formatted == "42");
    }
    
    SECTION("Floating point formatting") {
        std::string formatted = engine.formatNumber(Single{3.14f});
        REQUIRE_FALSE(formatted.empty());
    }
}

TEST_CASE("NumericEngine random numbers", "[numeric]") {
    NumericEngine engine;
    
    SECTION("RND function") {
        auto result = engine.rnd();
        REQUIRE(result);
        REQUIRE(result.value.v >= 0.0f);
        REQUIRE(result.value.v < 1.0f);
    }
    
    SECTION("RND with seed") {
        auto result = engine.rnd(Int16{42});
        REQUIRE(result);
        REQUIRE(result.value.v >= 0.0f);
        REQUIRE(result.value.v < 1.0f);
    }
}
