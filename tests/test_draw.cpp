#include <catch2/catch_all.hpp>
#include <memory>
#include <string>
#include <sstream>
#include <vector>
#include "../src/InterpreterLoop/BasicDispatcher.hpp"
#include "../src/Tokenizer/Tokenizer.hpp"

// Helper function to crunch a statement for testing
static std::vector<uint8_t> crunchStmt(Tokenizer& t, const std::string& source) {
    auto bytes = t.crunch(source);
    // Ensure single statement bytes end at 0x00
    if (bytes.empty() || bytes.back() != 0x00) bytes.push_back(0x00);
    return bytes;
}

TEST_CASE("BasicDispatcher DRAW basic movement", "[dispatcher]") {
    Tokenizer tok;
    std::string captured;
    auto printer = [&](const std::string& s){ captured += s; };
    
    // Mock graphics buffer
    static uint8_t graphicsBuffer[640 * 200];
    auto graphicsBufCb = []() -> uint8_t* { return graphicsBuffer; };
    
    auto disp = BasicDispatcher(std::make_shared<Tokenizer>(tok), nullptr, printer, nullptr, nullptr, nullptr, graphicsBufCb);
    disp.setTestMode(true);
    
    // Test basic DRAW commands
    auto stmt = crunchStmt(tok, "DRAW \"U10\"");
    auto r = disp(stmt);
    REQUIRE(r == 0);  // Should complete successfully
}

TEST_CASE("BasicDispatcher DRAW with string variable", "[dispatcher]") {
    Tokenizer tok;
    std::string captured;
    auto printer = [&](const std::string& s){ captured += s; };
    
    // Mock graphics buffer
    static uint8_t graphicsBuffer[640 * 200];
    auto graphicsBufCb = []() -> uint8_t* { return graphicsBuffer; };
    
    auto disp = BasicDispatcher(std::make_shared<Tokenizer>(tok), nullptr, printer, nullptr, nullptr, nullptr, graphicsBufCb);
    disp.setTestMode(true);
    
    // Set up variable and draw
    auto stmt1 = crunchStmt(tok, "A$ = \"D20R10U20L10\"");
    auto r1 = disp(stmt1);
    REQUIRE(r1 == 0);
    
    auto stmt2 = crunchStmt(tok, "DRAW A$");
    auto r2 = disp(stmt2);
    REQUIRE(r2 == 0);  // Should complete successfully
}

TEST_CASE("BasicDispatcher DRAW with scaling", "[dispatcher]") {
    Tokenizer tok;
    std::string captured;
    auto printer = [&](const std::string& s){ captured += s; };
    
    // Mock graphics buffer
    static uint8_t graphicsBuffer[640 * 200];
    auto graphicsBufCb = []() -> uint8_t* { return graphicsBuffer; };
    
    auto disp = BasicDispatcher(std::make_shared<Tokenizer>(tok), nullptr, printer, nullptr, nullptr, nullptr, graphicsBufCb);
    disp.setTestMode(true);
    
    // Test scaling command
    auto stmt = crunchStmt(tok, "DRAW \"S2 U10 D10\"");
    auto r = disp(stmt);
    REQUIRE(r == 0);  // Should complete successfully
}

TEST_CASE("BasicDispatcher DRAW with angle rotation", "[dispatcher]") {
    Tokenizer tok;
    std::string captured;
    auto printer = [&](const std::string& s){ captured += s; };
    
    // Mock graphics buffer
    static uint8_t graphicsBuffer[640 * 200];
    auto graphicsBufCb = []() -> uint8_t* { return graphicsBuffer; };
    
    auto disp = BasicDispatcher(std::make_shared<Tokenizer>(tok), nullptr, printer, nullptr, nullptr, nullptr, graphicsBufCb);
    disp.setTestMode(true);
    
    // Test angle command
    auto stmt = crunchStmt(tok, "DRAW \"A1 U10\"");
    auto r = disp(stmt);
    REQUIRE(r == 0);  // Should complete successfully
}

