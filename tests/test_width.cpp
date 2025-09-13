#include <catch2/catch_all.hpp>
#include "../src/InterpreterLoop/BasicDispatcher.hpp"
#include "../src/Tokenizer/Tokenizer.hpp"
#include "../src/ProgramStore/ProgramStore.hpp"
#include <memory>

using namespace std;

static vector<uint8_t> crunchStmt(Tokenizer& t, const string& s) {
    auto bytes = t.crunch(s);
    if (bytes.empty() || bytes.back() != 0x00) bytes.push_back(0x00);
    // Remove 0x0D LL HH if present
    size_t start = 0;
    if (bytes.size() >= 3 && bytes[0] == 0x0D) start = 3;
    return vector<uint8_t>(bytes.begin() + static_cast<ptrdiff_t>(start), bytes.end());
}

TEST_CASE("WIDTH basic valid/invalid") {
    auto tokenizer = make_shared<Tokenizer>();
    auto program = make_shared<ProgramStore>();
    string output;
    int lastWidth = 80;
    auto dispatcher = make_unique<BasicDispatcher>(tokenizer, program,
        [&](const string& s){ output += s; },
        [&](const string&){ return string{}; },
        nullptr,
        nullptr,
        nullptr,
        [&](int cols){ lastWidth = cols; return true; }
    );

    SECTION("WIDTH 40 works") {
        auto bytes = crunchStmt(*tokenizer, "WIDTH 40");
        REQUIRE_NOTHROW((*dispatcher)(bytes));
        REQUIRE(lastWidth == 40);
        REQUIRE(output.find("WIDTH 40") != string::npos);
    }

    SECTION("WIDTH 80 works") {
        auto bytes = crunchStmt(*tokenizer, "WIDTH 80");
        REQUIRE_NOTHROW((*dispatcher)(bytes));
        REQUIRE(lastWidth == 80);
    }

    SECTION("Invalid width error") {
        auto bytes = crunchStmt(*tokenizer, "WIDTH 50");
        REQUIRE_THROWS((*dispatcher)(bytes));
    }
}
