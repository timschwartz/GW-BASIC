#include <catch2/catch_all.hpp>
#include <memory>
#include <vector>
#include <string>

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
