#include <catch2/catch_all.hpp>
#include "NumericEngine.hpp"
#include <cmath>

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
    
    SECTION("Modulo operation") {
        auto result = engine.modulo(Int16{10}, Int16{3});
        REQUIRE(result);
        REQUIRE(std::holds_alternative<Int16>(result.value));
        REQUIRE(std::get<Int16>(result.value).v == 1);
    }
    
    SECTION("Modulo with floating point") {
        auto result = engine.modulo(Single{10.5f}, Single{3.0f});
        REQUIRE(result);
        REQUIRE(std::holds_alternative<Single>(result.value));
        REQUIRE(std::get<Single>(result.value).v == Catch::Approx(1.5f));
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
    
    SECTION("Sine function") {
        auto result = engine.sin(Single{0.0f});
        REQUIRE(result);
        REQUIRE(std::holds_alternative<Single>(result.value));
        REQUIRE(std::get<Single>(result.value).v == Catch::Approx(0.0f));
        
        auto result_pi_2 = engine.sin(Single{static_cast<float>(M_PI / 2)});
        REQUIRE(result_pi_2);
        REQUIRE(std::get<Single>(result_pi_2.value).v == Catch::Approx(1.0f));
    }
    
    SECTION("Cosine function") {
        auto result = engine.cos(Single{0.0f});
        REQUIRE(result);
        REQUIRE(std::holds_alternative<Single>(result.value));
        REQUIRE(std::get<Single>(result.value).v == Catch::Approx(1.0f));
        
        auto result_pi = engine.cos(Single{static_cast<float>(M_PI)});
        REQUIRE(result_pi);
        REQUIRE(std::get<Single>(result_pi.value).v == Catch::Approx(-1.0f));
    }
    
    SECTION("Tangent function") {
        auto result = engine.tan(Single{0.0f});
        REQUIRE(result);
        REQUIRE(std::holds_alternative<Single>(result.value));
        REQUIRE(std::get<Single>(result.value).v == Catch::Approx(0.0f));
        
        auto result_pi_4 = engine.tan(Single{static_cast<float>(M_PI / 4)});
        REQUIRE(result_pi_4);
        REQUIRE(std::get<Single>(result_pi_4.value).v == Catch::Approx(1.0f));
    }
    
    SECTION("Arctangent function") {
        auto result = engine.atn(Single{0.0f});
        REQUIRE(result);
        REQUIRE(std::holds_alternative<Single>(result.value));
        REQUIRE(std::get<Single>(result.value).v == Catch::Approx(0.0f));
        
        auto result_1 = engine.atn(Single{1.0f});
        REQUIRE(result_1);
        REQUIRE(std::get<Single>(result_1.value).v == Catch::Approx(M_PI / 4));
    }
    
    SECTION("Natural logarithm") {
        auto result = engine.log(Single{1.0f});
        REQUIRE(result);
        REQUIRE(std::holds_alternative<Single>(result.value));
        REQUIRE(std::get<Single>(result.value).v == Catch::Approx(0.0f));
        
        auto result_e = engine.log(Single{static_cast<float>(M_E)});
        REQUIRE(result_e);
        REQUIRE(std::get<Single>(result_e.value).v == Catch::Approx(1.0f));
    }
    
    SECTION("Logarithm of negative number") {
        auto result = engine.log(Single{-1.0f});
        REQUIRE_FALSE(result);
        REQUIRE(result.error == NumericError::IllegalFunctionCall);
    }
    
    SECTION("Exponential function") {
        auto result = engine.exp(Single{0.0f});
        REQUIRE(result);
        REQUIRE(std::holds_alternative<Single>(result.value));
        REQUIRE(std::get<Single>(result.value).v == Catch::Approx(1.0f));
        
        auto result_1 = engine.exp(Single{1.0f});
        REQUIRE(result_1);
        REQUIRE(std::get<Single>(result_1.value).v == Catch::Approx(M_E));
    }
    
    SECTION("Absolute value") {
        auto result_pos = engine.abs(Single{5.5f});
        REQUIRE(result_pos);
        REQUIRE(std::holds_alternative<Single>(result_pos.value));
        REQUIRE(std::get<Single>(result_pos.value).v == 5.5f);
        
        auto result_neg = engine.abs(Single{-5.5f});
        REQUIRE(result_neg);
        REQUIRE(std::get<Single>(result_neg.value).v == 5.5f);
        
        auto result_zero = engine.abs(Int16{0});
        REQUIRE(result_zero);
        REQUIRE(std::get<Int16>(result_zero.value).v == 0);
    }
    
    SECTION("Sign function") {
        auto result_pos = engine.sgn(Single{5.5f});
        REQUIRE(result_pos);
        REQUIRE(result_pos.value.v == 1);
        
        auto result_neg = engine.sgn(Single{-5.5f});
        REQUIRE(result_neg);
        REQUIRE(result_neg.value.v == -1);
        
        auto result_zero = engine.sgn(Int16{0});
        REQUIRE(result_zero);
        REQUIRE(result_zero.value.v == 0);
    }
    
    SECTION("INT function") {
        auto result_pos = engine.intFunc(Single{5.7f});
        REQUIRE(result_pos);
        REQUIRE(result_pos.value.v == 5);
        
        auto result_neg = engine.intFunc(Single{-5.7f});
        REQUIRE(result_neg);
        REQUIRE(result_neg.value.v == -6); // floor function
    }
    
    SECTION("FIX function") {
        auto result_pos = engine.fix(Single{5.7f});
        REQUIRE(result_pos);
        REQUIRE(result_pos.value.v == 5);
        
        auto result_neg = engine.fix(Single{-5.7f});
        REQUIRE(result_neg);
        REQUIRE(result_neg.value.v == -5); // truncate towards zero
    }
}

