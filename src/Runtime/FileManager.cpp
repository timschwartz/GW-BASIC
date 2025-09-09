#include "FileManager.hpp"
#include <iostream>
#include <algorithm>

namespace gwbasic {

bool FileManager::openFile(uint8_t fileNumber, const std::string& filename, FileMode mode) {
    // Validate file number
    if (!isValidFileNumber(fileNumber)) {
        return false;
    }
    
    // Check if file number is already in use
    if (isFileOpen(fileNumber)) {
        return false; // File already open
    }
    
    // Validate filename
    if (filename.empty()) {
        return false;
    }
    
    // Create new file handle
    auto handle = std::make_unique<FileHandle>(fileNumber, mode, filename);
    handle->stream = std::make_unique<std::fstream>();
    
    // Open the file with appropriate mode
    std::ios::openmode openMode = getOpenMode(mode);
    
    try {
        handle->stream->open(filename, openMode);
        
        if (!handle->stream->is_open()) {
            return false; // Failed to open file
        }
        
        // For INPUT mode, check if file exists and is readable
        if (mode == FileMode::INPUT) {
            if (handle->stream->fail() || handle->stream->bad()) {
                return false;
            }
        }
        
        // For APPEND mode, seek to end
        if (mode == FileMode::APPEND) {
            handle->stream->seekp(0, std::ios::end);
        }
        
        handle->isOpen = true;
        handle->position = static_cast<size_t>(handle->stream->tellg());
        
        // Store the file handle
        files_[fileNumber] = std::move(handle);
        
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

bool FileManager::closeFile(uint8_t fileNumber) {
    auto it = files_.find(fileNumber);
    if (it == files_.end()) {
        return false; // File not open
    }
    
    try {
        if (it->second->stream && it->second->stream->is_open()) {
            it->second->stream->close();
        }
        
        it->second->isOpen = false;
        files_.erase(it);
        
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

void FileManager::closeAll() {
    // Close all open files
    for (auto& pair : files_) {
        if (pair.second && pair.second->stream && pair.second->stream->is_open()) {
            try {
                pair.second->stream->close();
            } catch (...) {
                // Ignore errors during cleanup
            }
        }
    }
    files_.clear();
}

bool FileManager::isFileOpen(uint8_t fileNumber) const {
    auto it = files_.find(fileNumber);
    return it != files_.end() && it->second->isOpen;
}

FileHandle* FileManager::getFile(uint8_t fileNumber) {
    auto it = files_.find(fileNumber);
    return it != files_.end() ? it->second.get() : nullptr;
}

const FileHandle* FileManager::getFile(uint8_t fileNumber) const {
    auto it = files_.find(fileNumber);
    return it != files_.end() ? it->second.get() : nullptr;
}

bool FileManager::readLine(uint8_t fileNumber, std::string& line) {
    if (!validateFileForRead(fileNumber)) {
        return false;
    }
    
    auto* handle = getFile(fileNumber);
    if (!handle || !handle->stream) {
        return false;
    }
    
    try {
        if (std::getline(*handle->stream, line)) {
            handle->position = static_cast<size_t>(handle->stream->tellg());
            return true;
        }
        return false; // EOF or error
        
    } catch (const std::exception&) {
        return false;
    }
}

bool FileManager::writeLine(uint8_t fileNumber, const std::string& data) {
    return writeData(fileNumber, data + "\n");
}

bool FileManager::writeData(uint8_t fileNumber, const std::string& data) {
    if (!validateFileForWrite(fileNumber)) {
        return false;
    }
    
    auto* handle = getFile(fileNumber);
    if (!handle || !handle->stream) {
        return false;
    }
    
    try {
        *handle->stream << data;
        handle->stream->flush(); // Ensure data is written immediately
        
        if (handle->stream->fail()) {
            return false;
        }
        
        handle->position = static_cast<size_t>(handle->stream->tellp());
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

bool FileManager::isEOF(uint8_t fileNumber) const {
    const auto* handle = getFile(fileNumber);
    if (!handle) {
        return true; // File not open = EOF
    }
    
    return handle->isEOF();
}

std::vector<uint8_t> FileManager::getOpenFiles() const {
    std::vector<uint8_t> result;
    result.reserve(files_.size());
    
    for (const auto& pair : files_) {
        if (pair.second->isOpen) {
            result.push_back(pair.first);
        }
    }
    
    // Sort for consistent ordering
    std::sort(result.begin(), result.end());
    
    return result;
}

std::ios::openmode FileManager::getOpenMode(FileMode mode) const {
    switch (mode) {
        case FileMode::INPUT:
            return std::ios::in;
        
        case FileMode::OUTPUT:
            return std::ios::out | std::ios::trunc; // Truncate existing file
        
        case FileMode::APPEND:
            return std::ios::out | std::ios::app;   // Append to existing file
        
        case FileMode::RANDOM:
            return std::ios::in | std::ios::out | std::ios::binary;
        
        default:
            return std::ios::in;
    }
}

bool FileManager::validateFileForRead(uint8_t fileNumber) const {
    const auto* handle = getFile(fileNumber);
    if (!handle || !handle->isOpen) {
        return false;
    }
    
    // Check if file mode allows reading
    return handle->mode == FileMode::INPUT || handle->mode == FileMode::RANDOM;
}

bool FileManager::validateFileForWrite(uint8_t fileNumber) const {
    const auto* handle = getFile(fileNumber);
    if (!handle || !handle->isOpen) {
        return false;
    }
    
    // Check if file mode allows writing
    return handle->mode == FileMode::OUTPUT || 
           handle->mode == FileMode::APPEND || 
           handle->mode == FileMode::RANDOM;
}

} // namespace gwbasic
