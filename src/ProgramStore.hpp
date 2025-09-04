#ifndef PROGRAMSTORE_H
#define PROGRAMSTORE_H

#include <cstdint>
#include <vector>
#include <memory>
#include <string>
#include <unordered_map>

/**
 * GW-BASIC Program Store Implementation
 * 
 * Maintains a linked list of tokenized BASIC program lines, sorted by line number.
 * Implements the same storage format as the original GW-BASIC:
 * Line layout: link(2) lineNo(2) tokens... 0x00
 * 
 * Operations supported:
 * - Insert/replace/delete by line number
 * - Scan via link fields
 * - Load/save program text
 * - Memory management for program storage
 */
class ProgramStore {
public:
    // Forward declaration of line structure
    struct ProgramLine;
    
    // Shared pointer type for program lines
    using ProgramLinePtr = std::shared_ptr<ProgramLine>;
    
    /**
     * Program line structure - matches GW-BASIC format
     * Layout: link(2) lineNo(2) tokens... 0x00
     */
    struct ProgramLine {
        ProgramLinePtr next;        // Link to next line (corresponds to 2-byte link)
        uint16_t lineNumber;        // Line number (2 bytes)
        std::vector<uint8_t> tokens; // Tokenized bytes terminated by 0x00
        
        ProgramLine(uint16_t lineNum = 0) 
            : next(nullptr), lineNumber(lineNum) {}
        
        // Get the total size of this line in bytes (as it would be stored in memory)
        size_t getSize() const {
            return 2 + 2 + tokens.size(); // link + lineNumber + tokens
        }
        
        // Check if line is properly terminated
        bool isValid() const {
            return !tokens.empty() && tokens.back() == 0x00;
        }
    };
    
    // Iterator class for traversing program lines
    class Iterator {
    public:
        Iterator(ProgramLinePtr line) : current(line) {}
        
        Iterator& operator++() {
            if (current) current = current->next;
            return *this;
        }
        
        Iterator operator++(int) {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        
        bool operator==(const Iterator& other) const {
            return current == other.current;
        }
        
        bool operator!=(const Iterator& other) const {
            return !(*this == other);
        }
        
        ProgramLine& operator*() const {
            return *current;
        }
        
        ProgramLinePtr operator->() const {
            return current;
        }
        
        bool isValid() const {
            return current != nullptr;
        }
        
    private:
        ProgramLinePtr current;
    };

public:
    // Constructor
    ProgramStore();
    
    // Destructor
    ~ProgramStore();
    
    // Core operations
    
    /**
     * Insert or replace a program line
     * @param lineNumber Line number (1-65529)
     * @param tokens Tokenized bytes (should be terminated with 0x00)
     * @return true if successful, false if error
     */
    bool insertLine(uint16_t lineNumber, const std::vector<uint8_t>& tokens);
    
    /**
     * Delete a program line
     * @param lineNumber Line number to delete
     * @return true if line was found and deleted, false if not found
     */
    bool deleteLine(uint16_t lineNumber);
    
    /**
     * Get a program line by line number
     * @param lineNumber Line number to find
     * @return shared_ptr to line, or nullptr if not found
     */
    ProgramLinePtr getLine(uint16_t lineNumber) const;
    
    /**
     * Check if a line number exists
     * @param lineNumber Line number to check
     * @return true if line exists
     */
    bool hasLine(uint16_t lineNumber) const;
    
    // Program management
    
    /**
     * Clear all program lines (NEW command)
     */
    void clear();
    
    /**
     * Get the first line in the program
     * @return Iterator to first line
     */
    Iterator begin() const;
    
    /**
     * Get end iterator
     * @return End iterator
     */
    Iterator end() const;
    
    /**
     * Find the line with the specified line number or the next higher line
     * @param lineNumber Target line number
     * @return Iterator to line >= lineNumber, or end() if none found
     */
    Iterator findLine(uint16_t lineNumber) const;
    
    /**
     * Get the next line after the specified line number
     * @param lineNumber Current line number
     * @return Iterator to next line, or end() if none
     */
    Iterator getNextLine(uint16_t lineNumber) const;
    
    // Program analysis
    
    /**
     * Get total number of lines in program
     * @return Number of program lines
     */
    size_t getLineCount() const;
    
    /**
     * Get total memory used by program (in bytes)
     * @return Total bytes used
     */
    size_t getTotalSize() const;
    
    /**
     * Get lowest line number in program
     * @return Lowest line number, or 0 if empty
     */
    uint16_t getFirstLineNumber() const;
    
    /**
     * Get highest line number in program
     * @return Highest line number, or 0 if empty
     */
    uint16_t getLastLineNumber() const;
    
    /**
     * Check if program is empty
     * @return true if no lines
     */
    bool isEmpty() const;
    
