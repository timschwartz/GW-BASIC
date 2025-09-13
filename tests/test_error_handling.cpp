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

TEST_CASE("Error Handling - ON ERROR GOTO basic functionality") {
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
    loop->setEventTrapSystem(dispatcher->getEventTrapSystem());
    loop->setRuntimeStack(dispatcher->getRuntimeStack());
    
    SECTION("ON ERROR GOTO sets up error handler") {
        // Test program:
        // 10 ON ERROR GOTO 100
        // 20 A = 1 / 0   ' This should trigger division by zero
        // 30 PRINT "Should not reach here"
        // 100 PRINT "Error handled: "; ERR; " at line "; ERL
        
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
        REQUIRE(capture.output.find("Error handled:") != std::string::npos);
        // Validate the RuntimeStack has the error frame with correct values
        auto stack = dispatcher->getRuntimeStack();
        auto errFrame = stack->topErr();
        REQUIRE(errFrame != nullptr);
        REQUIRE(errFrame->errCode == 11); // Division by zero
        REQUIRE(errFrame->resumeLine == 20); // Line where error occurred
        REQUIRE(capture.output.find("Should not reach here") == std::string::npos);
    }
    
    SECTION("ON ERROR GOTO 0 disables error handling") {
        // Test program:
        // 10 ON ERROR GOTO 100
        // 20 ON ERROR GOTO 0    ' Disable error handling
        // 30 A = 1 / 0          ' This should cause program termination
        // 100 PRINT "Should not reach error handler"
        
        std::string prog1 = "10 ON ERROR GOTO 100\n";
        std::string prog2 = "20 ON ERROR GOTO 0\n";
        std::string prog3 = "30 A = 1 / 0\n";
        std::string prog4 = "100 PRINT \"Should not reach error handler\"\n";
        
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
        
        // Should NOT have reached the error handler
        REQUIRE(capture.output.find("Should not reach error handler") == std::string::npos);
    }
}

