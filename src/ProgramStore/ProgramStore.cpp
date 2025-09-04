#include "ProgramStore.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>

// Static member definitions (required for C++11/14)
constexpr uint16_t ProgramStore::MIN_LINE_NUMBER;
constexpr uint16_t ProgramStore::MAX_LINE_NUMBER;
constexpr uint8_t ProgramStore::LINE_TERMINATOR;
constexpr size_t ProgramStore::LINK_SIZE;
constexpr size_t ProgramStore::LINE_NUMBER_SIZE;
constexpr size_t ProgramStore::LINE_HEADER_SIZE;

// Constructor
ProgramStore::ProgramStore() 
    : firstLine(nullptr)
    , currentLine(nullptr)
    , lineCount(0)
    , totalSize(0)
    , indexValid(false) {
}

// Destructor
ProgramStore::~ProgramStore() {
    clear();
}

// Core operations

bool ProgramStore::insertLine(uint16_t lineNumber, const std::vector<uint8_t>& tokens) {
    if (!isValidLineNumber(lineNumber)) {
        return false;
    }
    
    // Make a copy of tokens and ensure proper termination
    std::vector<uint8_t> tokensCopy = tokens;
    ensureTokenTermination(tokensCopy);
    
    // Check if line already exists
    if (hasLine(lineNumber)) {
        // Replace existing line
        deleteLine(lineNumber);
    }
    
    // Create new line
    auto newLine = std::make_shared<ProgramLine>(lineNumber);
    newLine->tokens = std::move(tokensCopy);
    
    // Insert in sorted order
    insertSorted(newLine, newLine->tokens);
    
    // Update statistics and invalidate index
    lineCount++;
    totalSize += newLine->getSize();
    invalidateIndex();
    
    return true;
}

bool ProgramStore::deleteLine(uint16_t lineNumber) {
    auto removedLine = removeFromList(lineNumber);
    if (removedLine) {
        lineCount--;
        totalSize -= removedLine->getSize();
        
        // Update current line pointer if it was deleted
        if (currentLine == removedLine) {
            currentLine = nullptr;
        }
        
        invalidateIndex();
        return true;
    }
    return false;
}

ProgramStore::ProgramLinePtr ProgramStore::getLine(uint16_t lineNumber) const {
    if (!indexValid) {
        rebuildIndex();
    }
    
    auto it = lineIndex.find(lineNumber);
    return (it != lineIndex.end()) ? it->second : nullptr;
}

bool ProgramStore::hasLine(uint16_t lineNumber) const {
    return getLine(lineNumber) != nullptr;
}

// Program management

void ProgramStore::clear() {
    firstLine = nullptr;
    currentLine = nullptr;
    lineCount = 0;
    totalSize = 0;
    lineIndex.clear();
    indexValid = false;
}

ProgramStore::Iterator ProgramStore::begin() const {
    return Iterator(firstLine);
}

ProgramStore::Iterator ProgramStore::end() const {
    return Iterator(nullptr);
}

ProgramStore::Iterator ProgramStore::findLine(uint16_t lineNumber) const {
    auto current = firstLine;
    while (current && current->lineNumber < lineNumber) {
        current = current->next;
    }
    return Iterator(current);
}

ProgramStore::Iterator ProgramStore::getNextLine(uint16_t lineNumber) const {
    auto it = findLine(lineNumber);
    if (it.isValid() && it->lineNumber == lineNumber) {
        ++it;
    }
    return it;
}

// Program analysis

size_t ProgramStore::getLineCount() const {
    return lineCount;
}

size_t ProgramStore::getTotalSize() const {
    return totalSize;
}

uint16_t ProgramStore::getFirstLineNumber() const {
    return firstLine ? firstLine->lineNumber : 0;
}

uint16_t ProgramStore::getLastLineNumber() const {
    auto current = firstLine;
    uint16_t lastLineNum = 0;
    while (current) {
        lastLineNum = current->lineNumber;
        current = current->next;
    }
    return lastLineNum;
}

bool ProgramStore::isEmpty() const {
    return lineCount == 0;
}

// Serialization and listing

std::vector<uint8_t> ProgramStore::serialize() const {
    std::vector<uint8_t> result;
    
    auto current = firstLine;
    ProgramLinePtr nextLine = nullptr;
    
    while (current) {
        nextLine = current->next;
        
        // Calculate link value (offset to next line, or 0 if last)
        uint16_t linkValue = 0;
        if (nextLine) {
            linkValue = static_cast<uint16_t>(current->getSize());
        }
        
        // Write link (2 bytes, little-endian)
        result.push_back(static_cast<uint8_t>(linkValue & 0xFF));
        result.push_back(static_cast<uint8_t>((linkValue >> 8) & 0xFF));
        
        // Write line number (2 bytes, little-endian)
        result.push_back(static_cast<uint8_t>(current->lineNumber & 0xFF));
        result.push_back(static_cast<uint8_t>((current->lineNumber >> 8) & 0xFF));
        
        // Write tokens
        result.insert(result.end(), current->tokens.begin(), current->tokens.end());
        
        current = nextLine;
    }
    
    return result;
}

