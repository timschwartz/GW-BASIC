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
    dataPos = DataPosition{}; // Reset to invalid position
    
    if (!prog) {
        return;
    }
    
    // Start from the beginning of the program
    auto it = prog->begin();
    if (it != prog->end()) {
        dataPos.lineNumber = it->lineNumber;
        dataPos.tokenIndex = 0;
        dataPos.valid = true;
        
        // Find the first DATA statement
        findNextDataStatement();
    }
}

void DataManager::restore(uint16_t lineNumber) {
    dataPos = DataPosition{}; // Reset to invalid position
    
    if (!prog) {
        return;
    }
    
    // Special case: RESTORE 0 means restore to beginning (same as RESTORE without args)
    if (lineNumber == 0) {
        restore();
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
    if (!prog || !tok) {
        return false;
    }
    
    // If we don't have a valid position, try to find the first DATA statement
    if (!dataPos.valid) {
        restore();
        if (!dataPos.valid) {
            return false;
        }
    }
    
    // Parse the next value from current DATA statement
    if (parseNextValue(result)) {
        return true;
    }
    
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
        return false;
    }
    
    // Start from current position and look for DATA statements
    auto it = prog->findLine(dataPos.lineNumber);
    if (!it.isValid()) {
        dataPos.valid = false;
        return false;
    }
    
    // Search current line and subsequent lines for DATA statements
    while (it.isValid()) {
        const auto& tokens = it->tokens;
        
    // Look for DATA token in this line
        for (size_t i = (it->lineNumber == dataPos.lineNumber ? dataPos.tokenIndex : 0); 
             i < tokens.size(); ++i) {
            
            if (tokens[i] == tok->getTokenValue("DATA")) {
                // Found DATA statement
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
    // Skip leading commas if sitting on one
    if (pos < tokens.size() && (tokenIs(tokens, pos, ",") || tokens[pos] == ',')) {
        ++pos;
        skipWhitespace(tokens, pos);
    }

    // Check if we're at end of statement or line
    if (isEndOfStatement(tokens, pos)) {
        // No more values in this DATA statement
        return false;
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
    if (!prog || !dataPos.valid) {
        return;
    }
    
    auto it = prog->findLine(dataPos.lineNumber);
    if (!it.isValid()) {
        dataPos.valid = false;
        return;
    }
    
    const auto& tokens = it->tokens;
    size_t pos = dataPos.tokenIndex;
    
    // Skip to next comma or end of statement
    while (pos < tokens.size() &&
        !tokenIs(tokens, pos, ",") && tokens[pos] != ',' &&
        !tokenIs(tokens, pos, ":") && tokens[pos] != 0x00) {
        ++pos;
    }
    
    // If we found a comma, move past it
    if (pos < tokens.size() && (tokenIs(tokens, pos, ",") || tokens[pos] == ',')) {
        ++pos;
    }
    
    dataPos.tokenIndex = pos;
}

bool DataManager::parseTokenValue(const std::vector<uint8_t>& tokens, size_t& pos, Value& result) {
    if (pos >= tokens.size()) {
        return false;
    }
    
    skipWhitespace(tokens, pos);
    
    if (pos >= tokens.size()) {
        return false;
    }
    
    uint8_t token = tokens[pos];
    
    // Handle optional tokenized sign before numeric constants
    int sign = 1;
    if (tokenIs(tokens, pos, "+") || tokenIs(tokens, pos, "-")) {
        if (tokenIs(tokens, pos, "-")) sign = -1;
        ++pos;
        skipWhitespace(tokens, pos);
        if (pos >= tokens.size()) return false;
        token = tokens[pos];
    }
    
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
        
    // Hold data in a member buffer to ensure lifetime until assignment
    tempStrBuffer = std::move(str);
    StrDesc sd{};
    sd.len = static_cast<uint16_t>(tempStrBuffer.length());
    sd.ptr = reinterpret_cast<uint8_t*>(tempStrBuffer.data());
        result = Value::makeString(sd);
        return true;
        
    } else if (token == 0x11) {
        // Tokenized integer constant
        if (pos + 2 < tokens.size()) {
        int16_t value = static_cast<int16_t>(tokens[pos + 1]) | 
               (static_cast<int16_t>(tokens[pos + 2]) << 8);
        value = static_cast<int16_t>(value * sign);
            pos += 3;
            result = Value::makeInt(value);
            return true;
        }
        
    } else if (token == 0x1D) {
        // Tokenized single-precision constant (4 bytes)
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
        result = Value::makeSingle(conv.f * static_cast<float>(sign));
            return true;
        }
        
    } else if (token == 0x1F) {
        // Tokenized double-precision constant (8 bytes)
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
        result = Value::makeDouble(conv.d * static_cast<double>(sign));
            return true;
        }
        
    } else if ((token >= '0' && token <= '9') || token == '.' || token == '-' || token == '+') {
        // ASCII numeric literal
        std::string numStr;
        
    // Handle sign from leading ASCII if present or from prior tokenized sign
    if (sign < 0) numStr += '-';
    if (token == '-' || token == '+') { numStr += static_cast<char>(token); ++pos; }
        
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
        tempStrBuffer = numStr;
        StrDesc sd{};
        sd.len = static_cast<uint16_t>(tempStrBuffer.length());
        sd.ptr = reinterpret_cast<uint8_t*>(tempStrBuffer.data());
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
        
    tempStrBuffer = std::move(str);
    StrDesc sd{};
    sd.len = static_cast<uint16_t>(tempStrBuffer.length());
    sd.ptr = reinterpret_cast<uint8_t*>(tempStrBuffer.data());
        result = Value::makeString(sd);
        return true;
    }
    
    // Unknown token type - skip it
    ++pos;
    return false;
}

bool DataManager::isEndOfStatement(const std::vector<uint8_t>& tokens, size_t pos) const {
    return pos >= tokens.size() ||
           tokens[pos] == 0x00 ||
           tokenIs(tokens, pos, ":") || tokens[pos] == ':';
}

void DataManager::skipWhitespace(const std::vector<uint8_t>& tokens, size_t& pos) const {
    while (pos < tokens.size() && 
           (tokens[pos] == ' ' || tokens[pos] == '\t')) {
        ++pos;
    }
}

bool DataManager::tokenIs(const std::vector<uint8_t>& tokens, size_t pos, const char* symbol) const {
    if (pos >= tokens.size() || symbol == nullptr || *symbol == '\0') return false;
    // Match ASCII directly
    if (tokens[pos] == static_cast<uint8_t>(symbol[0]) && symbol[1] == '\0') return true;
    // Match tokenized form via tokenizer
    if (!tok) return false;
    std::string name = tok->getTokenName(tokens[pos]);
    return name == symbol;
}

} // namespace gwbasic
