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
    loop.setStatementHandler([](const std::vector<uint8_t>&, uint16_t){ return uint16_t{0}; });

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
    loop.setStatementHandler([&](const std::vector<uint8_t>& bytes, uint16_t){
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
    loop.setStatementHandler([&](const std::vector<uint8_t>&, uint16_t){ handled++; return uint16_t{0}; });

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
    loop.setStatementHandler([&](const std::vector<uint8_t>&, uint16_t){
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
    loop.setStatementHandler([&](const std::vector<uint8_t>& bytes, uint16_t){
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

TEST_CASE("InterpreterLoop GOSUB/RETURN control flow", "[interpreter]") {
    auto store = std::make_shared<ProgramStore>();
    auto tokenizer = std::make_shared<Tokenizer>();

    // Program:
    // 10 PRINT "MAIN1"
    // 20 GOSUB 100
    // 30 PRINT "MAIN2"  
    // 40 END
    // 100 PRINT "SUB"
    // 110 RETURN
    
    store->insertLine(10, crunchLine(*tokenizer, "10 PRINT \"MAIN1\""));
    store->insertLine(20, crunchLine(*tokenizer, "20 GOSUB 100"));
    store->insertLine(30, crunchLine(*tokenizer, "30 PRINT \"MAIN2\""));
    store->insertLine(40, crunchLine(*tokenizer, "40 END"));
    store->insertLine(100, crunchLine(*tokenizer, "100 PRINT \"SUB\""));
    store->insertLine(110, crunchLine(*tokenizer, "110 RETURN"));

    InterpreterLoop loop(store, tokenizer);
    std::vector<uint16_t> visited;
    std::vector<std::string> output;
    
    loop.setTrace(true);
    loop.setTraceCallback([&](uint16_t line, const std::vector<uint8_t>&){ visited.push_back(line); });
    
    // Simple mock statement handler that handles GOSUB/RETURN
    std::vector<std::pair<uint16_t, uint32_t>> gosubStack;
    
    loop.setStatementHandler([&](const std::vector<uint8_t>& bytes, uint16_t currentLine) -> uint16_t {
        auto text = tokenizer->detokenize(bytes);
        
        if (text.find("PRINT") != std::string::npos) {
            // Extract and store the printed text for verification
            size_t start = text.find('"');
            if (start != std::string::npos) {
                size_t end = text.find('"', start + 1);
                if (end != std::string::npos) {
                    output.push_back(text.substr(start + 1, end - start - 1));
                }
            }
            return 0; // continue
        }
        
        if (text.find("GOSUB") != std::string::npos) {
            // Extract target line number (simplified parsing)
            size_t pos = text.find("GOSUB") + 6;
            while (pos < text.size() && text[pos] == ' ') pos++;
            uint16_t targetLine = 0;
            while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') {
                targetLine = targetLine * 10 + (text[pos] - '0');
                pos++;
            }
            
            // Push return info onto mock stack
            gosubStack.push_back({currentLine, 0});
            return targetLine; // jump to subroutine
        }
        
        if (text.find("RETURN") != std::string::npos) {
            if (gosubStack.empty()) {
                return 0; // error case, but don't crash test
            }
            
            auto frame = gosubStack.back();
            gosubStack.pop_back();
            
            // Return to line after GOSUB
            auto nextIt = store->getNextLine(frame.first);
            if (nextIt.isValid()) {
                return nextIt->lineNumber;
            }
            return 0;
        }
        
        if (text.find("END") != std::string::npos) {
            loop.stop();
            return 0;
        }
        
        return 0; // continue
    });

    loop.run();
    
    // Verify execution order: 10 -> 20 -> 100 -> 110 -> 30 -> 40
    REQUIRE(visited == std::vector<uint16_t>({10, 20, 100, 110, 30, 40}));
    
    // Verify output order: "MAIN1", "SUB", "MAIN2"
    REQUIRE(output == std::vector<std::string>({"MAIN1", "SUB", "MAIN2"}));
}
