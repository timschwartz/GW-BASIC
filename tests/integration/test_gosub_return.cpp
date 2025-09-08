#include <catch2/catch_all.hpp>
#include <memory>
#include <vector>
#include <string>
#include <sstream>

#include "../src/InterpreterLoop/InterpreterLoop.hpp"
#include "../src/InterpreterLoop/BasicDispatcher.hpp"
#include "../src/ProgramStore/ProgramStore.hpp"
#include "../src/Tokenizer/Tokenizer.hpp"

static std::vector<uint8_t> crunchLine(Tokenizer& t, const std::string& source) {
    auto bytes = t.crunch(source);
    if (bytes.empty() || bytes.back() != 0x00) bytes.push_back(0x00);
    
    // If the line starts with a line number token (0x0D), skip past it
    // Format: 0x0D <low byte> <high byte> <statement tokens...> 0x00
    size_t start = 0;
    if (bytes.size() >= 3 && bytes[0] == 0x0D) {
        start = 3; // skip the 0x0D and 2-byte line number
    }
    
    // Return just the statement part 
    std::vector<uint8_t> result(bytes.begin() + start, bytes.end());
    return result;
}

TEST_CASE("BasicDispatcher GOSUB/RETURN integration", "[integration]") {
    auto store = std::make_shared<ProgramStore>();
    auto tokenizer = std::make_shared<Tokenizer>();

    // Test program:
    // 10 PRINT "START"
    // 20 GOSUB 100
    // 30 PRINT "MIDDLE" 
    // 40 GOSUB 200
    // 50 PRINT "END"
    // 60 END
    // 100 PRINT "SUB1"
    // 110 RETURN
    // 200 PRINT "SUB2"
    // 210 RETURN
    
    store->insertLine(10, crunchLine(*tokenizer, "10 PRINT \"START\""));
    store->insertLine(20, crunchLine(*tokenizer, "20 GOSUB 100"));
    store->insertLine(30, crunchLine(*tokenizer, "30 PRINT \"MIDDLE\""));
    store->insertLine(40, crunchLine(*tokenizer, "40 GOSUB 200"));
    store->insertLine(50, crunchLine(*tokenizer, "50 PRINT \"END\""));
    store->insertLine(60, crunchLine(*tokenizer, "60 END"));
    store->insertLine(100, crunchLine(*tokenizer, "100 PRINT \"SUB1\""));
    store->insertLine(110, crunchLine(*tokenizer, "110 RETURN"));
    store->insertLine(200, crunchLine(*tokenizer, "200 PRINT \"SUB2\""));
    store->insertLine(210, crunchLine(*tokenizer, "210 RETURN"));

    InterpreterLoop loop(store, tokenizer);
    
    // Capture output
    std::vector<std::string> output;
    BasicDispatcher dispatcher(tokenizer, store, [&output](const std::string& s) {
        output.push_back(s);
    });
    
    // Track execution order
    std::vector<uint16_t> executionOrder;
    loop.setTrace(true);
    loop.setTraceCallback([&](uint16_t line, const std::vector<uint8_t>&) { 
        executionOrder.push_back(line); 
    });
    
    loop.setStatementHandler([&](const std::vector<uint8_t>& bytes, uint16_t currentLine) {
        try {
            auto jumpTarget = dispatcher(bytes, currentLine);
            if (jumpTarget == 0xFFFF) { 
                loop.stop(); 
                return uint16_t{0}; 
            }
            return jumpTarget;
        } catch (const std::exception& e) {
            std::cerr << "Error at line " << currentLine << ": " << e.what() << std::endl;
            std::cerr << "Tokenized line: ";
            for (auto b : bytes) {
                std::cerr << "0x" << std::hex << (int)b << " ";
            }
            std::cerr << std::endl;
            std::cerr << "Detokenized: " << tokenizer->detokenize(bytes) << std::endl;
            FAIL("Exception during execution: " << e.what());
            return uint16_t{0};
        }
    });
    
    REQUIRE_NOTHROW(loop.run());
    
    // Expected execution order: 10, 20, 100, 110, 30, 40, 200, 210, 50, 60
    REQUIRE(executionOrder == std::vector<uint16_t>({10, 20, 100, 110, 30, 40, 200, 210, 50, 60}));
    
    // Expected output: "START", "SUB1", "MIDDLE", "SUB2", "END"
    REQUIRE(output.size() == 5);
    REQUIRE(output[0] == "START\n");
    REQUIRE(output[1] == "SUB1\n");
    REQUIRE(output[2] == "MIDDLE\n");
    REQUIRE(output[3] == "SUB2\n");
    REQUIRE(output[4] == "END\n");
}

TEST_CASE("BasicDispatcher RETURN without GOSUB error", "[integration]") {
    auto store = std::make_shared<ProgramStore>();
    auto tokenizer = std::make_shared<Tokenizer>();

    // Test program with invalid RETURN
    // 10 PRINT "TEST"
    // 20 RETURN
    
    store->insertLine(10, crunchLine(*tokenizer, "10 PRINT \"TEST\""));
    store->insertLine(20, crunchLine(*tokenizer, "20 RETURN"));

    InterpreterLoop loop(store, tokenizer);
    BasicDispatcher dispatcher(tokenizer, store);
    
    bool errorOccurred = false;
    std::string errorMessage;
    
    loop.setStatementHandler([&](const std::vector<uint8_t>& bytes, uint16_t currentLine) {
        try {
            auto jumpTarget = dispatcher(bytes, currentLine);
            if (jumpTarget == 0xFFFF) { 
                loop.stop(); 
                return uint16_t{0}; 
            }
            return jumpTarget;
        } catch (const std::exception& e) {
            errorOccurred = true;
            errorMessage = e.what();
            loop.stop();
            return uint16_t{0};
        }
    });
    
    REQUIRE_NOTHROW(loop.run());
    REQUIRE(errorOccurred);
    REQUIRE(errorMessage.find("RETURN without GOSUB") != std::string::npos);
}
