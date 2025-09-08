#include <catch2/catch_test_macros.hpp>
#include "../src/Runtime/ArrayManager.hpp"
#include "../src/Runtime/StringHeap.hpp"

using namespace gwbasic;

TEST_CASE("ArrayManager basic functionality", "[array]") {
    uint8_t heapBuf[1024];
    StringHeap stringHeap(heapBuf, sizeof(heapBuf));
    ArrayManager arrayManager(&stringHeap);
    
    SECTION("Create and access integer array") {
        // Create array A(10)
        std::vector<int16_t> dimensions = {10};
        REQUIRE(arrayManager.createArray("A", ScalarType::Int16, dimensions));
        
        // Verify array exists
        REQUIRE(arrayManager.arrayExists("A"));
        
        // Set element A(5) = 42
        std::vector<int32_t> indices = {5};
        Value value = Value::makeInt(42);
        REQUIRE(arrayManager.setElement("A", indices, value));
        
        // Get element A(5)
        Value result;
        REQUIRE(arrayManager.getElement("A", indices, result));
        REQUIRE(result.type == ScalarType::Int16);
        REQUIRE(result.i == 42);
    }
    
    SECTION("Create multi-dimensional array") {
        // Create array B(5,3)
        std::vector<int16_t> dimensions = {5, 3};
        REQUIRE(arrayManager.createArray("B", ScalarType::Single, dimensions));
        
        // Set element B(2,1) = 3.14
        std::vector<int32_t> indices = {2, 1};
        Value value = Value::makeSingle(3.14f);
        REQUIRE(arrayManager.setElement("B", indices, value));
        
        // Get element B(2,1)
        Value result;
        REQUIRE(arrayManager.getElement("B", indices, result));
        REQUIRE(result.type == ScalarType::Single);
        REQUIRE(result.f == 3.14f);
    }
    
    SECTION("String array functionality") {
        // Create string array C$(5)
        std::vector<int16_t> dimensions = {5};
        REQUIRE(arrayManager.createArray("C$", ScalarType::String, dimensions));
        
        // Set element C$(2) = "Hello"
        std::vector<int32_t> indices = {2};
        StrDesc desc;
        stringHeap.allocCopy("Hello", desc);
        Value value = Value::makeString(desc);
        REQUIRE(arrayManager.setElement("C$", indices, value));
        
        // Get element C$(2)
        Value result;
        REQUIRE(arrayManager.getElement("C$", indices, result));
        REQUIRE(result.type == ScalarType::String);
        REQUIRE(result.s.len == 5);
    }
    
    SECTION("Bounds checking") {
        // Create array D(3)
        std::vector<int16_t> dimensions = {3};
        REQUIRE(arrayManager.createArray("D", ScalarType::Int16, dimensions));
        
        // Try to access out-of-bounds element D(5)
        std::vector<int32_t> indices = {5};
        Value value = Value::makeInt(10);
        REQUIRE_FALSE(arrayManager.setElement("D", indices, value));
        
        Value result;
        REQUIRE_FALSE(arrayManager.getElement("D", indices, result));
    }
    
    SECTION("Clear arrays") {
        // Create an array
        std::vector<int16_t> dimensions = {5};
        REQUIRE(arrayManager.createArray("E", ScalarType::Int16, dimensions));
        REQUIRE(arrayManager.arrayExists("E"));
        
        // Clear all arrays
        arrayManager.clear();
        REQUIRE_FALSE(arrayManager.arrayExists("E"));
        REQUIRE(arrayManager.size() == 0);
    }
}
