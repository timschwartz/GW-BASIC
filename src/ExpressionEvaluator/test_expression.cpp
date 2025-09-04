#include <catch2/catch_all.hpp>
#include <memory>
#include <vector>
#include <string>

#include "ExpressionEvaluator.hpp"

using namespace expr;

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
