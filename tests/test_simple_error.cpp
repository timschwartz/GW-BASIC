#include <catch2/catch_all.hpp>
#include "../src/InterpreterLoop/InterpreterLoop.hpp"
#include "../src/InterpreterLoop/BasicDispatcher.hpp"
#include "../src/ProgramStore/ProgramStore.hpp"
#include "../src/Tokenizer/Tokenizer.hpp"
#include "../src/Runtime/GWError.hpp"
#include <memory>
#include <sstream>

using namespace gwbasic;

static std::vector<uint8_t> crunchLine(Tokenizer& t, const std::string& source) {
    auto bytes = t.crunch(source);
    if (bytes.empty() || bytes.back() != 0x00) bytes.push_back(0x00);
    
    // If the line starts with a line number token (0x0D), skip past it
    // Format: 0x0D <low byte> <high byte> <statement tokens...> 0x00
    size_t start = 0;
    if (bytes.size() >= 3 && bytes[0] == 0x0D) {
        start = 3; // skip the 0x0D and 2-byte line number
    }
    
    return std::vector<uint8_t>(bytes.begin() + start, bytes.end());
}

class TestOutputCapture {
public:
    std::string output;
    std::string lastInput;
    
    void print(const std::string& text) {
        output += text;
    }
    
    std::string input(const std::string& prompt) {
        return lastInput;
    }
    
    void clear() {
        output.clear();
        lastInput.clear();
    }
};

TEST_CASE("Simple Error Handling Test") {
    auto tokenizer = std::make_shared<Tokenizer>();
    auto program = std::make_shared<ProgramStore>();
    auto loop = std::make_unique<InterpreterLoop>(program, tokenizer);
    
    TestOutputCapture capture;
    auto dispatcher = std::make_unique<BasicDispatcher>(
        tokenizer, program,
        [&capture](const std::string& s) { capture.print(s); },
        [&capture](const std::string& s) { return capture.input(s); }
    );
    
    // Wire up the components
    loop->setStatementHandler([&dispatcher](const std::vector<uint8_t>& tokens, uint16_t line) -> uint16_t {
        return (*dispatcher)(tokens, line);
    });
    loop->setRuntimeStack(dispatcher->getRuntimeStack());
    
    SECTION("Basic division by zero with complex ERR expression") {
        // Test program:
        // 10 ON ERROR GOTO 100
        // 20 A = 1 / 0
        // 30 PRINT "Should not reach here"
        // 100 PRINT "Error handled: simple"
        
        std::string prog1 = "10 ON ERROR GOTO 100\n";
        std::string prog2 = "20 A = 1 / 0\n";
        std::string prog3 = "30 PRINT \"Should not reach here\"\n";
        std::string prog4 = "100 PRINT \"Error handled: simple\"\n";
        
        auto line10 = crunchLine(*tokenizer, prog1);
        auto line20 = crunchLine(*tokenizer, prog2);
        auto line30 = crunchLine(*tokenizer, prog3);
        auto line100 = crunchLine(*tokenizer, prog4);
        
        program->insertLine(10, line10);
        program->insertLine(20, line20);
        program->insertLine(30, line30);
        program->insertLine(100, line100);
        
        capture.clear();
        loop->run();
        
        // Should have jumped to error handler and printed error info
        REQUIRE(capture.output.find("Error handled: simple") != std::string::npos);
        REQUIRE(capture.output.find("Should not reach here") == std::string::npos);
        
        // Check that we have an error frame on the stack
        auto errFrame = dispatcher->getRuntimeStack()->topErr();
        REQUIRE(errFrame != nullptr);
        REQUIRE(errFrame->errCode == 11); // Division by zero error code
    }
}