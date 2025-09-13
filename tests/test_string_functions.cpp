#include <catch2/catch_all.hpp>
#include "../src/Runtime/StringFunctions.hpp"
#include <memory>

using namespace gwbasic;

TEST_CASE("StringFunctionProcessor basic operations") {
    auto stringManager = std::make_shared<StringManager>();
    StringFunctionProcessor processor(stringManager);
    
    SECTION("CHR$ function") {
        auto result = processor.chr(65); // ASCII 'A'
        REQUIRE(result.type == ScalarType::String);
        REQUIRE(stringManager->toString(result.s) == "A");
        
        auto result2 = processor.chr(32); // ASCII space
        REQUIRE(stringManager->toString(result2.s) == " ");
        
        // Test error conditions
        REQUIRE_THROWS(processor.chr(-1));
        REQUIRE_THROWS(processor.chr(256));
    }
    
    SECTION("STR$ function") {
        auto intVal = gwbasic::Value::makeInt(42);
        auto result = processor.str(intVal);
        REQUIRE(result.type == ScalarType::String);
        REQUIRE(stringManager->toString(result.s) == " 42"); // Leading space for positive
        
        auto negIntVal = gwbasic::Value::makeInt(-42);
        auto result2 = processor.str(negIntVal);
        REQUIRE(stringManager->toString(result2.s) == "-42"); // No leading space for negative
        
        auto singleVal = gwbasic::Value::makeSingle(3.14f);
        auto result3 = processor.str(singleVal);
        REQUIRE(stringManager->toString(result3.s).substr(0, 2) == " 3"); // Leading space + starts with 3
        
        // Test error conditions
        StrDesc testStr;
        stringManager->createString("test", testStr);
        auto strVal = gwbasic::Value::makeString(testStr);
        REQUIRE_THROWS(processor.str(strVal));
    }
    
    SECTION("LEN function") {
        StrDesc testStr;
        stringManager->createString("Hello", testStr);
        auto strVal = gwbasic::Value::makeString(testStr);
        
        int16_t length = processor.len(strVal);
        REQUIRE(length == 5);
        
        // Empty string
        StrDesc emptyStr;
        stringManager->createString("", emptyStr);
        auto emptyVal = gwbasic::Value::makeString(emptyStr);
        REQUIRE(processor.len(emptyVal) == 0);
        
        // Test error conditions
        auto intVal = gwbasic::Value::makeInt(42);
        REQUIRE_THROWS(processor.len(intVal));
    }
    
    SECTION("ASC function") {
        StrDesc testStr;
        stringManager->createString("ABC", testStr);
        auto strVal = gwbasic::Value::makeString(testStr);
        
        int16_t ascii = processor.asc(strVal);
        REQUIRE(ascii == 65); // ASCII value of 'A'
        
        // Test error conditions
        StrDesc emptyStr;
        stringManager->createString("", emptyStr);
        auto emptyVal = gwbasic::Value::makeString(emptyStr);
        REQUIRE_THROWS(processor.asc(emptyVal));
        
        auto intVal = gwbasic::Value::makeInt(42);
        REQUIRE_THROWS(processor.asc(intVal));
    }
}

