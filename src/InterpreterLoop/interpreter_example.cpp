#include <iostream>
#include <memory>
#include <vector>

#include "InterpreterLoop.hpp"
#include "../ProgramStore/ProgramStore.hpp"
#include "../Tokenizer/Tokenizer.hpp"

// Super-minimal demo statement handler:
// - If first nonzero byte is PRINT (0x90) then detokenize and echo
// - If first nonzero byte is END (0x80) then signal termination by returning 0 and stopping externally
static uint16_t demoHandler(Tokenizer& tokenizer, const std::vector<uint8_t>& tokens) {
    uint8_t first = 0;
    for (auto b : tokens) { if (b != 0x00) { first = b; break; } }
    if (first == 0x80 /* END */) {
        return 0; // fall-through will halt at end of program
    }
    if (first == 0x90 /* PRINT */) {
        std::cout << tokenizer.detokenize(tokens) << std::endl;
    }
    return 0; // continue
}

int main() {
    auto store = std::make_shared<ProgramStore>();
    auto tokenizer = std::make_shared<Tokenizer>();

    // Populate a tiny program: 10 PRINT "HI" : 20 END
    {
        auto t10 = tokenizer->crunch("10 PRINT \"HI\"");
        t10.push_back(0x00);
        store->insertLine(10, t10);
        auto t20 = tokenizer->crunch("20 END");
        t20.push_back(0x00);
        store->insertLine(20, t20);
    }

    InterpreterLoop loop(store, tokenizer);
    loop.setStatementHandler([&](const std::vector<uint8_t>& tokens){
        return demoHandler(*tokenizer, tokens);
    });
    loop.setTrace(true);
    loop.setTraceCallback([&](uint16_t line, const std::vector<uint8_t>& tokens){
        if (line) std::cerr << "[TRACE] line " << line << "\n";
        else std::cerr << "[TRACE] immediate" << "\n";
    });

    loop.run();
    return 0;
}