bool ProgramStore::deserialize(const std::vector<uint8_t>& data) {
    clear();
    
    if (data.empty()) {
        return true; // Empty program is valid
    }
    
    size_t pos = 0;
    while (pos < data.size()) {
        // Need at least 4 bytes for link and line number
        if (pos + 4 > data.size()) {
            clear(); // Corrupted data
            return false;
        }
        
        // Read link (2 bytes, little-endian)
        uint16_t link = data[pos] | (static_cast<uint16_t>(data[pos + 1]) << 8);
        pos += 2;
        
        // Read line number (2 bytes, little-endian)
        uint16_t lineNumber = data[pos] | (static_cast<uint16_t>(data[pos + 1]) << 8);
        pos += 2;
        
        if (!isValidLineNumber(lineNumber)) {
            clear(); // Invalid line number
            return false;
        }
        
        // Read tokens until 0x00 terminator or end of line
        std::vector<uint8_t> tokens;
        size_t lineStart = pos - 4;
        size_t lineEnd = (link == 0) ? data.size() : (lineStart + link);
        
        while (pos < lineEnd && pos < data.size()) {
            uint8_t token = data[pos++];
            tokens.push_back(token);
            if (token == 0x00) {
                break;
            }
        }
        
        // Ensure proper termination
        if (tokens.empty() || tokens.back() != 0x00) {
            tokens.push_back(0x00);
        }
        
        // Insert the line
        if (!insertLine(lineNumber, tokens)) {
            clear(); // Failed to insert
            return false;
        }
        
        // If link is 0, this was the last line
        if (link == 0) {
            break;
        }
        
        // Move to next line
        pos = lineStart + link;
    }
    
    return true;
}

std::vector<uint16_t> ProgramStore::getLineNumbers() const {
    std::vector<uint16_t> lineNumbers;
    lineNumbers.reserve(lineCount);
    
    auto current = firstLine;
    while (current) {
        lineNumbers.push_back(current->lineNumber);
        current = current->next;
    }
    
    return lineNumbers;
}

std::vector<ProgramStore::ProgramLinePtr> ProgramStore::getLinesInRange(uint16_t startLine, uint16_t endLine) const {
    std::vector<ProgramLinePtr> result;
    
    auto current = firstLine;
    while (current) {
        bool includeStart = (startLine == 0 || current->lineNumber >= startLine);
        bool includeEnd = (endLine == 0 || current->lineNumber <= endLine);
        
        if (includeStart && includeEnd) {
            result.push_back(current);
        } else if (endLine != 0 && current->lineNumber > endLine) {
            break; // Past the end of range
        }
        
        current = current->next;
    }
    
    return result;
}

// Memory management and pointers

ProgramStore::ProgramLinePtr ProgramStore::getProgramStart() const {
    return firstLine;
}

bool ProgramStore::setCurrentLine(uint16_t lineNumber) {
    auto line = getLine(lineNumber);
    if (line) {
        currentLine = line;
        return true;
    }
    return false;
}

ProgramStore::ProgramLinePtr ProgramStore::getCurrentLine() const {
    return currentLine;
}

uint16_t ProgramStore::getCurrentLineNumber() const {
    return currentLine ? currentLine->lineNumber : 0;
}

// Validation and debugging

bool ProgramStore::validate() const {
    if (isEmpty()) {
        return true;
    }
    
    auto current = firstLine;
    uint16_t lastLineNumber = 0;
    size_t actualLineCount = 0;
    size_t actualTotalSize = 0;
    
    while (current) {
        // Check line number ordering
        if (current->lineNumber <= lastLineNumber) {
            return false; // Not properly sorted
        }
        
        // Check line validity
        if (!current->isValid()) {
            return false; // Invalid line
        }
        
        // Check line number range
        if (!isValidLineNumber(current->lineNumber)) {
            return false; // Invalid line number
        }
        
        lastLineNumber = current->lineNumber;
        actualLineCount++;
        actualTotalSize += current->getSize();
        
        current = current->next;
    }
    
    // Check cached statistics
    return (actualLineCount == lineCount) && (actualTotalSize == totalSize);
}

std::string ProgramStore::getDebugInfo() const {
    std::ostringstream oss;
    oss << "ProgramStore Debug Info:\n";
    oss << "  Line Count: " << lineCount << "\n";
    oss << "  Total Size: " << totalSize << " bytes\n";
    oss << "  Index Valid: " << (indexValid ? "Yes" : "No") << "\n";
    oss << "  First Line: " << getFirstLineNumber() << "\n";
    oss << "  Last Line: " << getLastLineNumber() << "\n";
    oss << "  Current Line: " << getCurrentLineNumber() << "\n";
    oss << "  Validation: " << (validate() ? "PASS" : "FAIL") << "\n";
    
    if (!isEmpty()) {
        oss << "\nLine Details:\n";
        auto current = firstLine;
        while (current) {
            oss << "  Line " << current->lineNumber 
                << ": " << current->tokens.size() << " tokens, "
                << current->getSize() << " bytes\n";
            current = current->next;
        }
    }
    
    return oss.str();
}

