#include <catch2/catch_all.hpp>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

#include "ExpressionEvaluator.hpp"

using namespace expr;
using Catch::Approx;

static std::vector<uint8_t> asciiBody(const std::string& src) {
    std::vector<uint8_t> b(src.begin(), src.end());
    b.push_back(0x00);
    return b;
}

TEST_CASE("ExpressionEvaluator parses integer literals", "[expr]") {
    std::shared_ptr<Tokenizer> tok; // not used yet
    ExpressionEvaluator ev(tok);
    Env env;

    auto b = asciiBody("123");
    auto res = ev.evaluate(b, 0, env);
    REQUIRE(std::holds_alternative<Int16>(res.value));
    REQUIRE(std::get<Int16>(res.value).v == 123);
}

TEST_CASE("ExpressionEvaluator parses parentheses", "[expr]") {
    std::shared_ptr<Tokenizer> tok; // not used yet
    ExpressionEvaluator ev(tok);
    Env env;

    auto b = asciiBody("(456)");
    auto res = ev.evaluate(b, 0, env);
    REQUIRE(std::holds_alternative<Int16>(res.value));
    REQUIRE(std::get<Int16>(res.value).v == 456);
}

TEST_CASE("ExpressionEvaluator boolean conversion", "[expr]") {
    REQUIRE(ExpressionEvaluator::toBoolInt(Int16{0}) == 0);
    REQUIRE(ExpressionEvaluator::toBoolInt(Int16{42}) == -1);
    REQUIRE(ExpressionEvaluator::toBoolInt(Single{0.0f}) == 0);
    REQUIRE(ExpressionEvaluator::toBoolInt(Double{1.0}) == -1);
    REQUIRE(ExpressionEvaluator::toBoolInt(Str{""}) == 0);
    REQUIRE(ExpressionEvaluator::toBoolInt(Str{"A"}) == -1);
}

static std::vector<uint8_t> asciiExpr(const std::string& s) { return asciiBody(s); }

TEST_CASE("ExpressionEvaluator operator precedence and unary", "[expr]") {
    ExpressionEvaluator ev(nullptr);
    Env env;

    auto res1 = ev.evaluate(asciiExpr("1+2*3"), 0, env);
    REQUIRE(std::holds_alternative<Int16>(res1.value));
    REQUIRE(std::get<Int16>(res1.value).v == 7);

    auto res2 = ev.evaluate(asciiExpr("(1+2)*3"), 0, env);
    REQUIRE(std::holds_alternative<Int16>(res2.value));
    REQUIRE(std::get<Int16>(res2.value).v == 9);

    auto res3 = ev.evaluate(asciiExpr("-5^2"), 0, env);
    REQUIRE(std::holds_alternative<Double>(res3.value));
    REQUIRE(std::get<Double>(res3.value).v == Approx(-25.0));

    auto res4 = ev.evaluate(asciiExpr("10\\3"), 0, env);
    REQUIRE(std::holds_alternative<Int16>(res4.value));
    REQUIRE(std::get<Int16>(res4.value).v == 3);

    auto res5 = ev.evaluate(asciiExpr("10 MOD 3"), 0, env);
    REQUIRE(std::holds_alternative<Int16>(res5.value));
    REQUIRE(std::get<Int16>(res5.value).v == 1);
}

TEST_CASE("ExpressionEvaluator comparisons and logical", "[expr]") {
    ExpressionEvaluator ev(nullptr);
    Env env;

    auto r1 = ev.evaluate(asciiExpr("2=2"), 0, env);
    REQUIRE(std::get<Int16>(r1.value).v == -1);
    auto r2 = ev.evaluate(asciiExpr("2<>3"), 0, env);
    REQUIRE(std::get<Int16>(r2.value).v == -1);
    auto r3 = ev.evaluate(asciiExpr("2<3"), 0, env);
    REQUIRE(std::get<Int16>(r3.value).v == -1);

    auto r4 = ev.evaluate(asciiExpr("NOT 0"), 0, env);
    REQUIRE(std::get<Int16>(r4.value).v == -1);
    auto r5 = ev.evaluate(asciiExpr("(1 AND 0) OR 1"), 0, env);
    REQUIRE(std::get<Int16>(r5.value).v == -1);
}

TEST_CASE("ExpressionEvaluator string functions", "[expr]") {
    ExpressionEvaluator ev(nullptr);
    Env env;

    auto r1 = ev.evaluate(asciiExpr("LEN(\"HELLO\")"), 0, env);
    REQUIRE(std::holds_alternative<Int16>(r1.value));
    REQUIRE(std::get<Int16>(r1.value).v == 5);

    auto r2 = ev.evaluate(asciiExpr("ASC(\"A\")"), 0, env);
    REQUIRE(std::holds_alternative<Int16>(r2.value));
    REQUIRE(std::get<Int16>(r2.value).v == 65);

    auto r3 = ev.evaluate(asciiExpr("CHR$(65)"), 0, env);
    REQUIRE(std::holds_alternative<Str>(r3.value));
    REQUIRE(std::get<Str>(r3.value).v == "A");

    auto r4 = ev.evaluate(asciiExpr("LEFT$(\"HELLO\", 3)"), 0, env);
    REQUIRE(std::holds_alternative<Str>(r4.value));
    REQUIRE(std::get<Str>(r4.value).v == "HEL");

    auto r5 = ev.evaluate(asciiExpr("RIGHT$(\"HELLO\", 2)"), 0, env);
    REQUIRE(std::holds_alternative<Str>(r5.value));
    REQUIRE(std::get<Str>(r5.value).v == "LO");

    auto r6 = ev.evaluate(asciiExpr("MID$(\"HELLO\", 2, 3)"), 0, env);
    REQUIRE(std::holds_alternative<Str>(r6.value));
    REQUIRE(std::get<Str>(r6.value).v == "ELL");
}

