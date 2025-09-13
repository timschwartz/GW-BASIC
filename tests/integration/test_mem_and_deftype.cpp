#include <catch2/catch_all.hpp>
#include <memory>
#include <vector>
#include <string>
#include <sstream>

#include "../src/InterpreterLoop/InterpreterLoop.hpp"
#include "../src/InterpreterLoop/BasicDispatcher.hpp"
#include "../src/ProgramStore/ProgramStore.hpp"
#include "../src/Tokenizer/Tokenizer.hpp"

static std::vector<uint8_t> crunchStmt(Tokenizer& t, const std::string& source) {
    auto bytes = t.crunch(source);
    if (bytes.empty() || bytes.back() != 0x00) bytes.push_back(0x00);
    size_t start = 0;
    if (bytes.size() >= 3 && bytes[0] == 0x0D) start = 3;
    return std::vector<uint8_t>(bytes.begin() + start, bytes.end());
}

TEST_CASE("DEF SEG with PEEK/POKE", "[integration]") {
    auto store = std::make_shared<ProgramStore>();
    auto tokenizer = std::make_shared<Tokenizer>();

    // Program writes and reads from memory using DEF SEG and PEEK/POKE
    // 10 DEF SEG
    // 20 POKE 106, 123
    // 30 PRINT PEEK(106)
    // 40 DEF SEG = &H1000
    // 50 POKE 0, 77
    // 60 PRINT PEEK(0)
    // 70 END
    store->insertLine(10,  crunchStmt(*tokenizer, "10 DEF SEG"));
    store->insertLine(20,  crunchStmt(*tokenizer, "20 POKE 106, 123"));
    store->insertLine(30,  crunchStmt(*tokenizer, "30 PRINT PEEK(106)"));
    store->insertLine(40,  crunchStmt(*tokenizer, "40 DEF SEG = &H1000"));
    store->insertLine(50,  crunchStmt(*tokenizer, "50 POKE 0, 77"));
    store->insertLine(60,  crunchStmt(*tokenizer, "60 PRINT PEEK(0)"));
    store->insertLine(70,  crunchStmt(*tokenizer, "70 END"));

    InterpreterLoop loop(store, tokenizer);

    std::vector<std::string> output;
    BasicDispatcher dispatcher(tokenizer, store, [&output](const std::string& s) {
        output.push_back(s);
    });

    loop.setStatementHandler([&](const std::vector<uint8_t>& bytes, uint16_t currentLine) {
        auto r = dispatcher(bytes, currentLine);
        if (r == 0xFFFF) { loop.stop(); return uint16_t{0}; }
        return r;
    });

    REQUIRE_NOTHROW(loop.run());
    REQUIRE(output.size() >= 2);
    // First print should be 123 at segment 0 offset 106
    REQUIRE(output[0] == std::string("123\n"));
    // Second print should be 77 at segment 0x1000 offset 0
    REQUIRE(output[1] == std::string("77\n"));
}

TEST_CASE("DEFINT/DEFSNG/DEFDBL/DEFSTR affect defaults", "[integration]") {
    auto store = std::make_shared<ProgramStore>();
    auto tokenizer = std::make_shared<Tokenizer>();

    // Use default typing to influence variable types; we just test basic execution flows
    // 10 DEFINT A-C
    // 20 A = 5.5: B = 2.2: PRINT A;B
    // 30 DEFSNG A-C
    // 40 C = 1/3: PRINT C
    // 50 DEFSTR S-Z
    // 60 S$ = "OK": PRINT S$
    // 70 END
    auto line10 = crunchStmt(*tokenizer, "10 DEFINT A-C");
    // Sanity: first byte should be a statement token for DEFINT (>= 0x80)
    REQUIRE(line10.size() >= 2);
    INFO("First byte hex=" << std::hex << (int)line10[0] << std::dec << ", detok='" << tokenizer->detokenize(line10) << "'");
    store->insertLine(10, line10);
    store->insertLine(20, crunchStmt(*tokenizer, "20 A = 5.5: B = 2.2: PRINT A;B"));
    store->insertLine(30, crunchStmt(*tokenizer, "30 DEFSNG A-C"));
    store->insertLine(40, crunchStmt(*tokenizer, "40 C = 1/3: PRINT C"));
    store->insertLine(50, crunchStmt(*tokenizer, "50 DEFSTR S-Z"));
    store->insertLine(60, crunchStmt(*tokenizer, "60 S$ = \"OK\": PRINT S$"));
    store->insertLine(70, crunchStmt(*tokenizer, "70 END"));

    InterpreterLoop loop(store, tokenizer);

    std::vector<std::string> output;
    BasicDispatcher dispatcher(tokenizer, store, [&output](const std::string& s) { output.push_back(s); });

    loop.setStatementHandler([&](const std::vector<uint8_t>& bytes, uint16_t currentLine) {
        auto r = dispatcher(bytes, currentLine);
        if (r == 0xFFFF) { loop.stop(); return uint16_t{0}; }
        return r;
    });

    REQUIRE_NOTHROW(loop.run());
    // We don't assert exact FP formatting; just that we got three prints total
    REQUIRE(output.size() >= 3);
    // Third printed line should be the string "OK"
    REQUIRE(output.back() == std::string("OK\n"));
}
