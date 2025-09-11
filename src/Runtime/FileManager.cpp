#include "FileManager.hpp"
#include <iostream>
#include <algorithm>

namespace gwbasic {

bool FileManager::openFile(uint8_t fileNumber, const std::string& filename, FileMode mode, size_t recordLength) {
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
    auto handle = std::make_unique<FileHandle>(fileNumber, mode, filename, recordLength);
    handle->stream = std::make_unique<std::fstream>();
    
    // Open the file with appropriate mode
    std::ios::openmode openMode = getOpenMode(mode);
    
    try {
        handle->stream->open(filename, openMode);
        
        // For RANDOM mode, if file doesn't exist, create it
        if (mode == FileMode::RANDOM && !handle->stream->is_open()) {
            handle->stream->clear(); // Clear any error flags
            handle->stream->open(filename, std::ios::in | std::ios::out | std::ios::binary | std::ios::trunc);
        }
        
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
            // For random access, try to open existing file first, create if needed
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

bool FileManager::fieldFile(uint8_t fileNumber, const std::vector<std::pair<size_t, std::string>>& fieldDefs) {
    auto* handle = getFile(fileNumber);
    if (!handle || !handle->isOpen || handle->mode != FileMode::RANDOM) {
        return false;
    }
    
    // Clear existing field definitions
    handle->fields.clear();
    
    size_t offset = 0;
    for (const auto& fieldDef : fieldDefs) {
        size_t width = fieldDef.first;
        const std::string& name = fieldDef.second;
        
        if (offset + width > handle->recordLength) {
            return false; // Field extends beyond record length
        }
        
        handle->fields.emplace_back(name, offset, width, true);
        offset += width;
    }
    
    return true;
}

bool FileManager::getRecord(uint8_t fileNumber, size_t recordNumber) {
    auto* handle = getFile(fileNumber);
    if (!handle || !handle->isOpen || handle->mode != FileMode::RANDOM) {
        return false;
    }
    
    try {
        size_t targetPos;
        if (recordNumber == 0) {
            // Use current position
            targetPos = handle->position;
        } else {
            // Calculate position for specific record (1-based)
            targetPos = (recordNumber - 1) * handle->recordLength;
        }
        
        // Seek to record position
        handle->stream->seekg(targetPos);
        if (handle->stream->fail()) {
            return false;
        }
        
        // Read record into buffer
        handle->stream->read(reinterpret_cast<char*>(handle->recordBuffer.data()), 
                           handle->recordLength);
        
        size_t bytesRead = static_cast<size_t>(handle->stream->gcount());
        
        // Pad with zeros if we read less than record length
        if (bytesRead < handle->recordLength) {
            std::fill(handle->recordBuffer.begin() + bytesRead, 
                     handle->recordBuffer.end(), 0);
        }
        
        handle->position = targetPos;
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

bool FileManager::putRecord(uint8_t fileNumber, size_t recordNumber) {
    auto* handle = getFile(fileNumber);
    if (!handle || !handle->isOpen || handle->mode != FileMode::RANDOM) {
        return false;
    }
    
    try {
        size_t targetPos;
        if (recordNumber == 0) {
            // Use current position
            targetPos = handle->position;
        } else {
            // Calculate position for specific record (1-based)
            targetPos = (recordNumber - 1) * handle->recordLength;
        }
        
        // Seek to record position
        handle->stream->seekp(targetPos);
        if (handle->stream->fail()) {
            return false;
        }
        
        // Write record from buffer
        handle->stream->write(reinterpret_cast<const char*>(handle->recordBuffer.data()), 
                            handle->recordLength);
        handle->stream->flush();
        
        if (handle->stream->fail()) {
            return false;
        }
        
        handle->position = targetPos;
        return true;
        
    } catch (const std::exception&) {
        return false;
    }
}

bool FileManager::lsetField(uint8_t fileNumber, const std::string& fieldName, const std::string& value) {
    auto* handle = getFile(fileNumber);
    if (!handle || !handle->isOpen || handle->mode != FileMode::RANDOM) {
        return false;
    }
    
    // Find the field
    for (const auto& field : handle->fields) {
        if (field.name == fieldName) {
            // Left-justify the value in the field
            std::string paddedValue = value;
            if (paddedValue.length() > field.length) {
                paddedValue = paddedValue.substr(0, field.length);
            } else {
                paddedValue.resize(field.length, ' '); // Pad with spaces
            }
            
            // Copy to record buffer
            std::copy(paddedValue.begin(), paddedValue.end(), 
                     handle->recordBuffer.begin() + field.offset);
            return true;
        }
    }
    
    return false; // Field not found
}

bool FileManager::rsetField(uint8_t fileNumber, const std::string& fieldName, const std::string& value) {
    auto* handle = getFile(fileNumber);
    if (!handle || !handle->isOpen || handle->mode != FileMode::RANDOM) {
        return false;
    }
    
    // Find the field
    for (const auto& field : handle->fields) {
        if (field.name == fieldName) {
            // Right-justify the value in the field
            std::string paddedValue;
            if (value.length() > field.length) {
                paddedValue = value.substr(0, field.length);
            } else {
                paddedValue = std::string(field.length - value.length(), ' ') + value;
            }
            
            // Copy to record buffer
            std::copy(paddedValue.begin(), paddedValue.end(), 
                     handle->recordBuffer.begin() + field.offset);
            return true;
        }
    }
    
    return false; // Field not found
}

std::string FileManager::getFieldValue(uint8_t fileNumber, const std::string& fieldName) const {
    const auto* handle = getFile(fileNumber);
    if (!handle || !handle->isOpen || handle->mode != FileMode::RANDOM) {
        return "";
    }
    
    // Find the field
    for (const auto& field : handle->fields) {
        if (field.name == fieldName) {
            std::string value(handle->recordBuffer.begin() + field.offset,
                            handle->recordBuffer.begin() + field.offset + field.length);
            
            // Remove trailing spaces for string fields
            if (field.isString) {
                size_t end = value.find_last_not_of(' ');
                if (end != std::string::npos) {
                    value = value.substr(0, end + 1);
                } else {
                    value.clear();
                }
            }
            
            return value;
        }
    }
    
    return ""; // Field not found
}

} // namespace gwbasic
