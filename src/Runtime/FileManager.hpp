#pragma once

#include <string>
#include <unordered_map>
#include <fstream>
#include <memory>
#include <vector>
#include <cstdint>

namespace gwbasic {

// File modes for OPEN statement
enum class FileMode {
    INPUT = 1,    // Sequential input
    OUTPUT = 2,   // Sequential output  
    APPEND = 3,   // Append to end of file
    RANDOM = 4    // Random access (not implemented yet)
};

// Record field definition for random access files
struct FileField {
    std::string name;
    size_t offset;
    size_t length;
    bool isString;  // true for string fields, false for numeric
    
    FileField(const std::string& n, size_t off, size_t len, bool str = true)
        : name(n), offset(off), length(len), isString(str) {}
};

// File handle structure
struct FileHandle {
    uint8_t fileNumber;
    FileMode mode;
    std::string filename;
    std::unique_ptr<std::fstream> stream;
    bool isOpen;
    size_t position;  // Current read/write position
    size_t recordLength;  // Record length for random access files
    std::vector<FileField> fields;  // FIELD definitions for random access
    std::vector<uint8_t> recordBuffer;  // Buffer for current record
    
    FileHandle(uint8_t num, FileMode m, const std::string& name, size_t recLen = 128) 
        : fileNumber(num), mode(m), filename(name), isOpen(false), position(0), 
          recordLength(recLen), recordBuffer(recLen, 0) {}
    
    // Check if file is at end
    bool isEOF() const {
        if (!stream || !isOpen) return true;
        return stream->eof() || stream->peek() == EOF;
    }
    
    // Get current file position
    size_t tell() const {
        if (!stream || !isOpen) return 0;
        return static_cast<size_t>(stream->tellg());
    }
    
    // Get current record number (1-based)
    size_t getCurrentRecord() const {
        if (mode != FileMode::RANDOM || recordLength == 0) return 0;
        return (position / recordLength) + 1;
    }
};

class FileManager {
public:
    FileManager() = default;
    ~FileManager() { closeAll(); }
    
    // Disable copy constructor and assignment
    FileManager(const FileManager&) = delete;
    FileManager& operator=(const FileManager&) = delete;
    
    // OPEN "filename" FOR mode AS #filenumber [LEN=recordlength]
    bool openFile(uint8_t fileNumber, const std::string& filename, FileMode mode, size_t recordLength = 128);
    
    // CLOSE [#filenumber] - if no number specified, close all files
    bool closeFile(uint8_t fileNumber);
    void closeAll();
    
    // Check if a file number is open
    bool isFileOpen(uint8_t fileNumber) const;
    
    // Get file handle for operations
    FileHandle* getFile(uint8_t fileNumber);
    const FileHandle* getFile(uint8_t fileNumber) const;
    
    // INPUT# filenumber, variable - read a line from file
    bool readLine(uint8_t fileNumber, std::string& line);
    
    // PRINT# filenumber, data - write data to file
    bool writeLine(uint8_t fileNumber, const std::string& data);
    bool writeData(uint8_t fileNumber, const std::string& data);
    
    // Check if file is at EOF
    bool isEOF(uint8_t fileNumber) const;
    
    // Random access file operations
    // FIELD #filenumber, fieldwidth AS string$[, fieldwidth AS string$]...
    bool fieldFile(uint8_t fileNumber, const std::vector<std::pair<size_t, std::string>>& fieldDefs);
    
    // GET #filenumber, [recordnumber]
    bool getRecord(uint8_t fileNumber, size_t recordNumber = 0);
    
    // PUT #filenumber, [recordnumber]  
    bool putRecord(uint8_t fileNumber, size_t recordNumber = 0);
    
    // LSET string$ = expression - set field value left-justified
    bool lsetField(uint8_t fileNumber, const std::string& fieldName, const std::string& value);
    
    // RSET string$ = expression - set field value right-justified
    bool rsetField(uint8_t fileNumber, const std::string& fieldName, const std::string& value);
    
    // Get field value from current record buffer
    std::string getFieldValue(uint8_t fileNumber, const std::string& fieldName) const;
    
    // Get list of open file numbers
    std::vector<uint8_t> getOpenFiles() const;
    
    // File validation
    static bool isValidFileNumber(uint8_t fileNumber) {
        return fileNumber >= 1 && fileNumber <= 255;
    }
    
    static std::string getModeString(FileMode mode) {
        switch (mode) {
            case FileMode::INPUT: return "INPUT";
            case FileMode::OUTPUT: return "OUTPUT";
            case FileMode::APPEND: return "APPEND";
            case FileMode::RANDOM: return "RANDOM";
            default: return "UNKNOWN";
        }
    }

private:
    std::unordered_map<uint8_t, std::unique_ptr<FileHandle>> files_;
    
    // Helper to get proper file mode flags for fstream
    std::ios::openmode getOpenMode(FileMode mode) const;
    
    // Validate file operations
    bool validateFileForRead(uint8_t fileNumber) const;
    bool validateFileForWrite(uint8_t fileNumber) const;
};

} // namespace gwbasic
