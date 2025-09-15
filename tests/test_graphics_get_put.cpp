#include <catch2/catch_all.hpp>
#include <memory>
#include <string>
#include <sstream>
#include "../src/InterpreterLoop/BasicDispatcher.hpp"
#include "../src/Tokenizer/Tokenizer.hpp"
#include "../src/Graphics/GraphicsContext.hpp"

// Mock graphics buffer for testing
static uint8_t mockBuffer[320 * 200]; // Standard VGA buffer size
static bool mockGraphicsInitialized = false;

static uint8_t* mockGraphicsBufferCallback() {
    if (!mockGraphicsInitialized) {
        // Initialize with test pattern
        for (int i = 0; i < 320 * 200; ++i) {
            mockBuffer[i] = static_cast<uint8_t>(i % 16); // Color pattern 0-15
        }
        mockGraphicsInitialized = true;
    }
    return mockBuffer;
}

static std::vector<uint8_t> crunchStmt(Tokenizer& t, const std::string& source) {
    auto bytes = t.crunch(source);
    // Ensure single statement bytes end at 0x00
    if (bytes.empty() || bytes.back() != 0x00) bytes.push_back(0x00);
    return bytes;
}

class GraphicsTestFixture {
public:
    Tokenizer tok;
    std::string captured;
    std::unique_ptr<BasicDispatcher> disp;
    
    GraphicsTestFixture() {
        auto printer = [this](const std::string& s){ captured += s; };
        disp = std::make_unique<BasicDispatcher>(
            std::make_shared<Tokenizer>(tok), 
            nullptr,  // program store
            printer,  // print callback
            nullptr,  // input callback
            nullptr,  // screen mode callback
            nullptr,  // color callback
            mockGraphicsBufferCallback,  // graphics buffer callback
            nullptr,  // width callback
            nullptr,  // locate callback
            nullptr,  // cls callback
            nullptr   // inkey callback
        );
        disp->setTestMode(true);
        
        // Initialize graphics mode
        auto modeStmt = crunchStmt(tok, "SCREEN 1");
        disp->operator()(modeStmt);
        captured.clear(); // Clear SCREEN output
    }
};

TEST_CASE_METHOD(GraphicsTestFixture, "Graphics GET basic functionality", "[graphics][get]") {
    // DIM an array to hold GET data
    auto dimStmt = crunchStmt(tok, "DIM PATTERN%(100)");
    REQUIRE(disp->operator()(dimStmt) == 0);
    
    // Draw some pixels first
    auto psetStmt = crunchStmt(tok, "PSET (10,10),5");
    REQUIRE(disp->operator()(psetStmt) == 0);
    
    auto psetStmt2 = crunchStmt(tok, "PSET (11,10),7");
    REQUIRE(disp->operator()(psetStmt2) == 0);
    
    // GET a 2x2 block
    auto getStmt = crunchStmt(tok, "GET (10,10)-(11,11),PATTERN%");
    REQUIRE(disp->operator()(getStmt) == 0);
    
    // Verify array contains header + pixels
    // Should be: [2, 0, 2, 0, pixel1, pixel2, pixel3, pixel4]
    gwbasic::Value val;
    // Check width (should be 2)
    std::vector<int32_t> idx = {0};
    REQUIRE(disp->vars.getArrayElement("PATTERN%", idx, val));
    REQUIRE(val.type == gwbasic::ScalarType::Int16);
    REQUIRE(val.i == 2);
    // Check height (should be 2)
    idx = {2};
    REQUIRE(disp->vars.getArrayElement("PATTERN%", idx, val));
    REQUIRE(val.type == gwbasic::ScalarType::Int16);
    REQUIRE(val.i == 2);
}

TEST_CASE_METHOD(GraphicsTestFixture, "Graphics GET with STEP mode", "[graphics][get][step]") {
    // DIM an array
    auto dimStmt = crunchStmt(tok, "DIM BLOCK%(20)");
    REQUIRE(disp->operator()(dimStmt) == 0);
    
    // Set current graphics position
    auto psetStmt = crunchStmt(tok, "PSET (50,50),1");
    REQUIRE(disp->operator()(psetStmt) == 0);
    
    // GET with STEP (relative to current position)
    auto getStmt = crunchStmt(tok, "GET STEP (0,0)-(2,2),BLOCK%");
    REQUIRE(disp->operator()(getStmt) == 0);
    
    // Verify it captured from (50,50) area
    gwbasic::Value val;
    // Check dimensions
    std::vector<int32_t> idx = {0};
    REQUIRE(disp->vars.getArrayElement("BLOCK%", idx, val));
    REQUIRE(val.i == 3); // width
    idx = {2};
    REQUIRE(disp->vars.getArrayElement("BLOCK%", idx, val));
    REQUIRE(val.i == 3); // height
}

