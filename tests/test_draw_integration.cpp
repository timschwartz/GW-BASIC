#include <catch2/catch_all.hpp>
#include <memory>
#include <string>
#include <sstream>
#include <vector>
#include "../src/InterpreterLoop/BasicDispatcher.hpp"
#include "../src/Tokenizer/Tokenizer.hpp"

TEST_CASE("DRAW integration test", "[integration]") {
    Tokenizer tok;
    std::string captured;
    auto printer = [&](const std::string& s){ captured += s; };
    
    // Mock graphics buffer
    static uint8_t graphicsBuffer[640 * 200];
    auto graphicsBufCb = []() -> uint8_t* { return graphicsBuffer; };
    
    auto disp = BasicDispatcher(std::make_shared<Tokenizer>(tok), nullptr, printer, nullptr, nullptr, nullptr, graphicsBufCb);
    disp.setTestMode(true);
    
    // Test DRAW tokenization and execution
    std::string source = "DRAW \"U10D10L10R10\"";
    auto tokens = tok.crunch(source);
    
    // Verify DRAW is tokenized as extended statement 0x11
    REQUIRE(tokens.size() >= 2);
    REQUIRE(tokens[0] == 0xfe); // Extended statement prefix
    REQUIRE(tokens[1] == 0x11); // DRAW token
    
    // Execute the statement
    if (tokens.empty() || tokens.back() != 0x00) tokens.push_back(0x00);
    auto r = disp(tokens);
    REQUIRE(r == 0); // Should complete successfully
}