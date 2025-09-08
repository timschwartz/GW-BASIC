#include "DataManager.hpp"
#include "../ProgramStore/ProgramStore.hpp"
#include "../Tokenizer/Tokenizer.hpp"
#include <iostream>
#include <string>
#include <sstream>

namespace gwbasic {

DataManager::DataManager(std::shared_ptr<::ProgramStore> programStore, 
                        std::shared_ptr<::Tokenizer> tokenizer)
    : prog(std::move(programStore)), tok(std::move(tokenizer)) {
    restore(); // Initialize to beginning
}

void DataManager::setProgramStore(std::shared_ptr<::ProgramStore> programStore) {
    prog = std::move(programStore);
    restore(); // Reset to beginning when program changes
}

void DataManager::setTokenizer(std::shared_ptr<::Tokenizer> tokenizer) {
    tok = std::move(tokenizer);
}

void DataManager::restore() {
    std::cerr << "DEBUG: restore() called" << std::endl;
    dataPos = DataPosition{}; // Reset to invalid position
    
    if (!prog) {
        std::cerr << "DEBUG: restore() - no prog" << std::endl;
        return;
    }
    
    // Start from the beginning of the program
    auto it = prog->begin();
    if (it != prog->end()) {
        std::cerr << "DEBUG: restore() - found program, first line " << it->lineNumber << std::endl;
        dataPos.lineNumber = it->lineNumber;
        dataPos.tokenIndex = 0;
        dataPos.valid = true;
        
        // Find the first DATA statement
        std::cerr << "DEBUG: restore() - calling findNextDataStatement" << std::endl;
        findNextDataStatement();
    } else {
        std::cerr << "DEBUG: restore() - empty program" << std::endl;
    }
}

void DataManager::restore(uint16_t lineNumber) {
    dataPos = DataPosition{}; // Reset to invalid position
    
    if (!prog) {
        return;
    }
    
    // Find the specified line or the next line after it
    auto it = prog->findLine(lineNumber);
    if (!it.isValid()) {
        // If exact line not found, start from the next line
        it = prog->getNextLine(lineNumber - 1);
    }
    
    if (it.isValid()) {
        dataPos.lineNumber = it->lineNumber;
        dataPos.tokenIndex = 0;
        dataPos.valid = true;
        
        // Find the next DATA statement from this position
        findNextDataStatement();
    }
}

bool DataManager::readValue(Value& result) {
    std::cerr << "DEBUG: readValue() called, valid=" << dataPos.valid << std::endl;
    
    if (!prog || !tok) {
        std::cerr << "DEBUG: readValue() - no prog or tok" << std::endl;
        return false;
    }
    
    // If we don't have a valid position, try to find the first DATA statement
    if (!dataPos.valid) {
        std::cerr << "DEBUG: readValue() - invalid position, calling restore()" << std::endl;
        restore();
        if (!dataPos.valid) {
            std::cerr << "DEBUG: readValue() - still no valid position after restore" << std::endl;
            return false;
        }
    }
    
    std::cerr << "DEBUG: readValue() - trying parseNextValue at line " << dataPos.lineNumber << std::endl;
    
    // Parse the next value from current DATA statement
    if (parseNextValue(result)) {
        std::cerr << "DEBUG: readValue() - parseNextValue succeeded" << std::endl;
        return true;
    }
    
    std::cerr << "DEBUG: readValue() - parseNextValue failed, trying findNextDataStatement" << std::endl;
    
    // No more values in current DATA statement, find next one
    if (findNextDataStatement()) {
        std::cerr << "DEBUG: readValue() - found next DATA statement, trying parseNextValue again" << std::endl;
        return parseNextValue(result);
    }
    
    std::cerr << "DEBUG: readValue() - no more DATA statements" << std::endl;
    // No more DATA statements
    return false;
}

bool DataManager::hasMoreData() const {
    // This is a simplified check - in a full implementation we'd need to
    // look ahead to see if there are more DATA statements or values
    return dataPos.valid;
}

uint16_t DataManager::getCurrentDataLine() const {
    return dataPos.lineNumber;
}

bool DataManager::findNextDataStatement() {
    if (!prog || !tok || !dataPos.valid) {
        std::cerr << "DEBUG: findNextDataStatement() - early return: prog=" << (prog ? "ok" : "null") 
                  << " tok=" << (tok ? "ok" : "null") 
                  << " valid=" << dataPos.valid << std::endl;
        return false;
    }
    
    // Start from current position and look for DATA statements
    auto it = prog->findLine(dataPos.lineNumber);
    if (!it.isValid()) {
        std::cerr << "DEBUG: findNextDataStatement() - line " << dataPos.lineNumber << " not found" << std::endl;
        dataPos.valid = false;
        return false;
    }
    
    std::cerr << "DEBUG: findNextDataStatement() - searching from line " << dataPos.lineNumber 
              << " token " << dataPos.tokenIndex << std::endl;
    
    // Search current line and subsequent lines for DATA statements
    while (it.isValid()) {
        const auto& tokens = it->tokens;
        std::cerr << "DEBUG: checking line " << it->lineNumber << " with " << tokens.size() << " tokens" << std::endl;
        
        // Debug: dump all tokens for line 10 (our DATA line)
        if (it->lineNumber == 10) {
            std::cerr << "DEBUG: Line 10 all tokens: ";
            for (size_t i = 0; i < tokens.size(); ++i) {
                std::cerr << "[" << i << "]=0x" << std::hex << static_cast<int>(tokens[i]) << std::dec;
                if (tokens[i] >= 32 && tokens[i] <= 126) {
                    std::cerr << "('" << static_cast<char>(tokens[i]) << "')";
                }
                std::cerr << " ";
            }
            std::cerr << std::endl;
        }
        
        // Look for DATA token in this line
        for (size_t i = (it->lineNumber == dataPos.lineNumber ? dataPos.tokenIndex : 0); 
             i < tokens.size(); ++i) {
            
            std::cerr << "DEBUG: token[" << i << "] = 0x" << std::hex << (int)tokens[i] << std::dec << std::endl;
            
            if (tokens[i] == tok->getTokenValue("DATA")) {
                // Found DATA statement
                std::cerr << "DEBUG: Found DATA token at line " << it->lineNumber << " token " << i << std::endl;
                dataPos.lineNumber = it->lineNumber;
                dataPos.tokenIndex = i + 1; // Position after DATA token
                dataPos.valid = true;
                return true;
            }
        }
        
        // Move to next line
        ++it;
        dataPos.tokenIndex = 0;
    }
    
    // No more DATA statements found
    std::cerr << "DEBUG: No DATA statements found" << std::endl;
    dataPos.valid = false;
    return false;
}

bool DataManager::parseNextValue(Value& result) {
    if (!prog || !tok || !dataPos.valid) {
        return false;
    }
    
    auto it = prog->findLine(dataPos.lineNumber);
    if (!it.isValid()) {
        dataPos.valid = false;
        return false;
    }
    
    const auto& tokens = it->tokens;
    size_t pos = dataPos.tokenIndex;
    
    // Skip whitespace and commas
    skipWhitespace(tokens, pos);
    
    // Check if we're at end of statement or line
    if (isEndOfStatement(tokens, pos)) {
        // No more values in this DATA statement
        return false;
    }
    
    // Skip comma if present
    if (pos < tokens.size() && (tokens[pos] == ',' || tokens[pos] == 0xF5)) {
        ++pos;
        skipWhitespace(tokens, pos);
    }
    
    // Parse the value
    if (parseTokenValue(tokens, pos, result)) {
        // Update position
        dataPos.tokenIndex = pos;
        
        // Skip to next value position
        skipToNextValue();
        
        return true;
    }
    
    return false;
}

void DataManager::skipToNextValue() {
    std::cerr << "DEBUG: skipToNextValue() - current pos=" << dataPos.tokenIndex << std::endl;
    
    if (!prog || !dataPos.valid) {
        std::cerr << "DEBUG: skipToNextValue() - invalid state" << std::endl;
        return;
    }
    
    auto it = prog->findLine(dataPos.lineNumber);
    if (!it.isValid()) {
        std::cerr << "DEBUG: skipToNextValue() - invalid line" << std::endl;
        dataPos.valid = false;
        return;
    }
    
    const auto& tokens = it->tokens;
    size_t pos = dataPos.tokenIndex;
    
    std::cerr << "DEBUG: skipToNextValue() - scanning from pos " << pos << ", tokens:";
    for (size_t i = pos; i < tokens.size() && i < pos + 5; ++i) {
        std::cerr << " 0x" << std::hex << static_cast<int>(tokens[i]) << std::dec;
    }
    std::cerr << std::endl;
    
    // Skip to next comma or end of statement
    while (pos < tokens.size() && 
           tokens[pos] != ',' && tokens[pos] != 0xF5 && // comma
           tokens[pos] != ':' && tokens[pos] != 0x00) { // statement separator or end
        ++pos;
    }
    
    // If we found a comma, move past it
    if (pos < tokens.size() && (tokens[pos] == ',' || tokens[pos] == 0xF5)) {
        std::cerr << "DEBUG: skipToNextValue() - found comma at pos " << pos << std::endl;
        ++pos;
    }
    
    std::cerr << "DEBUG: skipToNextValue() - final pos=" << pos << std::endl;
    dataPos.tokenIndex = pos;
}

bool DataManager::parseTokenValue(const std::vector<uint8_t>& tokens, size_t& pos, Value& result) {
    std::cerr << "DEBUG: parseTokenValue() - pos=" << pos << ", tokens.size()=" << tokens.size() << std::endl;
    
    if (pos >= tokens.size()) {
        std::cerr << "DEBUG: parseTokenValue() - pos >= tokens.size()" << std::endl;
        return false;
    }
    
    skipWhitespace(tokens, pos);
    
    if (pos >= tokens.size()) {
        std::cerr << "DEBUG: parseTokenValue() - pos >= tokens.size() after skipWhitespace" << std::endl;
        return false;
    }
    
    uint8_t token = tokens[pos];
    std::cerr << "DEBUG: parseTokenValue() - token=0x" << std::hex << static_cast<int>(token) << std::dec;
    if (token >= 32 && token <= 126) {
        std::cerr << " ('" << static_cast<char>(token) << "')";
    }
    std::cerr << std::endl;
    
    // Handle different token types
    if (token == '"') {
        // String literal
        ++pos; // Skip opening quote
        std::string str;
        
        while (pos < tokens.size() && tokens[pos] != '"' && tokens[pos] != 0x00) {
            str += static_cast<char>(tokens[pos++]);
        }
        
        if (pos < tokens.size() && tokens[pos] == '"') {
            ++pos; // Skip closing quote
        }
        
        // Create string value - for now we'll create a temporary StrDesc
        // that will be properly allocated when assigned to a variable
        StrDesc sd{};
        sd.len = static_cast<uint16_t>(str.length());
        sd.ptr = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(str.c_str()));
        result = Value::makeString(sd);
        return true;
        
    } else if (token == 0x11) {
        // Tokenized integer constant
        if (pos + 2 < tokens.size()) {
            int16_t value = static_cast<int16_t>(tokens[pos + 1]) | 
                           (static_cast<int16_t>(tokens[pos + 2]) << 8);
            pos += 3;
            result = Value::makeInt(value);
            return true;
        }
        
    } else if (token == 0x1C) {
        // Tokenized single-precision constant
        if (pos + 4 < tokens.size()) {
            // Extract 4-byte float from tokens
            union {
                float f;
                uint8_t bytes[4];
            } conv;
            
            for (int i = 0; i < 4; ++i) {
                conv.bytes[i] = tokens[pos + 1 + i];
            }
            
            pos += 5;
            result = Value::makeSingle(conv.f);
            return true;
        }
        
    } else if (token == 0x1D) {
        // Tokenized double-precision constant
        if (pos + 8 < tokens.size()) {
            // Extract 8-byte double from tokens
            union {
                double d;
                uint8_t bytes[8];
            } conv;
            
            for (int i = 0; i < 8; ++i) {
                conv.bytes[i] = tokens[pos + 1 + i];
            }
            
            pos += 9;
            result = Value::makeDouble(conv.d);
            return true;
        }
        
    } else if ((token >= '0' && token <= '9') || token == '.' || token == '-' || token == '+') {
        // ASCII numeric literal
        std::string numStr;
        
        // Handle sign
        if (token == '-' || token == '+') {
            numStr += static_cast<char>(token);
            ++pos;
        }
        
        // Collect digits and decimal point
        while (pos < tokens.size() && 
               ((tokens[pos] >= '0' && tokens[pos] <= '9') || 
                tokens[pos] == '.' || 
                tokens[pos] == 'E' || tokens[pos] == 'e' || 
                tokens[pos] == '+' || tokens[pos] == '-')) {
            
            numStr += static_cast<char>(tokens[pos++]);
            
            // Handle scientific notation
            if (numStr.length() > 1 && 
                (numStr.back() == 'E' || numStr.back() == 'e')) {
                // Next character might be a sign
                if (pos < tokens.size() && 
                    (tokens[pos] == '+' || tokens[pos] == '-')) {
                    numStr += static_cast<char>(tokens[pos++]);
                }
            }
        }
        
        if (!numStr.empty()) {
            try {
                // Try to parse as different numeric types
                if (numStr.find('.') == std::string::npos && 
                    numStr.find('E') == std::string::npos && 
                    numStr.find('e') == std::string::npos) {
                    // Integer
                    int32_t value = std::stoi(numStr);
                    if (value >= -32768 && value <= 32767) {
                        result = Value::makeInt(static_cast<int16_t>(value));
                        return true;
                    } else {
                        // Too large for int16, use single
                        result = Value::makeSingle(static_cast<float>(value));
                        return true;
                    }
                } else {
                    // Floating point
                    double value = std::stod(numStr);
                    
                    // Check if it fits in single precision
                    float singleVal = static_cast<float>(value);
                    if (static_cast<double>(singleVal) == value) {
                        result = Value::makeSingle(singleVal);
                    } else {
                        result = Value::makeDouble(value);
                    }
                    return true;
                }
            } catch (...) {
                // Parse error - return as string
                StrDesc sd{};
                sd.len = static_cast<uint16_t>(numStr.length());
                sd.ptr = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(numStr.c_str()));
                result = Value::makeString(sd);
                return true;
            }
        }
        
    } else if ((token >= 'A' && token <= 'Z') || (token >= 'a' && token <= 'z')) {
        // ASCII identifier - treat as string literal
        std::string str;
        
        while (pos < tokens.size() && 
               ((tokens[pos] >= 'A' && tokens[pos] <= 'Z') || 
                (tokens[pos] >= 'a' && tokens[pos] <= 'z') || 
                (tokens[pos] >= '0' && tokens[pos] <= '9') || 
                tokens[pos] == '_')) {
            str += static_cast<char>(tokens[pos++]);
        }
        
        StrDesc sd{};
        sd.len = static_cast<uint16_t>(str.length());
        sd.ptr = const_cast<uint8_t*>(reinterpret_cast<const uint8_t*>(str.c_str()));
        result = Value::makeString(sd);
        return true;
    }
    
    // Unknown token type - skip it
    std::cerr << "DEBUG: parseTokenValue() - unknown token type, returning false" << std::endl;
    ++pos;
    return false;
}

bool DataManager::isEndOfStatement(const std::vector<uint8_t>& tokens, size_t pos) const {
    return pos >= tokens.size() || 
           tokens[pos] == 0x00 || // end of line
           tokens[pos] == ':';    // statement separator
}

void DataManager::skipWhitespace(const std::vector<uint8_t>& tokens, size_t& pos) const {
    while (pos < tokens.size() && 
           (tokens[pos] == ' ' || tokens[pos] == '\t')) {
        ++pos;
    }
}

} // namespace gwbasic