TEST_CASE_METHOD(GraphicsTestFixture, "Graphics PUT basic functionality", "[graphics][put]") {
    // Create and populate test array with pattern
    auto dimStmt = crunchStmt(tok, "DIM TESTPAT%(10)");
    REQUIRE(disp->operator()(dimStmt) == 0);
    
    // Set up a 2x2 pattern: width=2, height=2, then 4 pixels
    auto setElement = [&](int index, int16_t value) {
        std::vector<int32_t> idx = {index};
        gwbasic::Value val = gwbasic::Value::makeInt(value);
        REQUIRE(disp->vars.setArrayElement("TESTPAT%", idx, val));
    };
    
    setElement(0, 2);  // width
    setElement(1, 0);  // width high byte
    setElement(2, 2);  // height  
    setElement(3, 0);  // height high byte
    setElement(4, 15); // pixel (0,0) - white
    setElement(5, 14); // pixel (1,0) - yellow
    setElement(6, 13); // pixel (0,1) - magenta
    setElement(7, 12); // pixel (1,1) - light red
    
    // PUT the pattern at (20,20)
    auto putStmt = crunchStmt(tok, "PUT (20,20),TESTPAT%");
    REQUIRE(disp->operator()(putStmt) == 0);
    
    // Verify pixels were set (can't easily read back, but operation should succeed)
    REQUIRE(captured.empty()); // No error messages
}

TEST_CASE_METHOD(GraphicsTestFixture, "Graphics PUT with different verbs", "[graphics][put][verbs]") {
    // Create test pattern
    auto dimStmt = crunchStmt(tok, "DIM PAT%(10)");
    REQUIRE(disp->operator()(dimStmt) == 0);
    
    auto setElement = [&](int index, int16_t value) {
        std::vector<int32_t> idx = {index};
        gwbasic::Value val = gwbasic::Value::makeInt(value);
        REQUIRE(disp->vars.setArrayElement("PAT%", idx, val));
    };
    
    // 1x1 pattern with color 7
    setElement(0, 1); setElement(1, 0); // width=1
    setElement(2, 1); setElement(3, 0); // height=1  
    setElement(4, 7); // color 7
    
    SECTION("PUT with PSET") {
        auto putStmt = crunchStmt(tok, "PUT (30,30),PAT%,PSET");
        REQUIRE(disp->operator()(putStmt) == 0);
    }
    
    SECTION("PUT with PRESET") {
        auto putStmt = crunchStmt(tok, "PUT (30,30),PAT%,PRESET");
        REQUIRE(disp->operator()(putStmt) == 0);
    }
    
    SECTION("PUT with XOR") {
        auto putStmt = crunchStmt(tok, "PUT (30,30),PAT%,XOR");
        REQUIRE(disp->operator()(putStmt) == 0);
    }
    
    SECTION("PUT with AND") {
        auto putStmt = crunchStmt(tok, "PUT (30,30),PAT%,AND");
        REQUIRE(disp->operator()(putStmt) == 0);
    }
    
    SECTION("PUT with OR") {
        auto putStmt = crunchStmt(tok, "PUT (30,30),PAT%,OR");
        REQUIRE(disp->operator()(putStmt) == 0);
    }
}

TEST_CASE_METHOD(GraphicsTestFixture, "Graphics PUT with STEP mode", "[graphics][put][step]") {
    // Create pattern
    auto dimStmt = crunchStmt(tok, "DIM STEPAT%(8)");
    REQUIRE(disp->operator()(dimStmt) == 0);
    
    auto setElement = [&](int index, int16_t value) {
        std::vector<int32_t> idx = {index};
        gwbasic::Value val = gwbasic::Value::makeInt(value);
        REQUIRE(disp->vars.setArrayElement("STEPAT%", idx, val));
    };
    
    // 1x1 pattern
    setElement(0, 1); setElement(1, 0); // width=1
    setElement(2, 1); setElement(3, 0); // height=1
    setElement(4, 9); // color 9
    
    // Set current position
    auto psetStmt = crunchStmt(tok, "PSET (100,100),1");
    REQUIRE(disp->operator()(psetStmt) == 0);
    
    // PUT with STEP (relative to current position)
    auto putStmt = crunchStmt(tok, "PUT STEP (5,5),STEPAT%");
    REQUIRE(disp->operator()(putStmt) == 0);
    
    // Should have placed pattern at (105,105)
    REQUIRE(captured.empty()); // No errors
}

TEST_CASE_METHOD(GraphicsTestFixture, "Graphics GET/PUT error conditions", "[graphics][errors]") {
    SECTION("GET with array too small") {
        auto dimStmt = crunchStmt(tok, "DIM SMALL%(3)"); // Only 4 elements, need at least 4 for header
        REQUIRE(disp->operator()(dimStmt) == 0);
        
        // Try to GET a large block that won't fit
        auto getStmt = crunchStmt(tok, "GET (0,0)-(10,10),SMALL%");
        REQUIRE_THROWS_AS(disp->operator()(getStmt), expr::BasicError);
    }
    
    SECTION("PUT with malformed array") {
        auto dimStmt = crunchStmt(tok, "DIM BROKEN%(2)"); // Too small for header
        REQUIRE(disp->operator()(dimStmt) == 0);
        
        auto putStmt = crunchStmt(tok, "PUT (0,0),BROKEN%");
        REQUIRE_THROWS_AS(disp->operator()(putStmt), expr::BasicError);
    }
    
    SECTION("GET with undimensioned array") {
        auto getStmt = crunchStmt(tok, "GET (0,0)-(1,1),NOARRAY%");
        REQUIRE_THROWS_AS(disp->operator()(getStmt), expr::BasicError);
    }
    
    SECTION("PUT with undimensioned array") {
        auto putStmt = crunchStmt(tok, "PUT (0,0),NOARRAY%");
        REQUIRE_THROWS_AS(disp->operator()(putStmt), expr::BasicError);
    }
}