// Line number utilities

uint16_t ProgramStore::findNextAutoLineNumber(uint16_t start, uint16_t increment) const {
    // GW-BASIC AUTO behavior: 
    // 1. If start is available and < increment, use it
    // 2. If start is a multiple of increment and available, use it
    // 3. Otherwise, find the next multiple of increment that is available
    
    uint16_t candidate = start;
    
    // Special case: if start < increment and available, use it
    if (start < increment && !hasLine(start)) {
        return start;
    }
    
    // If start is not a multiple of increment, round up to next multiple
    if (start % increment != 0) {
        candidate = ((start / increment) + 1) * increment;
    }
    
    // Now find the first available multiple of increment >= candidate
    while (candidate <= MAX_LINE_NUMBER) {
        if (!hasLine(candidate)) {
            return candidate;
        }
        
        // Check for overflow
        if (candidate > MAX_LINE_NUMBER - increment) {
            break;
        }
        
        candidate += increment;
    }
    
    return 0; // No available line number found
}

bool ProgramStore::renumberLines(uint16_t newStart, uint16_t increment, 
                                uint16_t oldStart, uint16_t oldEnd) {
    // Get lines to renumber
    auto linesToRenumber = getLinesInRange(oldStart, oldEnd);
    
    if (linesToRenumber.empty()) {
        return true; // Nothing to renumber
    }
    
    // Check if renumbering is possible
    uint16_t newLineNumber = newStart;
    for (size_t i = 0; i < linesToRenumber.size(); i++) {
        if (newLineNumber > MAX_LINE_NUMBER) {
            return false; // Would exceed maximum line number
        }
        
        // Check for overflow on next iteration
        if (i < linesToRenumber.size() - 1 && newLineNumber > MAX_LINE_NUMBER - increment) {
            return false;
        }
        
        newLineNumber += increment;
    }
    
    // Create mapping of old to new line numbers
    std::vector<std::pair<uint16_t, uint16_t>> renumberMap;
    newLineNumber = newStart;
    
    for (auto& line : linesToRenumber) {
        renumberMap.emplace_back(line->lineNumber, newLineNumber);
        newLineNumber += increment;
    }
    
    // Remove old lines and add with new line numbers
    for (auto& mapping : renumberMap) {
        uint16_t oldLineNum = mapping.first;
        uint16_t newLineNum = mapping.second;
        
        auto line = getLine(oldLineNum);
        if (line) {
            std::vector<uint8_t> tokens = line->tokens;
            deleteLine(oldLineNum);
            insertLine(newLineNum, tokens);
        }
    }
    
    return true;
}

// Private helper methods

void ProgramStore::rebuildIndex() const {
    lineIndex.clear();
    
    auto current = firstLine;
    while (current) {
        lineIndex[current->lineNumber] = current;
        current = current->next;
    }
    
    indexValid = true;
}

void ProgramStore::invalidateIndex() {
    indexValid = false;
}

void ProgramStore::insertSorted(ProgramLinePtr newLine, const std::vector<uint8_t>& tokens) {
    newLine->tokens = tokens;
    
    // Find insertion point
    if (!firstLine || newLine->lineNumber < firstLine->lineNumber) {
        // Insert at beginning
        newLine->next = firstLine;
        firstLine = newLine;
    } else {
        // Find correct position
        auto current = firstLine;
        while (current->next && current->next->lineNumber < newLine->lineNumber) {
            current = current->next;
        }
        
        // Insert after current
        newLine->next = current->next;
        current->next = newLine;
    }
}

ProgramStore::ProgramLinePtr ProgramStore::removeFromList(uint16_t lineNumber) {
    if (!firstLine) {
        return nullptr;
    }
    
    // Check if first line is the target
    if (firstLine->lineNumber == lineNumber) {
        auto removedLine = firstLine;
        firstLine = firstLine->next;
        removedLine->next = nullptr; // Clean up link
        return removedLine;
    }
    
    // Search for line to remove
    auto current = firstLine;
    while (current->next && current->next->lineNumber != lineNumber) {
        current = current->next;
    }
    
    if (current->next && current->next->lineNumber == lineNumber) {
        auto removedLine = current->next;
        current->next = removedLine->next;
        removedLine->next = nullptr; // Clean up link
        return removedLine;
    }
    
    return nullptr; // Line not found
}

void ProgramStore::updateStatistics() {
    lineCount = 0;
    totalSize = 0;
    
    auto current = firstLine;
    while (current) {
        lineCount++;
        totalSize += current->getSize();
        current = current->next;
    }
}

bool ProgramStore::isValidLineNumber(uint16_t lineNumber) const {
    return lineNumber >= MIN_LINE_NUMBER && lineNumber <= MAX_LINE_NUMBER;
}

void ProgramStore::ensureTokenTermination(std::vector<uint8_t>& tokens) const {
    if (tokens.empty() || tokens.back() != LINE_TERMINATOR) {
        tokens.push_back(LINE_TERMINATOR);
    }
}