TEST_CASE("StringFunctionProcessor string manipulation") {
    auto stringManager = std::make_shared<StringManager>();
    StringFunctionProcessor processor(stringManager);
    
    SECTION("LEFT$ function") {
        StrDesc testStr;
        stringManager->createString("Hello World", testStr);
        auto strVal = gwbasic::Value::makeString(testStr);
        
        auto result = processor.left(strVal, 5);
        REQUIRE(result.type == ScalarType::String);
        REQUIRE(stringManager->toString(result.s) == "Hello");
        
        // Count larger than string length
        auto result2 = processor.left(strVal, 20);
        REQUIRE(stringManager->toString(result2.s) == "Hello World");
        
        // Zero count
        auto result3 = processor.left(strVal, 0);
        REQUIRE(stringManager->toString(result3.s) == "");
        
        // Test error conditions
        REQUIRE_THROWS(processor.left(strVal, -1));
        
        auto intVal = gwbasic::Value::makeInt(42);
        REQUIRE_THROWS(processor.left(intVal, 5));
    }
    
    SECTION("RIGHT$ function") {
        StrDesc testStr;
        stringManager->createString("Hello World", testStr);
        auto strVal = gwbasic::Value::makeString(testStr);
        
        auto result = processor.right(strVal, 5);
        REQUIRE(result.type == ScalarType::String);
        REQUIRE(stringManager->toString(result.s) == "World");
        
        // Count larger than string length
        auto result2 = processor.right(strVal, 20);
        REQUIRE(stringManager->toString(result2.s) == "Hello World");
        
        // Zero count
        auto result3 = processor.right(strVal, 0);
        REQUIRE(stringManager->toString(result3.s) == "");
        
        // Test error conditions
        REQUIRE_THROWS(processor.right(strVal, -1));
    }
    
    SECTION("MID$ function") {
        StrDesc testStr;
        stringManager->createString("Hello World", testStr);
        auto strVal = gwbasic::Value::makeString(testStr);
        
        // MID$ with start position only
        auto result = processor.mid(strVal, 7, -1);
        REQUIRE(stringManager->toString(result.s) == "World");
        
        // MID$ with start and count
        auto result2 = processor.mid(strVal, 7, 3);
        REQUIRE(stringManager->toString(result2.s) == "Wor");
        
        // Start position beyond string length
        auto result3 = processor.mid(strVal, 20, 5);
        REQUIRE(stringManager->toString(result3.s) == "");
        
        // Test error conditions
        REQUIRE_THROWS(processor.mid(strVal, 0, 5)); // Start must be >= 1
    }
    
    SECTION("VAL function") {
        StrDesc testStr;
        stringManager->createString("123", testStr);
        auto strVal = gwbasic::Value::makeString(testStr);
        
        auto result = processor.val(strVal);
        REQUIRE(result.type == ScalarType::Int16);
        REQUIRE(result.i == 123);
        
        // Floating point
        StrDesc floatStr;
        stringManager->createString("3.14", floatStr);
        auto floatVal = gwbasic::Value::makeString(floatStr);
        auto result2 = processor.val(floatVal);
        REQUIRE(result2.type == ScalarType::Double);
        REQUIRE(result2.d == 3.14);
        
        // Invalid string
        StrDesc invalidStr;
        stringManager->createString("abc", invalidStr);
        auto invalidVal = gwbasic::Value::makeString(invalidStr);
        auto result3 = processor.val(invalidVal);
        REQUIRE(result3.type == ScalarType::Int16);
        REQUIRE(result3.i == 0);
        
        // Empty string
        StrDesc emptyStr;
        stringManager->createString("", emptyStr);
        auto emptyVal = gwbasic::Value::makeString(emptyStr);
        auto result4 = processor.val(emptyVal);
        REQUIRE(result4.type == ScalarType::Int16);
        REQUIRE(result4.i == 0);
    }
    
    SECTION("INSTR function") {
        StrDesc sourceStr;
        stringManager->createString("Hello World", sourceStr);
        auto sourceVal = gwbasic::Value::makeString(sourceStr);
        
        StrDesc searchStr;
        stringManager->createString("World", searchStr);
        auto searchVal = gwbasic::Value::makeString(searchStr);
        
        int16_t pos = processor.instr(sourceVal, searchVal, 1);
        REQUIRE(pos == 7); // "World" starts at position 7 (1-based)
        
        // Search for substring not found
        StrDesc notFoundStr;
        stringManager->createString("xyz", notFoundStr);
        auto notFoundVal = gwbasic::Value::makeString(notFoundStr);
        
        int16_t pos2 = processor.instr(sourceVal, notFoundVal, 1);
        REQUIRE(pos2 == 0); // Not found
        
        // Search starting from later position
        StrDesc charStr;
        stringManager->createString("l", charStr);
        auto charVal = gwbasic::Value::makeString(charStr);
        
        int16_t pos3 = processor.instr(sourceVal, charVal, 4);
        REQUIRE(pos3 == 4); // First 'l' after position 4 is at position 4
    }
}