TEST_CASE("ExpressionEvaluator math functions", "[expr]") {
    ExpressionEvaluator ev(nullptr);
    Env env;

    auto r1 = ev.evaluate(asciiExpr("ABS(-5)"), 0, env);
    REQUIRE(std::holds_alternative<Double>(r1.value));
    REQUIRE(std::get<Double>(r1.value).v == Approx(5.0));

    auto r2 = ev.evaluate(asciiExpr("SGN(-3)"), 0, env);
    REQUIRE(std::holds_alternative<Int16>(r2.value));
    REQUIRE(std::get<Int16>(r2.value).v == -1);

    auto r3 = ev.evaluate(asciiExpr("SGN(5)"), 0, env);
    REQUIRE(std::holds_alternative<Int16>(r3.value));
    REQUIRE(std::get<Int16>(r3.value).v == 1);

    auto r4 = ev.evaluate(asciiExpr("INT(5.7)"), 0, env);
    REQUIRE(std::holds_alternative<Int16>(r4.value));
    REQUIRE(std::get<Int16>(r4.value).v == 5);

    auto r5 = ev.evaluate(asciiExpr("SQR(9)"), 0, env);
    REQUIRE(std::holds_alternative<Double>(r5.value));
    REQUIRE(std::get<Double>(r5.value).v == Approx(3.0));

    auto r6 = ev.evaluate(asciiExpr("SIN(0)"), 0, env);
    REQUIRE(std::holds_alternative<Double>(r6.value));
    REQUIRE(std::get<Double>(r6.value).v == Approx(0.0));
}

TEST_CASE("ExpressionEvaluator type conversion functions", "[expr]") {
    ExpressionEvaluator ev(nullptr);
    Env env;

    auto r1 = ev.evaluate(asciiExpr("CINT(5.7)"), 0, env);
    REQUIRE(std::holds_alternative<Int16>(r1.value));
    REQUIRE(std::get<Int16>(r1.value).v == 5);

    auto r2 = ev.evaluate(asciiExpr("VAL(\"123\")"), 0, env);
    REQUIRE(std::holds_alternative<Int16>(r2.value));
    REQUIRE(std::get<Int16>(r2.value).v == 123);

    auto r3 = ev.evaluate(asciiExpr("STR$(42)"), 0, env);
    REQUIRE(std::holds_alternative<Str>(r3.value));
    REQUIRE(std::get<Str>(r3.value).v == " 42");  // GW-BASIC adds leading space for positive numbers
}

TEST_CASE("ExpressionEvaluator array element access", "[expr]") {
    ExpressionEvaluator ev(nullptr);
    Env env;

    // Simulate a simple 1D array A(0) = 10, A(1) = 20, A(2) = 30
    std::unordered_map<std::string, std::vector<int16_t>> arrays;
    arrays["A"] = {10, 20, 30};
    arrays["B"] = {100, 200, 300, 400};

    // Set up array element resolver
    env.getArrayElem = [&arrays](const std::string& name, const std::vector<Value>& indices, Value& out) -> bool {
        auto it = arrays.find(name);
        if (it == arrays.end()) return false;
        
        if (indices.size() != 1) return false; // Only 1D arrays for this test
        
        if (!std::holds_alternative<Int16>(indices[0])) return false;
        int16_t index = std::get<Int16>(indices[0]).v;
        
        if (index < 0 || index >= static_cast<int16_t>(it->second.size())) return false;
        
        out = Int16{it->second[index]};
        return true;
    };

    // Test basic array access
    auto r1 = ev.evaluate(asciiExpr("A(0)"), 0, env);
    REQUIRE(std::holds_alternative<Int16>(r1.value));
    REQUIRE(std::get<Int16>(r1.value).v == 10);

    auto r2 = ev.evaluate(asciiExpr("A(2)"), 0, env);
    REQUIRE(std::holds_alternative<Int16>(r2.value));
    REQUIRE(std::get<Int16>(r2.value).v == 30);

    // Test square bracket syntax
    auto r3 = ev.evaluate(asciiExpr("B[1]"), 0, env);
    REQUIRE(std::holds_alternative<Int16>(r3.value));
    REQUIRE(std::get<Int16>(r3.value).v == 200);

    // Test expression in array subscript
    auto r4 = ev.evaluate(asciiExpr("A(1+1)"), 0, env);
    REQUIRE(std::holds_alternative<Int16>(r4.value));
    REQUIRE(std::get<Int16>(r4.value).v == 30);

    // Test array access in arithmetic
    auto r5 = ev.evaluate(asciiExpr("A(0) + A(1)"), 0, env);
    REQUIRE(std::holds_alternative<Int16>(r5.value));
    REQUIRE(std::get<Int16>(r5.value).v == 30);
}