    // Serialization and listing
    
    /**
     * Convert program to tokenized binary format (for SAVE)
     * @return Binary representation of entire program
     */
    std::vector<uint8_t> serialize() const;
    
    /**
     * Load program from tokenized binary format (for LOAD)
     * @param data Binary program data
     * @return true if successful, false if corrupted data
     */
    bool deserialize(const std::vector<uint8_t>& data);
    
    /**
     * Get list of all line numbers (for LIST command)
     * @return Vector of line numbers in ascending order
     */
    std::vector<uint16_t> getLineNumbers() const;
    
    /**
     * Get lines in a range (for LIST start-end command)
     * @param startLine Starting line number (0 = from beginning)
     * @param endLine Ending line number (0 = to end)
     * @return Vector of lines in range
     */
    std::vector<ProgramLinePtr> getLinesInRange(uint16_t startLine = 0, uint16_t endLine = 0) const;
    
    // Memory management and pointers (for compatibility with GW-BASIC)
    
    /**
     * Get pointer to start of program text (TXTTAB equivalent)
     * @return Pointer to first line, or nullptr if empty
     */
    ProgramLinePtr getProgramStart() const;
    
    /**
     * Set current line pointer for execution (CURLIN equivalent)
     * @param lineNumber Line to set as current
     * @return true if line exists
     */
    bool setCurrentLine(uint16_t lineNumber);
    
    /**
     * Get current line pointer
     * @return Current line, or nullptr if not set
     */
    ProgramLinePtr getCurrentLine() const;
    
    /**
     * Get current line number
     * @return Current line number, or 0 if not set
     */
    uint16_t getCurrentLineNumber() const;
    
    // Validation and debugging
    
    /**
     * Validate program structure integrity
     * @return true if all links and structures are valid
     */
    bool validate() const;
    
    /**
     * Get debug information about program structure
     * @return String containing debug info
     */
    std::string getDebugInfo() const;
    
    // Line number utilities
    
    /**
     * Find a suitable line number for AUTO command
     * @param start Starting line number
     * @param increment Line number increment
     * @return Next available line number
     */
    uint16_t findNextAutoLineNumber(uint16_t start = 10, uint16_t increment = 10) const;
    
    /**
     * Renumber program lines (RENUM command)
     * @param newStart New starting line number
     * @param increment Line number increment
     * @param oldStart Old starting line number (0 = from beginning)
     * @param oldEnd Old ending line number (0 = to end)
     * @return true if successful
     */
    bool renumberLines(uint16_t newStart = 10, uint16_t increment = 10, 
                      uint16_t oldStart = 0, uint16_t oldEnd = 0);

private:
    // Internal data
    ProgramLinePtr firstLine;           // Head of linked list (TXTTAB equivalent)
    ProgramLinePtr currentLine;         // Current line for execution (CURLIN equivalent)
    size_t lineCount;                   // Number of lines
    size_t totalSize;                   // Total memory used
    
    // Line number index for fast lookups
    mutable std::unordered_map<uint16_t, ProgramLinePtr> lineIndex;
    mutable bool indexValid;            // Track if index needs rebuilding
    
    // Internal helper methods
    
    /**
     * Rebuild the line number index
     */
    void rebuildIndex() const;
    
    /**
     * Invalidate the line number index
     */
    void invalidateIndex();
    
    /**
     * Insert line in sorted order by line number
     * @param newLine Line to insert
     * @param tokens Tokenized content
     */
    void insertSorted(ProgramLinePtr newLine, const std::vector<uint8_t>& tokens);
    
    /**
     * Remove line from linked list
     * @param lineNumber Line number to remove
     * @return Removed line, or nullptr if not found
     */
    ProgramLinePtr removeFromList(uint16_t lineNumber);
    
    /**
     * Update cached statistics
     */
    void updateStatistics();
    
    /**
     * Validate line number (1-65529)
     * @param lineNumber Line number to check
     * @return true if valid
     */
    bool isValidLineNumber(uint16_t lineNumber) const;
    
    /**
     * Ensure tokens are properly terminated
     * @param tokens Token vector to check/fix
     */
    void ensureTokenTermination(std::vector<uint8_t>& tokens) const;
    
    // Constants
    static constexpr uint16_t MIN_LINE_NUMBER = 1;
    static constexpr uint16_t MAX_LINE_NUMBER = 65529;
    static constexpr uint8_t LINE_TERMINATOR = 0x00;
    static constexpr size_t LINK_SIZE = 2;
    static constexpr size_t LINE_NUMBER_SIZE = 2;
    static constexpr size_t LINE_HEADER_SIZE = LINK_SIZE + LINE_NUMBER_SIZE;
};

#endif // PROGRAMSTORE_H