TEST_CASE("BasicDispatcher DRAW with absolute move", "[dispatcher]") {
    Tokenizer tok;
    std::string captured;
    auto printer = [&](const std::string& s){ captured += s; };
    
    // Mock graphics buffer
    static uint8_t graphicsBuffer[640 * 200];
    auto graphicsBufCb = []() -> uint8_t* { return graphicsBuffer; };
    
    auto disp = BasicDispatcher(std::make_shared<Tokenizer>(tok), nullptr, printer, nullptr, nullptr, nullptr, graphicsBufCb);
    disp.setTestMode(true);
    
    // Test absolute move command
    auto stmt = crunchStmt(tok, "DRAW \"M100,50\"");
    auto r = disp(stmt);
    REQUIRE(r == 0);  // Should complete successfully
}

TEST_CASE("BasicDispatcher DRAW with color", "[dispatcher]") {
    Tokenizer tok;
    std::string captured;
    auto printer = [&](const std::string& s){ captured += s; };
    
    // Mock graphics buffer
    static uint8_t graphicsBuffer[640 * 200];
    auto graphicsBufCb = []() -> uint8_t* { return graphicsBuffer; };
    
    auto disp = BasicDispatcher(std::make_shared<Tokenizer>(tok), nullptr, printer, nullptr, nullptr, nullptr, graphicsBufCb);
    disp.setTestMode(true);
    
    // Test color command
    auto stmt = crunchStmt(tok, "DRAW \"C2 U10\"");
    auto r = disp(stmt);
    REQUIRE(r == 0);  // Should complete successfully
}

TEST_CASE("BasicDispatcher DRAW with blank move", "[dispatcher]") {
    Tokenizer tok;
    std::string captured;
    auto printer = [&](const std::string& s){ captured += s; };
    
    // Mock graphics buffer
    static uint8_t graphicsBuffer[640 * 200];
    auto graphicsBufCb = []() -> uint8_t* { return graphicsBuffer; };
    
    auto disp = BasicDispatcher(std::make_shared<Tokenizer>(tok), nullptr, printer, nullptr, nullptr, nullptr, graphicsBufCb);
    disp.setTestMode(true);
    
    // Test blank (no draw) move command
    auto stmt = crunchStmt(tok, "DRAW \"BU10 D10\"");
    auto r = disp(stmt);
    REQUIRE(r == 0);  // Should complete successfully
}

TEST_CASE("BasicDispatcher DRAW complex pattern", "[dispatcher]") {
    Tokenizer tok;
    std::string captured;
    auto printer = [&](const std::string& s){ captured += s; };
    
    // Mock graphics buffer
    static uint8_t graphicsBuffer[640 * 200];
    auto graphicsBufCb = []() -> uint8_t* { return graphicsBuffer; };
    
    auto disp = BasicDispatcher(std::make_shared<Tokenizer>(tok), nullptr, printer, nullptr, nullptr, nullptr, graphicsBufCb);
    disp.setTestMode(true);
    
    // Test complex drawing pattern
    auto stmt = crunchStmt(tok, "DRAW \"S4 C1 U5 E3 R5 F3 D5 G3 L5 H3\"");
    auto r = disp(stmt);
    REQUIRE(r == 0);  // Should complete successfully
}

TEST_CASE("BasicDispatcher DRAW error handling", "[dispatcher]") {
    Tokenizer tok;
    std::string captured;
    auto printer = [&](const std::string& s){ captured += s; };
    
    // Mock graphics buffer
    static uint8_t graphicsBuffer[640 * 200];
    auto graphicsBufCb = []() -> uint8_t* { return graphicsBuffer; };
    
    auto disp = BasicDispatcher(std::make_shared<Tokenizer>(tok), nullptr, printer, nullptr, nullptr, nullptr, graphicsBufCb);
    disp.setTestMode(true);
    
    // Test invalid command - should be ignored, not throw
    auto stmt = crunchStmt(tok, "DRAW \"Z10\"");
    auto r = disp(stmt);
    REQUIRE(r == 0);  // Should complete successfully, ignoring unknown command
}