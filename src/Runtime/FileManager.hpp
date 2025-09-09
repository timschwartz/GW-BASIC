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

// File handle structure
struct FileHandle {
    uint8_t fileNumber;
    FileMode mode;
    std::string filename;
    std::unique_ptr<std::fstream> stream;
    bool isOpen;
    size_t position;  // Current read/write position
    
    FileHandle(uint8_t num, FileMode m, const std::string& name) 
        : fileNumber(num), mode(m), filename(name), isOpen(false), position(0) {}
    
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
};

class FileManager {
public:
    FileManager() = default;
    ~FileManager() { closeAll(); }
    
    // Disable copy constructor and assignment
    FileManager(const FileManager&) = delete;
    FileManager& operator=(const FileManager&) = delete;
    
    // OPEN "filename" FOR mode AS #filenumber
    bool openFile(uint8_t fileNumber, const std::string& filename, FileMode mode);
    
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