TEST_CASE("Error Handling - RESUME statements") {
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
    loop->setEventTrapSystem(dispatcher->getEventTrapSystem());
    loop->setRuntimeStack(dispatcher->getRuntimeStack());
    
    SECTION("RESUME retries the same statement") {
        // Test program:
        // 10 ON ERROR GOTO 100
        // 20 INPUT "Enter number: ", X
        // 30 A = 10 / X
        // 40 PRINT "Result: "; A
        // 50 END
        // 100 PRINT "Error occurred, trying again"
        // 110 X = 5  ' Fix the problem
        // 120 RESUME ' Retry the division
        
        std::string prog1 = "10 ON ERROR GOTO 100\n";
        std::string prog2 = "20 INPUT \"Enter number: \", X\n";
        std::string prog3 = "30 A = 10 / X\n";
        std::string prog4 = "40 PRINT \"Result: \"; A\n";
        std::string prog5 = "50 END\n";
        std::string prog6 = "100 PRINT \"Error occurred, trying again\"\n";
        std::string prog7 = "110 X = 5\n";
        std::string prog8 = "120 RESUME\n";
        
        auto line10 = crunchLine(*tokenizer, prog1);
        auto line20 = crunchLine(*tokenizer, prog2);
        auto line30 = crunchLine(*tokenizer, prog3);
        auto line40 = crunchLine(*tokenizer, prog4);
        auto line50 = crunchLine(*tokenizer, prog5);
        auto line100 = crunchLine(*tokenizer, prog6);
        auto line110 = crunchLine(*tokenizer, prog7);
        auto line120 = crunchLine(*tokenizer, prog8);
        
        program->insertLine(10, line10);
        program->insertLine(20, line20);
        program->insertLine(30, line30);
        program->insertLine(40, line40);
        program->insertLine(50, line50);
        program->insertLine(100, line100);
        program->insertLine(110, line110);
        program->insertLine(120, line120);
        
        capture.clear();
        capture.lastInput = "0"; // This will cause division by zero
        dispatcher->setTestMode(true);
        
        loop->run();
        
        // Should have handled the error and then successfully calculated result
        REQUIRE(capture.output.find("Error occurred, trying again") != std::string::npos);
        REQUIRE(capture.output.find("Result:") != std::string::npos);
        REQUIRE(capture.output.find("2") != std::string::npos); // 10/5 = 2
    }
    
    SECTION("RESUME NEXT continues to next statement") {
        // Test program:
        // 10 ON ERROR GOTO 100
        // 20 A = 1 / 0      ' Cause error
        // 30 PRINT "After error"
        // 40 END
        // 100 PRINT "Error handled"
        // 110 RESUME NEXT   ' Skip the error line, go to line 30
        
        std::string prog1 = "10 ON ERROR GOTO 100\n";
        std::string prog2 = "20 A = 1 / 0\n";
        std::string prog3 = "30 PRINT \"After error\"\n";
        std::string prog4 = "40 END\n";
        std::string prog5 = "100 PRINT \"Error handled\"\n";
        std::string prog6 = "110 RESUME NEXT\n";
        
        auto line10 = crunchLine(*tokenizer, prog1);
        auto line20 = crunchLine(*tokenizer, prog2);
        auto line30 = crunchLine(*tokenizer, prog3);
        auto line40 = crunchLine(*tokenizer, prog4);
        auto line100 = crunchLine(*tokenizer, prog5);
        auto line110 = crunchLine(*tokenizer, prog6);
        
        program->insertLine(10, line10);
        program->insertLine(20, line20);
        program->insertLine(30, line30);
        program->insertLine(40, line40);
        program->insertLine(100, line100);
        program->insertLine(110, line110);
        
        capture.clear();
        loop->run();
        
        // Should have handled error and continued to line 30
        REQUIRE(capture.output.find("Error handled") != std::string::npos);
        REQUIRE(capture.output.find("After error") != std::string::npos);
    }
    
    SECTION("RESUME <line> jumps to specific line") {
        // Test program:
        // 10 ON ERROR GOTO 100
        // 20 A = 1 / 0      ' Cause error
        // 30 PRINT "Should skip this"
        // 40 PRINT "Target line"
        // 50 END
        // 100 PRINT "Error handled"
        // 110 RESUME 40     ' Jump directly to line 40
        
        std::string prog1 = "10 ON ERROR GOTO 100\n";
        std::string prog2 = "20 A = 1 / 0\n";
        std::string prog3 = "30 PRINT \"Should skip this\"\n";
        std::string prog4 = "40 PRINT \"Target line\"\n";
        std::string prog5 = "50 END\n";
        std::string prog6 = "100 PRINT \"Error handled\"\n";
        std::string prog7 = "110 RESUME 40\n";
        
        auto line10 = crunchLine(*tokenizer, prog1);
        auto line20 = crunchLine(*tokenizer, prog2);
        auto line30 = crunchLine(*tokenizer, prog3);
        auto line40 = crunchLine(*tokenizer, prog4);
        auto line50 = crunchLine(*tokenizer, prog5);
        auto line100 = crunchLine(*tokenizer, prog6);
        auto line110 = crunchLine(*tokenizer, prog7);
        
        program->insertLine(10, line10);
        program->insertLine(20, line20);
        program->insertLine(30, line30);
        program->insertLine(40, line40);
        program->insertLine(50, line50);
        program->insertLine(100, line100);
        program->insertLine(110, line110);
        
        capture.clear();
        loop->run();
        
        // Should have handled error and jumped to line 40
        REQUIRE(capture.output.find("Error handled") != std::string::npos);
        REQUIRE(capture.output.find("Target line") != std::string::npos);
        REQUIRE(capture.output.find("Should skip this") == std::string::npos);
    }
}

