#include "ProgramStore.hpp"
#include <iostream>
#include <iomanip>

/**
 * Example program demonstrating ProgramStore usage
 * 
 * This example shows how to:
 * - Create and manage a BASIC program in memory
 * - Insert, modify, and delete program lines
 * - Serialize and deserialize programs
 * - Use iterators to traverse program lines
 */

// Helper function to print a hex dump of tokens
void printTokens(const std::vector<uint8_t>& tokens) {
    std::cout << "Tokens: ";
    for (size_t i = 0; i < tokens.size(); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                  << static_cast<unsigned>(tokens[i]);
        if (i < tokens.size() - 1) std::cout << " ";
    }
    std::cout << std::dec << std::endl;
}

// Helper function to create tokenized PRINT statement
std::vector<uint8_t> createPrintStatement(const std::string& text) {
    std::vector<uint8_t> tokens;
    tokens.push_back(0x90); // PRINT token
    tokens.push_back(0x22); // Quote
    for (char c : text) {
        tokens.push_back(static_cast<uint8_t>(c));
    }
    tokens.push_back(0x22); // Quote
    tokens.push_back(0x00); // Line terminator
    return tokens;
}

// Helper function to create simple statements
std::vector<uint8_t> createStatement(uint8_t token) {
    return {token, 0x00};
}

int main() {
    std::cout << "GW-BASIC ProgramStore Example\n";
    std::cout << "=============================\n\n";
    
    // Create a new program store
    ProgramStore store;
    
    std::cout << "1. Creating a simple BASIC program...\n";
    
    // Create a simple BASIC program:
    // 10 PRINT "Hello, World!"
    // 20 PRINT "This is GW-BASIC"
    // 30 FOR I=1 TO 5
    // 40 PRINT "Loop iteration"; I
    // 50 NEXT I
    // 60 END
    
    store.insertLine(10, createPrintStatement("Hello, World!"));
    store.insertLine(20, createPrintStatement("This is GW-BASIC"));
    
    // FOR I=1 TO 5 (simplified tokenization)
    std::vector<uint8_t> forLoop = {0x81, 'I', '=', '1', 0xEA, '5', 0x00}; // FOR I=1 TO 5
    store.insertLine(30, forLoop);
    
    store.insertLine(40, createPrintStatement("Loop iteration"));
    
    // NEXT I
    std::vector<uint8_t> nextStmt = {0x82, 'I', 0x00}; // NEXT I
    store.insertLine(50, nextStmt);
    
    store.insertLine(60, createStatement(0x80)); // END
    
    std::cout << "Program created with " << store.getLineCount() << " lines\n";
    std::cout << "Total size: " << store.getTotalSize() << " bytes\n";
    std::cout << "Line range: " << store.getFirstLineNumber() 
              << " - " << store.getLastLineNumber() << "\n\n";
    
    std::cout << "2. Listing program lines...\n";
    for (auto it = store.begin(); it != store.end(); ++it) {
        std::cout << "Line " << it->lineNumber << ": ";
        printTokens(it->tokens);
    }
    std::cout << std::endl;
    
    std::cout << "3. Modifying the program...\n";
    
    // Insert a new line between 20 and 30
    store.insertLine(25, createPrintStatement("Inserted line!"));
    
    // Replace line 40
    store.insertLine(40, createPrintStatement("Modified line"));
    
    // Delete line 50 and add it back with different number
    store.deleteLine(50);
    store.insertLine(55, nextStmt);
    
    std::cout << "After modifications:\n";
    for (auto it = store.begin(); it != store.end(); ++it) {
        std::cout << "Line " << it->lineNumber << ": ";
        printTokens(it->tokens);
    }
    std::cout << std::endl;
    
    std::cout << "4. Testing line lookup and iteration...\n";
    
    // Test line existence
    std::cout << "Line 25 exists: " << (store.hasLine(25) ? "Yes" : "No") << std::endl;
    std::cout << "Line 50 exists: " << (store.hasLine(50) ? "Yes" : "No") << std::endl;
    
    // Find line >= 35
    auto it = store.findLine(35);
    if (it.isValid()) {
        std::cout << "First line >= 35 is line " << it->lineNumber << std::endl;
    }
    
    // Get line numbers in range 20-50
    auto rangeLines = store.getLinesInRange(20, 50);
    std::cout << "Lines in range 20-50: ";
    for (const auto& line : rangeLines) {
        std::cout << line->lineNumber << " ";
    }
    std::cout << std::endl << std::endl;
    
    std::cout << "5. Testing serialization...\n";
    
    // Serialize the program
    auto serialized = store.serialize();
    std::cout << "Serialized program size: " << serialized.size() << " bytes\n";
    
    // Create a new store and load the serialized data
    ProgramStore store2;
    if (store2.deserialize(serialized)) {
        std::cout << "Successfully deserialized program\n";
        std::cout << "Deserialized program has " << store2.getLineCount() << " lines\n";
        
        // Verify the programs are identical
        bool identical = true;
        auto it1 = store.begin();
        auto it2 = store2.begin();
        
        while (it1 != store.end() && it2 != store2.end()) {
            if (it1->lineNumber != it2->lineNumber || it1->tokens != it2->tokens) {
                identical = false;
                break;
            }
            ++it1;
            ++it2;
        }
        
        if (it1 != store.end() || it2 != store2.end()) {
            identical = false;
        }
        
        std::cout << "Programs are " << (identical ? "identical" : "different") << std::endl;
    } else {
        std::cout << "Failed to deserialize program\n";
    }
    std::cout << std::endl;
    
    std::cout << "6. Testing AUTO and RENUM functionality...\n";
    
    // Find next available line number for AUTO
    uint16_t nextAuto = store.findNextAutoLineNumber(70, 10);
    std::cout << "Next AUTO line number (start=70, increment=10): " << nextAuto << std::endl;
    
    // Renumber lines in range 10-30 to start at 100
    std::cout << "Renumbering lines 10-30 to start at 100...\n";
    if (store.renumberLines(100, 10, 10, 30)) {
        std::cout << "Renumber successful. New line numbers: ";
        auto lineNumbers = store.getLineNumbers();
        for (uint16_t lineNum : lineNumbers) {
            std::cout << lineNum << " ";
        }
        std::cout << std::endl;
    } else {
        std::cout << "Renumber failed\n";
    }
    std::cout << std::endl;
    
    std::cout << "7. Program validation and debug info...\n";
    
    bool isValid = store.validate();
    std::cout << "Program validation: " << (isValid ? "PASS" : "FAIL") << std::endl;
    
    std::cout << "\nDebug information:\n";
    std::cout << store.getDebugInfo() << std::endl;
    
    std::cout << "8. Clearing the program...\n";
    store.clear();
    std::cout << "Program cleared. Empty: " << (store.isEmpty() ? "Yes" : "No") << std::endl;
    
    std::cout << "\nExample completed successfully!\n";
    
    return 0;
}