TEST_CASE("NumericEngine comparison operations", "[numeric]") {
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
    
    SECTION("Not equals") {
        auto result = engine.notEquals(Int16{5}, Int16{3});
        REQUIRE(result);
        REQUIRE(result.value.v == -1); // true
        
        auto result_eq = engine.notEquals(Int16{5}, Int16{5});
        REQUIRE(result_eq);
        REQUIRE(result_eq.value.v == 0); // false
    }
    
    SECTION("Less than") {
        auto result = engine.lessThan(Int16{3}, Int16{5});
        REQUIRE(result);
        REQUIRE(result.value.v == -1); // true
        
        auto result_false = engine.lessThan(Int16{5}, Int16{3});
        REQUIRE(result_false);
        REQUIRE(result_false.value.v == 0); // false
    }
    
    SECTION("Greater than") {
        auto result = engine.greaterThan(Int16{5}, Int16{3});
        REQUIRE(result);
        REQUIRE(result.value.v == -1); // true
        
        auto result_false = engine.greaterThan(Int16{3}, Int16{5});
        REQUIRE(result_false);
        REQUIRE(result_false.value.v == 0); // false
    }
}

TEST_CASE("NumericEngine type conversions", "[numeric]") {
    NumericEngine engine;
    
    SECTION("Int16 to Single") {
        auto result = engine.toSingle(Int16{42});
        REQUIRE(result);
        REQUIRE(result.value.v == 42.0f);
    }
    
    SECTION("Single to Int16") {
        auto result = engine.toInt16(Single{42.7f});
        REQUIRE(result);
        REQUIRE(result.value.v == 43); // rounds to nearest
    }
    
    SECTION("Double to Single") {
        auto result = engine.toSingle(Double{123.456});
        REQUIRE(result);
        REQUIRE(result.value.v == Catch::Approx(123.456f));
    }
    
    SECTION("Overflow in conversion") {
        auto result = engine.toInt16(Single{50000.0f});
        REQUIRE_FALSE(result);
        REQUIRE(result.error == NumericError::Overflow);
    }
}

TEST_CASE("NumericEngine unary operations", "[numeric]") {
    NumericEngine engine;
    
    SECTION("Negate positive") {
        auto result = engine.negate(Int16{5});
        REQUIRE(result);
        REQUIRE(std::holds_alternative<Int16>(result.value));
        REQUIRE(std::get<Int16>(result.value).v == -5);
    }
    
    SECTION("Negate negative") {
        auto result = engine.negate(Single{-3.14f});
        REQUIRE(result);
        REQUIRE(std::holds_alternative<Single>(result.value));
        REQUIRE(std::get<Single>(result.value).v == Catch::Approx(3.14f));
    }
    
    SECTION("Negate minimum int16") {
        auto result = engine.negate(Int16{std::numeric_limits<int16_t>::min()});
        REQUIRE(result);
        REQUIRE(std::holds_alternative<Single>(result.value)); // promotes to Single
        REQUIRE(std::get<Single>(result.value).v == 32768.0f);
    }
}

TEST_CASE("NumericEngine utility functions", "[numeric]") {
    NumericEngine engine;
    
    SECTION("isZero") {
        REQUIRE(engine.isZero(Int16{0}));
        REQUIRE(engine.isZero(Single{0.0f}));
        REQUIRE(engine.isZero(Double{0.0}));
        REQUIRE_FALSE(engine.isZero(Int16{1}));
        REQUIRE_FALSE(engine.isZero(Single{0.1f}));
    }
    
    SECTION("isNegative") {
        REQUIRE(engine.isNegative(Int16{-1}));
        REQUIRE(engine.isNegative(Single{-0.1f}));
        REQUIRE_FALSE(engine.isNegative(Int16{0}));
        REQUIRE_FALSE(engine.isNegative(Int16{1}));
    }
    
    SECTION("isInteger") {
        REQUIRE(engine.isInteger(Int16{42}));
        REQUIRE(engine.isInteger(Single{42.0f}));
        REQUIRE(engine.isInteger(Double{-17.0}));
        REQUIRE_FALSE(engine.isInteger(Single{42.5f}));
        REQUIRE_FALSE(engine.isInteger(Double{3.14159}));
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
    
    SECTION("RANDOMIZE function") {
        engine.randomize(Int16{123});
        auto result1 = engine.rnd();
        REQUIRE(result1);
        
        engine.randomize(Int16{123}); // Same seed
        auto result2 = engine.rnd();
        REQUIRE(result2);
        REQUIRE(result1.value.v == result2.value.v); // Should be reproducible
    }
}