TEST_CASE("StringFunctionProcessor expr integration") {
    auto stringManager = std::make_shared<StringManager>();
    StringFunctionProcessor processor(stringManager);
    
    SECTION("Expression to runtime conversion") {
        expr::Int16 exprInt{42};
        auto runtimeVal = processor.exprToRuntime(exprInt);
        REQUIRE(runtimeVal.type == ScalarType::Int16);
        REQUIRE(runtimeVal.i == 42);
        
        expr::Single exprFloat{3.14f};
        auto runtimeVal2 = processor.exprToRuntime(exprFloat);
        REQUIRE(runtimeVal2.type == ScalarType::Single);
        REQUIRE(runtimeVal2.f == 3.14f);
        
        expr::Str exprStr{"Hello"};
        auto runtimeVal3 = processor.exprToRuntime(exprStr);
        REQUIRE(runtimeVal3.type == ScalarType::String);
        REQUIRE(stringManager->toString(runtimeVal3.s) == "Hello");
    }
    
    SECTION("Runtime to expression conversion") {
        auto runtimeInt = gwbasic::Value::makeInt(42);
        auto exprVal = processor.runtimeToExpr(runtimeInt);
        REQUIRE(std::holds_alternative<expr::Int16>(exprVal));
        REQUIRE(std::get<expr::Int16>(exprVal).v == 42);
        
        StrDesc testStr;
        stringManager->createString("Hello", testStr);
        auto runtimeStr = gwbasic::Value::makeString(testStr);
        auto exprVal2 = processor.runtimeToExpr(runtimeStr);
        REQUIRE(std::holds_alternative<expr::Str>(exprVal2));
        REQUIRE(std::get<expr::Str>(exprVal2).v == "Hello");
    }
    
    SECTION("String function call interface") {
        std::vector<expr::Value> args;
        expr::Value result;
        
        // Test CHR$ function call
        args.push_back(expr::Int16{65});
        REQUIRE(processor.callStringFunction("CHR$", args, result));
        REQUIRE(std::holds_alternative<expr::Str>(result));
        REQUIRE(std::get<expr::Str>(result).v == "A");
        
        // Test LEN function call
        args.clear();
        args.push_back(expr::Str{"Hello"});
        REQUIRE(processor.callStringFunction("LEN", args, result));
        REQUIRE(std::holds_alternative<expr::Int16>(result));
        REQUIRE(std::get<expr::Int16>(result).v == 5);
        
        // Test LEFT$ function call
        args.clear();
        args.push_back(expr::Str{"Hello World"});
        args.push_back(expr::Int16{5});
        REQUIRE(processor.callStringFunction("LEFT$", args, result));
        REQUIRE(std::holds_alternative<expr::Str>(result));
        REQUIRE(std::get<expr::Str>(result).v == "Hello");
        
        // Test unknown function
        args.clear();
        REQUIRE_FALSE(processor.callStringFunction("UNKNOWN", args, result));
        
        // Test case insensitive function names
        args.clear();
        args.push_back(expr::Int16{66});
        REQUIRE(processor.callStringFunction("chr$", args, result)); // lowercase
        REQUIRE(std::get<expr::Str>(result).v == "B");
    }
}

TEST_CASE("StringFunctionProcessor memory management") {
    auto stringManager = std::make_shared<StringManager>();
    StringFunctionProcessor processor(stringManager);
    
    SECTION("String allocation and garbage collection") {
        // Create multiple strings to test memory management
        std::vector<gwbasic::Value> strings;
        
        for (int i = 0; i < 100; ++i) {
            auto str = processor.chr(65 + (i % 26)); // Cycle through A-Z
            strings.push_back(str);
        }
        
        // All strings should be valid
        REQUIRE(strings.size() == 100);
        for (const auto& str : strings) {
            REQUIRE(str.type == ScalarType::String);
            REQUIRE(str.s.len == 1);
        }
        
        // Check memory usage
        REQUIRE(stringManager->getUsedBytes() == 100);
        
        // Test that StringManager's memory management is working
        REQUIRE(stringManager->getFreeBytes() < stringManager->getTotalBytes());
    }
    
    SECTION("Expression conversion doesn't leak memory") {
        size_t initialUsed = stringManager->getUsedBytes();
        
        // Convert many expressions back and forth
        for (int i = 0; i < 50; ++i) {
            expr::Str exprStr{"Test string " + std::to_string(i)};
            auto runtimeVal = processor.exprToRuntime(exprStr);
            auto backToExpr = processor.runtimeToExpr(runtimeVal);
            
            REQUIRE(std::holds_alternative<expr::Str>(backToExpr));
        }
        
        // Memory usage should have increased predictably
        size_t finalUsed = stringManager->getUsedBytes();
        REQUIRE(finalUsed > initialUsed);
        
        // But no excessive fragmentation
        REQUIRE(stringManager->getFragmentation() < 0.5); // Less than 50% fragmentation
    }
}