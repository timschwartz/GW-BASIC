#include "ProgramStore.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <cstdint>

// Simple test framework
class TestRunner {
public:
    static void run() {
        std::cout << "Running ProgramStore tests...\n\n";
        
        testBasicOperations();
        testSerialization();
        testLineNumberUtilities();
        testIterators();
        testValidation();
        
        std::cout << "\nAll tests passed!\n";
    }

private:
    static void testBasicOperations() {
        std::cout << "Testing basic operations...\n";
        
        ProgramStore store;
        
        // Test empty store
        assert(store.isEmpty());
        assert(store.getLineCount() == 0);
        assert(store.getTotalSize() == 0);
        assert(store.getFirstLineNumber() == 0);
        assert(store.getLastLineNumber() == 0);
        
        // Test inserting lines
        std::vector<uint8_t> tokens1 = {0x90, 0x22, 'H', 'e', 'l', 'l', 'o', 0x22, 0x00}; // PRINT "Hello"
        std::vector<uint8_t> tokens2 = {0x88, 0x14, 0x32, 0x00}; // GOTO 50 (14 32 = little endian 50)
        std::vector<uint8_t> tokens3 = {0x80, 0x00}; // END
        
        assert(store.insertLine(10, tokens1));
        assert(store.insertLine(30, tokens3));
        assert(store.insertLine(20, tokens2));
        
        // Test store state
        assert(!store.isEmpty());
        assert(store.getLineCount() == 3);
        assert(store.getFirstLineNumber() == 10);
        assert(store.getLastLineNumber() == 30);
        
        // Test line retrieval
        assert(store.hasLine(10));
        assert(store.hasLine(20));
        assert(store.hasLine(30));
        assert(!store.hasLine(15));
        
        auto line10 = store.getLine(10);
        assert(line10 != nullptr);
        assert(line10->lineNumber == 10);
        assert(line10->tokens == tokens1);
        
        // Test line deletion
        assert(store.deleteLine(20));
        assert(!store.hasLine(20));
        assert(store.getLineCount() == 2);
        
        // Test clear
        store.clear();
        assert(store.isEmpty());
        assert(store.getLineCount() == 0);
        
        std::cout << "  Basic operations: PASS\n";
    }
    
    static void testSerialization() {
        std::cout << "Testing serialization...\n";
        
        ProgramStore store;
        
        // Add some test lines
        std::vector<uint8_t> tokens1 = {0x90, 0x22, 'T', 'e', 's', 't', 0x22, 0x00}; // PRINT "Test"
        std::vector<uint8_t> tokens2 = {0x81, 'I', '=', '1', 0xEA, '1', '0', 0x00}; // FOR I=1 TO 10
        std::vector<uint8_t> tokens3 = {0x82, 'I', 0x00}; // NEXT I
        
        store.insertLine(100, tokens1);
        store.insertLine(110, tokens2);
        store.insertLine(120, tokens3);
        
        // Serialize
        auto serialized = store.serialize();
        assert(!serialized.empty());
        
        // Create new store and deserialize
        ProgramStore store2;
        assert(store2.deserialize(serialized));
        
        // Verify deserialized store
        assert(store2.getLineCount() == 3);
        assert(store2.hasLine(100));
        assert(store2.hasLine(110));
        assert(store2.hasLine(120));
        
        auto line100 = store2.getLine(100);
        assert(line100 != nullptr);
        assert(line100->tokens == tokens1);
        
        std::cout << "  Serialization: PASS\n";
    }
    
    static void testLineNumberUtilities() {
        std::cout << "Testing line number utilities...\n";
        
        ProgramStore store;
        
        // Add lines with gaps
        std::vector<uint8_t> tokens = {0x80, 0x00}; // END
        store.insertLine(10, tokens);
        store.insertLine(30, tokens);
        store.insertLine(50, tokens);
        
    // Test AUTO line number finding
    assert(store.findNextAutoLineNumber(5, 10) == 5);   // Before first line
    assert(store.findNextAutoLineNumber(15, 10) == 20); // In gap
    assert(store.findNextAutoLineNumber(35, 10) == 40); // In gap
    assert(store.findNextAutoLineNumber(55, 10) == 60); // After last line
        
        // Test renumbering
        assert(store.renumberLines(100, 10, 0, 0)); // Renumber all lines
        
        assert(!store.hasLine(10));
        assert(!store.hasLine(30));
        assert(!store.hasLine(50));
        assert(store.hasLine(100));
        assert(store.hasLine(110));
        assert(store.hasLine(120));
        
        std::cout << "  Line number utilities: PASS\n";
    }
    
    static void testIterators() {
        std::cout << "Testing iterators...\n";
        
        ProgramStore store;
        std::vector<uint8_t> tokens = {0x80, 0x00}; // END
        
        // Add lines in random order
        store.insertLine(30, tokens);
        store.insertLine(10, tokens);
        store.insertLine(20, tokens);
        
        // Test iteration order (should be sorted)
        std::vector<uint16_t> expectedOrder = {10, 20, 30};
        size_t index = 0;
        
        for (auto it = store.begin(); it != store.end(); ++it) {
            assert(index < expectedOrder.size());
            assert(it->lineNumber == expectedOrder[index]);
            index++;
        }
        assert(index == expectedOrder.size());
        
        // Test findLine
        auto it = store.findLine(15);
        assert(it.isValid());
        assert(it->lineNumber == 20); // Next line >= 15
        
        it = store.findLine(25);
        assert(it.isValid());
        assert(it->lineNumber == 30); // Next line >= 25
        
        it = store.findLine(35);
        assert(!it.isValid()); // No line >= 35
        
        std::cout << "  Iterators: PASS\n";
    }
    
    static void testValidation() {
        std::cout << "Testing validation...\n";
        
        ProgramStore store;
        
        // Test invalid line numbers
        std::vector<uint8_t> tokens = {0x80, 0x00};
        assert(!store.insertLine(0, tokens));      // Too low
        assert(!store.insertLine(65530, tokens));  // Too high
        assert(store.insertLine(1, tokens));       // Valid minimum
        assert(store.insertLine(65529, tokens));   // Valid maximum
        
        // Test validation
        assert(store.validate());
        
        // Test line validity
        store.clear();
        std::vector<uint8_t> validTokens = {0x90, 0x22, 'O', 'K', 0x22, 0x00};
        store.insertLine(10, validTokens);
        
        auto line = store.getLine(10);
        assert(line->isValid());
        
        std::cout << "  Validation: PASS\n";
    }
};

int main() {
    try {
        TestRunner::run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}