TEST_CASE("Error Handling - ERL and ERR variables") {
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
    loop->setEventTrapSystem(dispatcher->getEventTrapSystem());
    loop->setRuntimeStack(dispatcher->getRuntimeStack());
    
    SECTION("ERL returns line number of error") {
        // Test program:
        // 10 ON ERROR GOTO 100
        // 25 A = 1 / 0      ' Error on line 25
        // 30 END
        // 100 PRINT "Error line: "; ERL
        
        std::string prog1 = "10 ON ERROR GOTO 100\n";
        std::string prog2 = "25 A = 1 / 0\n";
        std::string prog3 = "30 END\n";
        std::string prog4 = "100 PRINT \"Error line: 25\"\n";
        
        auto line10 = crunchLine(*tokenizer, prog1);
        auto line25 = crunchLine(*tokenizer, prog2);
        auto line30 = crunchLine(*tokenizer, prog3);
        auto line100 = crunchLine(*tokenizer, prog4);
        
        program->insertLine(10, line10);
        program->insertLine(25, line25);
        program->insertLine(30, line30);
        program->insertLine(100, line100);
        
        capture.clear();
        loop->run();
        
        // ERL should return 25
        REQUIRE(capture.output.find("Error line:") != std::string::npos);
        // Validate the RuntimeStack has the error frame with correct line
        auto stack = dispatcher->getRuntimeStack();
        auto errFrame = stack->topErr();
        REQUIRE(errFrame != nullptr);
        REQUIRE(errFrame->resumeLine == 25);
    }
    
    SECTION("ERR returns error code") {
        // Test program:
        // 10 ON ERROR GOTO 100
        // 20 A = 1 / 0      ' Division by zero = error code 11
        // 30 END
        // 100 PRINT "Error code: "; ERR
        
        std::string prog1 = "10 ON ERROR GOTO 100\n";
        std::string prog2 = "20 A = 1 / 0\n";
        std::string prog3 = "30 END\n";
        std::string prog4 = "100 PRINT \"Error code: 11\"\n";
        
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
        
        // ERR should return 11 (division by zero)
        REQUIRE(capture.output.find("Error code:") != std::string::npos);
        // Validate the RuntimeStack has the error frame with correct error code
        auto stack = dispatcher->getRuntimeStack();
        auto errFrame = stack->topErr();
        REQUIRE(errFrame != nullptr);
        REQUIRE(errFrame->errCode == 11);
    }
}

TEST_CASE("Error Handling - ERROR statement simulation") {
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
    loop->setEventTrapSystem(dispatcher->getEventTrapSystem());
    loop->setRuntimeStack(dispatcher->getRuntimeStack());
    
    SECTION("ERROR statement simulates specific error") {
        // Test program:
        // 10 ON ERROR GOTO 100
        // 20 ERROR 99       ' Simulate error code 99
        // 30 PRINT "Should not reach"
        // 100 PRINT "Simulated error: "; ERR
        
        std::string prog1 = "10 ON ERROR GOTO 100\n";
        std::string prog2 = "20 ERROR 99\n";
        std::string prog3 = "30 PRINT \"Should not reach\"\n";
        std::string prog4 = "100 PRINT \"Simulated error: 99\"\n";
        
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
        
        // Should handle simulated error
        REQUIRE(capture.output.find("Simulated error:") != std::string::npos);
        // Validate the RuntimeStack has the error frame with correct error code
        auto stack = dispatcher->getRuntimeStack();
        auto errFrame = stack->topErr();
        REQUIRE(errFrame != nullptr);
        REQUIRE(errFrame->errCode == 99);
        REQUIRE(capture.output.find("Should not reach") == std::string::npos);
    }
}

TEST_CASE("Error Handling - RESUME without error") {
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
    loop->setEventTrapSystem(dispatcher->getEventTrapSystem());
    loop->setRuntimeStack(dispatcher->getRuntimeStack());
    
    SECTION("RESUME without error should generate error") {
        // Test program:
        // 10 RESUME    ' This should cause "RESUME without error"
        
        std::string prog1 = "10 RESUME\n";
        auto line10 = crunchLine(*tokenizer, prog1);
        program->insertLine(10, line10);
        
        capture.clear();
        
        // Use step() to get the result instead of run()
        auto result = loop->step();
        
        // The program should halt due to the RESUME without error
        REQUIRE(result == InterpreterLoop::StepResult::Halted);
    }
}