#include <iostream>
#include <memory>
#include <vector>

#include "InterpreterLoop.hpp"
#include "../ProgramStore/ProgramStore.hpp"
#include "../Tokenizer/Tokenizer.hpp"
#include "BasicDispatcher.hpp"

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
        // Crunch without the line number; ProgramStore inserts line number separately
        auto t10 = tokenizer->crunch("PRINT \"HI\"");
        store->insertLine(10, t10);
        auto t20 = tokenizer->crunch("END");
        store->insertLine(20, t20);
    }

    // Add a line with multiple statements and IF/ELSE to smoke test dispatcher
    {
        auto t15 = tokenizer->crunch("A=1:IF A THEN PRINT \"T\" ELSE PRINT \"F\"");
        store->insertLine(15, t15);
    }

        InterpreterLoop loop(store, tokenizer);
        loop.setTrace(true);
        loop.setTraceCallback([&](uint16_t line, const std::vector<uint8_t>&){
            std::cout << "[TRACE] Executing line " << line << std::endl;
        });
        BasicDispatcher disp(tokenizer);
        loop.setStatementHandler([&](const std::vector<uint8_t>& bytes){
            try {
                auto j = disp(bytes);
                if (j == 0xFFFF) { loop.stop(); return uint16_t{0}; }
                return j;
            } catch (const expr::BasicError& e) {
                std::cerr << "[RUNTIME ERROR] " << e.what() << "\n";
                std::cerr << "Line: " << tokenizer->detokenize(bytes) << "\n";
                loop.stop();
                return uint16_t{0};
            }
        });

    loop.run();
    return 0;
}
