#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include "Value.hpp"

// Forward declarations for classes outside gwbasic namespace
class ProgramStore;
class Tokenizer;

namespace gwbasic {

/**
 * DataManager handles DATA/READ/RESTORE functionality.
 * 
 * In GW-BASIC:
 * - DATA statements contain comma-separated values that can be read by READ statements
 * - READ statements consume values sequentially from DATA statements
 * - RESTORE resets the data pointer to the beginning or to a specific line
 * - Data is read in the order DATA statements appear in the program
 */
class DataManager {
public:
    explicit DataManager(std::shared_ptr<::ProgramStore> programStore = nullptr, 
                        std::shared_ptr<::Tokenizer> tokenizer = nullptr);
    
    // Set program store and tokenizer (can be changed at runtime)
    void setProgramStore(std::shared_ptr<::ProgramStore> programStore);
    void setTokenizer(std::shared_ptr<::Tokenizer> tokenizer);
    
    // Reset data pointer to beginning of program
    void restore();
    
    // Reset data pointer to specific line number
    void restore(uint16_t lineNumber);
    
    // Read next value from DATA statements
    // Returns false if no more data is available
    bool readValue(Value& result);
    
    // Check if more data is available
    bool hasMoreData() const;
    
    // Get current line number where data pointer is located (for error reporting)
    uint16_t getCurrentDataLine() const;
    
private:
    std::shared_ptr<::ProgramStore> prog;
    std::shared_ptr<::Tokenizer> tok;
    
    // Current position in the program for data reading
    struct DataPosition {
        uint16_t lineNumber = 0;    // Current line number
        size_t tokenIndex = 0;      // Index within the line's tokens
        bool valid = false;         // Whether position is valid
    } dataPos;
    
    // Find the next DATA statement starting from current position
    bool findNextDataStatement();
    
    // Parse and return the next value from current DATA statement
    bool parseNextValue(Value& result);
    
    // Skip to the next comma or end of DATA statement
    void skipToNextValue();
    
    // Helper to parse a token value into a Value
    bool parseTokenValue(const std::vector<uint8_t>& tokens, size_t& pos, Value& result);
    
    // Helper to check if we're at the end of a statement
    bool isEndOfStatement(const std::vector<uint8_t>& tokens, size_t pos) const;
    
    // Helper to skip whitespace tokens
    void skipWhitespace(const std::vector<uint8_t>& tokens, size_t& pos) const;
};

} // namespace gwbasic
