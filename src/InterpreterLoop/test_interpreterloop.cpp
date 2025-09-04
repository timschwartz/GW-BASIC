#include <catch2/catch_all.hpp>
#include <memory>
#include <vector>
#include <string>

#include "InterpreterLoop.hpp"
#include "../ProgramStore/ProgramStore.hpp"
#include "../Tokenizer/Tokenizer.hpp"

static std::vector<uint8_t> crunchLine(Tokenizer& t, const std::string& source) {
    auto bytes = t.crunch(source);
    if (bytes.empty() || bytes.back() != 0x00) bytes.push_back(0x00);
    return bytes;
}

TEST_CASE("InterpreterLoop runs sequentially and halts at end", "[interpreter]") {
    auto store = std::make_shared<ProgramStore>();
    auto tokenizer = std::make_shared<Tokenizer>();

    store->insertLine(10, crunchLine(*tokenizer, "10 PRINT \"A\""));
    store->insertLine(20, crunchLine(*tokenizer, "20 PRINT \"B\""));
    store->insertLine(30, crunchLine(*tokenizer, "30 END"));

    InterpreterLoop loop(store, tokenizer);

    std::vector<uint16_t> visited;
    loop.setTrace(true);
    loop.setTraceCallback([&](uint16_t line, const std::vector<uint8_t>&){ visited.push_back(line); });
    loop.setStatementHandler([](const std::vector<uint8_t>&){ return uint16_t{0}; });

    REQUIRE_NOTHROW(loop.run());
    REQUIRE(visited == std::vector<uint16_t>({10, 20, 30}));
}

TEST_CASE("InterpreterLoop supports jump override", "[interpreter]") {
    auto store = std::make_shared<ProgramStore>();
    auto tokenizer = std::make_shared<Tokenizer>();

    store->insertLine(10, crunchLine(*tokenizer, "10 PRINT \"FIRST\""));
    store->insertLine(20, crunchLine(*tokenizer, "20 PRINT \"SECOND\""));
    store->insertLine(30, crunchLine(*tokenizer, "30 END"));

    InterpreterLoop loop(store, tokenizer);

    std::vector<uint16_t> visited;
    loop.setTrace(true);
    loop.setTraceCallback([&](uint16_t line, const std::vector<uint8_t>&){ visited.push_back(line); });
    loop.setStatementHandler([&](const std::vector<uint8_t>& bytes){
        auto text = tokenizer->detokenize(bytes);
        if (text.find("FIRST") != std::string::npos) return uint16_t{30};
        return uint16_t{0};
    });

    loop.run();
    REQUIRE(visited == std::vector<uint16_t>({10, 30}));
}

TEST_CASE("InterpreterLoop executeImmediate runs handler and traces line 0", "[interpreter]") {
    auto store = std::make_shared<ProgramStore>();
    auto tokenizer = std::make_shared<Tokenizer>();

    InterpreterLoop loop(store, tokenizer);
    int handled = 0;
    std::vector<uint16_t> traceLines;
    loop.setTrace(true);
    loop.setTraceCallback([&](uint16_t line, const std::vector<uint8_t>&){ traceLines.push_back(line); });
    loop.setStatementHandler([&](const std::vector<uint8_t>&){ handled++; return uint16_t{0}; });

    bool ok = loop.executeImmediate("PRINT \"IMM\"");
    REQUIRE(ok);
    REQUIRE(handled == 1);
    REQUIRE_FALSE(traceLines.empty());
    REQUIRE(traceLines.back() == 0); // immediate mode traced as line 0
}

TEST_CASE("InterpreterLoop halts on invalid jump target", "[interpreter]") {
    auto store = std::make_shared<ProgramStore>();
    auto tokenizer = std::make_shared<Tokenizer>();

    store->insertLine(10, crunchLine(*tokenizer, "10 PRINT \"A\""));
    store->insertLine(20, crunchLine(*tokenizer, "20 END"));

    InterpreterLoop loop(store, tokenizer);
    std::vector<uint16_t> visited;
    loop.setTrace(true);
    loop.setTraceCallback([&](uint16_t line, const std::vector<uint8_t>&){ visited.push_back(line); });
    bool first = true;
    loop.setStatementHandler([&](const std::vector<uint8_t>&){
        if (first) { first = false; return uint16_t{9999}; }
        return uint16_t{0};
    });

    loop.run();
    REQUIRE(visited == std::vector<uint16_t>({10}));
}

TEST_CASE("InterpreterLoop stop and cont with setCurrentLine", "[interpreter]") {
    auto store = std::make_shared<ProgramStore>();
    auto tokenizer = std::make_shared<Tokenizer>();

    store->insertLine(10, crunchLine(*tokenizer, "10 PRINT \"A\""));
    store->insertLine(20, crunchLine(*tokenizer, "20 PRINT \"B\""));
    store->insertLine(30, crunchLine(*tokenizer, "30 PRINT \"C\""));
    store->insertLine(40, crunchLine(*tokenizer, "40 END"));

    InterpreterLoop loop(store, tokenizer);
    std::vector<uint16_t> visited;
    loop.setTrace(true);
    loop.setTraceCallback([&](uint16_t line, const std::vector<uint8_t>&){ visited.push_back(line); });
    loop.setStatementHandler([&](const std::vector<uint8_t>& bytes){
        auto text = tokenizer->detokenize(bytes);
        if (text.find("B") != std::string::npos) {
            loop.stop();
        }
        return uint16_t{0};
    });

    loop.run();
    REQUIRE(visited == std::vector<uint16_t>({10, 20}));

    // Resume from an explicit line and continue to the end
    loop.setCurrentLine(30);
    loop.cont();
    REQUIRE(visited == std::vector<uint16_t>({10, 20, 30, 40}));
}
