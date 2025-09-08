#include <catch2/catch_all.hpp>
#include <memory>
#include <string>
#include <sstream>
#include "BasicDispatcher.hpp"
#include "../Tokenizer/Tokenizer.hpp"

static std::vector<uint8_t> crunchStmt(Tokenizer& t, const std::string& source) {
    auto bytes = t.crunch(source);
    // Ensure single statement bytes end at 0x00
    if (bytes.empty() || bytes.back() != 0x00) bytes.push_back(0x00);
    return bytes;
}

TEST_CASE("BasicDispatcher PRINT literal", "[dispatcher]") {
    Tokenizer tok;
    auto disp = BasicDispatcher(std::make_shared<Tokenizer>(tok));
    auto stmt = crunchStmt(tok, "PRINT \"X\"");
    std::ostringstream out;
    auto* oldBuf = std::cout.rdbuf(out.rdbuf());
    auto r = disp(stmt);
    std::cout.rdbuf(oldBuf);
    REQUIRE(r == 0);
    REQUIRE(out.str() == std::string("X\n"));
}

TEST_CASE("BasicDispatcher LET + IF THEN ELSE inline", "[dispatcher]") {
    Tokenizer tok;
    auto disp = BasicDispatcher(std::make_shared<Tokenizer>(tok));
    auto stmt = crunchStmt(tok, "A=0:IF A THEN PRINT \"T\" ELSE PRINT \"F\"");
    std::ostringstream out;
    auto* oldBuf = std::cout.rdbuf(out.rdbuf());
    auto r = disp(stmt);
    std::cout.rdbuf(oldBuf);
    REQUIRE(r == 0);
    REQUIRE(out.str() == std::string("F\n"));
}

TEST_CASE("BasicDispatcher GOTO returns jump target", "[dispatcher]") {
    Tokenizer tok;
    auto disp = BasicDispatcher(std::make_shared<Tokenizer>(tok));
    auto stmt = crunchStmt(tok, "GOTO 120");
    auto r = disp(stmt);
    REQUIRE(r == 120);
}

TEST_CASE("BasicDispatcher GOSUB returns jump target", "[dispatcher]") {
    Tokenizer tok;
    auto disp = BasicDispatcher(std::make_shared<Tokenizer>(tok));
    auto stmt = crunchStmt(tok, "GOSUB 200");
    auto r = disp(stmt);
    REQUIRE(r == 200);
}

TEST_CASE("BasicDispatcher RETURN without GOSUB throws error", "[dispatcher]") {
    Tokenizer tok;
    auto disp = BasicDispatcher(std::make_shared<Tokenizer>(tok));
    auto stmt = crunchStmt(tok, "RETURN");
    REQUIRE_THROWS_AS(disp(stmt), expr::BasicError);
}