TEST_CASE_METHOD(GraphicsTestFixture, "Graphics GET/PUT with different array types", "[graphics][types]") {
    SECTION("Single precision array") {
        auto dimStmt = crunchStmt(tok, "DIM SINGLES!(20)");
        REQUIRE(disp->operator()(dimStmt) == 0);
        
        auto getStmt = crunchStmt(tok, "GET (0,0)-(2,2),SINGLES!");
        REQUIRE(disp->operator()(getStmt) == 0);
        
        auto putStmt = crunchStmt(tok, "PUT (40,40),SINGLES!");
        REQUIRE(disp->operator()(putStmt) == 0);
    }
    
    SECTION("Double precision array") {
        auto dimStmt = crunchStmt(tok, "DIM DOUBLES#(20)");
        REQUIRE(disp->operator()(dimStmt) == 0);
        
        auto getStmt = crunchStmt(tok, "GET (0,0)-(2,2),DOUBLES#");
        REQUIRE(disp->operator()(getStmt) == 0);
        
        auto putStmt = crunchStmt(tok, "PUT (60,60),DOUBLES#");
        REQUIRE(disp->operator()(putStmt) == 0);
    }
    
    SECTION("String array should fail") {
        auto dimStmt = crunchStmt(tok, "DIM STRINGS$(10)");
        REQUIRE(disp->operator()(dimStmt) == 0);
        
        auto getStmt = crunchStmt(tok, "GET (0,0)-(1,1),STRINGS$");
        REQUIRE_THROWS_AS(disp->operator()(getStmt), expr::BasicError);
    }
}

TEST_CASE_METHOD(GraphicsTestFixture, "Graphics coordinate validation", "[graphics][coordinates]") {
    auto dimStmt = crunchStmt(tok, "DIM COORDS%(50)");
    REQUIRE(disp->operator()(dimStmt) == 0);
    
    SECTION("Valid coordinates") {
        auto getStmt = crunchStmt(tok, "GET (0,0)-(10,10),COORDS%");
        REQUIRE(disp->operator()(getStmt) == 0);
    }
    
    SECTION("Swapped coordinates (handled gracefully)") {
        auto getStmt = crunchStmt(tok, "GET (10,10)-(0,0),COORDS%");
        REQUIRE(disp->operator()(getStmt) == 0);
    }
    
    SECTION("Large coordinates (clipped to screen)") {
        auto getStmt = crunchStmt(tok, "GET (0,0)-(1000,1000),COORDS%");
        REQUIRE(disp->operator()(getStmt) == 0);
    }
}

TEST_CASE_METHOD(GraphicsTestFixture, "Graphics GET/PUT round trip", "[graphics][roundtrip]") {
    // Draw a test pattern
    auto pset1 = crunchStmt(tok, "PSET (5,5),1");
    auto pset2 = crunchStmt(tok, "PSET (6,5),2");
    auto pset3 = crunchStmt(tok, "PSET (5,6),3");
    auto pset4 = crunchStmt(tok, "PSET (6,6),4");
    
    REQUIRE(disp->operator()(pset1) == 0);
    REQUIRE(disp->operator()(pset2) == 0);
    REQUIRE(disp->operator()(pset3) == 0);
    REQUIRE(disp->operator()(pset4) == 0);
    
    // GET the pattern
    auto dimStmt = crunchStmt(tok, "DIM ROUNDTRIP%(10)");
    REQUIRE(disp->operator()(dimStmt) == 0);
    
    auto getStmt = crunchStmt(tok, "GET (5,5)-(6,6),ROUNDTRIP%");
    REQUIRE(disp->operator()(getStmt) == 0);
    
    // Clear the original area
    auto preset1 = crunchStmt(tok, "PRESET (5,5)");
    auto preset2 = crunchStmt(tok, "PRESET (6,5)");
    auto preset3 = crunchStmt(tok, "PRESET (5,6)");
    auto preset4 = crunchStmt(tok, "PRESET (6,6)");
    
    REQUIRE(disp->operator()(preset1) == 0);
    REQUIRE(disp->operator()(preset2) == 0);
    REQUIRE(disp->operator()(preset3) == 0);
    REQUIRE(disp->operator()(preset4) == 0);
    
    // PUT it back at a different location
    auto putStmt = crunchStmt(tok, "PUT (15,15),ROUNDTRIP%");
    REQUIRE(disp->operator()(putStmt) == 0);
    
    // Should complete without errors
    REQUIRE(captured.empty());
}