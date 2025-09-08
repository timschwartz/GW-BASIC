#include <iostream>
#include <memory>
#include <vector>

#include "InterpreterLoop.hpp"
#include "../ProgramStore/ProgramStore.hpp"
#include "../Tokenizer/Tokenizer.hpp"
#include "BasicDispatcher.hpp"

int main() {
    auto store = std::make_shared<ProgramStore>();
    auto tokenizer = std::make_shared<Tokenizer>();

    // Create dispatcher with program store
    BasicDispatcher disp(tokenizer, store);
    
    std::cout << "Testing LOAD command..." << std::endl;
    
    // Test LOAD command with the test.bas file
    std::string loadCmd = "LOAD \"test.bas\"";
    auto loadTokens = tokenizer->crunch(loadCmd);
    
    try {
        uint16_t result = disp(loadTokens);
        std::cout << "LOAD command result: " << result << std::endl;
        
        // Now check if the program was loaded by running it
        InterpreterLoop loop(store, tokenizer);
        loop.setStatementHandler([&](const std::vector<uint8_t>& bytes, uint16_t currentLine) -> uint16_t {
            try {
                return disp(bytes, currentLine);
            } catch (const std::exception& e) {
                std::cerr << "Error: " << e.what() << std::endl;
                return 0xFFFF;
            }
        });
        
        std::cout << "Running loaded program..." << std::endl;
        loop.run();
        
    } catch (const std::exception& e) {
        std::cerr << "Error testing LOAD: " << e.what() << std::endl;
    }
    
    return 0;
}
