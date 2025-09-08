// Central string management system for GW-BASIC runtime
#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include <string>
#include "StringHeap.hpp"
#include "StringTypes.hpp"

namespace gwbasic {

/**
 * StringManager provides a high-level interface for string operations in GW-BASIC.
 * It manages a string heap with automatic garbage collection and provides
 * convenient methods for common string operations.
 */
class StringManager : public StringRootProvider {
public:
    // Configuration for string manager
    struct Config {
        std::size_t heapSize;                    // Default 8KB string heap
        StringHeap::GCPolicy gcPolicy;
        double gcThreshold;                       // 20% threshold
        std::size_t tempPoolCapacity;              // Temporary string pool size
        
        Config() : heapSize(8192), gcPolicy(StringHeap::GCPolicy::OnDemand), 
                   gcThreshold(0.2), tempPoolCapacity(32) {}
    };

    explicit StringManager(const Config& config = Config())
        : config_(config),
          heapBuffer_(std::make_unique<uint8_t[]>(config.heapSize)),
          heap_(heapBuffer_.get(), config.heapSize, config.gcPolicy),
          tempPool_(config.tempPoolCapacity) {
        
        heap_.setGCThreshold(config.gcThreshold);
        heap_.addRootProvider(this);
    }

    ~StringManager() = default;

    // StringRootProvider implementation
    void collectStringRoots(std::vector<StrDesc*>& roots) override {
        tempPool_.collectStringRoots(roots);
    }

    // Get the underlying heap (for integration with other components)
    StringHeap* getHeap() { return &heap_; }
    const StringHeap* getHeap() const { return &heap_; }

    // Get the temporary string pool
    TempStrPool* getTempPool() { return &tempPool_; }

    // String creation operations
    bool createString(const char* value, StrDesc& out) {
        return heap_.allocCopy(value, out);
    }

    bool createString(const uint8_t* value, uint16_t len, StrDesc& out) {
        return heap_.allocCopy(value, len, out);
    }

    bool createString(const std::string& value, StrDesc& out) {
        if (value.length() > 255) return false;
        return heap_.allocCopy(reinterpret_cast<const uint8_t*>(value.c_str()), 
                              static_cast<uint16_t>(value.length()), out);
    }

    // String concatenation with automatic memory management
    bool concatenate(const StrDesc& left, const StrDesc& right, StrDesc& result) {
        if (left.len + right.len > 255) return false; // GW-BASIC string limit
        
        if (!heap_.alloc(left.len + right.len, result)) return false;
        
        if (left.len > 0) {
            std::copy(left.ptr, left.ptr + left.len, result.ptr);
        }
        if (right.len > 0) {
            std::copy(right.ptr, right.ptr + right.len, result.ptr + left.len);
        }
        
        return true;
    }

    // String slicing operations (LEFT$, RIGHT$, MID$)
    bool left(const StrDesc& source, uint16_t count, StrDesc& result) {
        if (count == 0) {
            result = {};
            return true;
        }
        
        uint16_t len = std::min(count, source.len);
        if (!heap_.alloc(len, result)) return false;
        
        if (len > 0) {
            std::copy(source.ptr, source.ptr + len, result.ptr);
        }
        
        return true;
    }

    bool right(const StrDesc& source, uint16_t count, StrDesc& result) {
        if (count == 0 || source.len == 0) {
            result = {};
            return true;
        }
        
        uint16_t len = std::min(count, source.len);
        if (!heap_.alloc(len, result)) return false;
        
        uint8_t* start = source.ptr + (source.len - len);
        std::copy(start, start + len, result.ptr);
        
        return true;
    }

    bool mid(const StrDesc& source, uint16_t start1based, int optCount, StrDesc& result) {
        if (start1based < 1 || start1based > source.len || source.len == 0) {
            result = {};
            return true;
        }
        
        uint16_t start0 = start1based - 1;
        uint16_t remain = source.len - start0;
        uint16_t len = (optCount < 0) ? remain : std::min(remain, static_cast<uint16_t>(optCount));
        
        if (len == 0) {
            result = {};
            return true;
        }
        
        if (!heap_.alloc(len, result)) return false;
        std::copy(source.ptr + start0, source.ptr + start0 + len, result.ptr);
        
        return true;
    }

    // String search (INSTR function)
    int instr(const StrDesc& source, const StrDesc& search, uint16_t start1based = 1) const {
        if (search.len == 0 || source.len == 0 || start1based < 1) return 0;
        if (start1based > source.len) return 0;
        
        uint16_t start0 = start1based - 1;
        for (uint16_t i = start0; i <= source.len - search.len; ++i) {
            if (std::equal(search.ptr, search.ptr + search.len, source.ptr + i)) {
                return static_cast<int>(i + 1); // Return 1-based position
            }
        }
        
        return 0; // Not found
    }

    // String comparison (for relational operators)
    int compare(const StrDesc& left, const StrDesc& right) const {
        std::size_t minLen = std::min(left.len, right.len);
        
        for (std::size_t i = 0; i < minLen; ++i) {
            if (left.ptr[i] < right.ptr[i]) return -1;
            if (left.ptr[i] > right.ptr[i]) return 1;
        }
        
        // If common prefix is equal, compare lengths
        if (left.len < right.len) return -1;
        if (left.len > right.len) return 1;
        return 0;
    }

    // Convert string to C-style string (for debugging/display)
    std::string toString(const StrDesc& desc) const {
        if (desc.len == 0) return std::string{};
        return std::string(reinterpret_cast<const char*>(desc.ptr), desc.len);
    }

    // Temporary string management
    StrDesc* pushTemp() {
        return tempPool_.push();
    }

    StrDesc* pushTempCopy(const StrDesc& source) {
        StrDesc* temp = tempPool_.push();
        if (!temp) return nullptr;
        
        // Allocate a new copy of the string data
        if (!heap_.allocCopy(source.ptr, source.len, *temp)) {
            // Failed to allocate - remove the just-added descriptor
            tempPool_.pop();
            return nullptr;
        }
        
        return temp;
    }

    void clearTemp() {
        tempPool_.clear();
    }

    // Manual garbage collection
    std::size_t collectGarbage() {
        return heap_.collectGarbage();
    }

    // String protection during complex operations
    void protectString(StrDesc* desc) {
        heap_.protectString(desc);
    }

    void clearProtected() {
        heap_.clearProtected();
    }

    // Memory statistics and diagnostics
    std::size_t getFreeBytes() const { return heap_.freeBytes(); }
    std::size_t getUsedBytes() const { return heap_.usedBytes(); }
    std::size_t getTotalBytes() const { return heap_.totalBytes(); }
    double getFragmentation() const { return heap_.fragmentation(); }
    
    const StringHeap::Statistics& getStatistics() const {
        return heap_.getStatistics();
    }

    // Configuration access
    const Config& getConfig() const { return config_; }
    
    void setGCPolicy(StringHeap::GCPolicy policy) {
        heap_.setGCPolicy(policy);
    }
    
    void setGCThreshold(double threshold) {
        heap_.setGCThreshold(threshold);
    }

    // Reset to empty state (useful for NEW command)
    void reset() {
        tempPool_.clear();
        heap_.reset();
    }

    // Validate internal state (for debugging)
    bool validate() const {
        return heap_.validateIntegrity();
    }

private:
    Config config_;
    std::unique_ptr<uint8_t[]> heapBuffer_;
    StringHeap heap_;
    TempStrPool tempPool_;
};

/**
 * RAII helper for temporary string operations.
 * Automatically clears the temporary pool when destroyed.
 */
class TempStringScope {
public:
    explicit TempStringScope(StringManager* manager) : manager_(manager) {}
    
    ~TempStringScope() {
        if (manager_) manager_->clearTemp();
    }
    
    StrDesc* push() {
        return manager_ ? manager_->pushTemp() : nullptr;
    }
    
    StrDesc* pushCopy(const StrDesc& source) {
        return manager_ ? manager_->pushTempCopy(source) : nullptr;
    }

private:
    StringManager* manager_;
};

} // namespace gwbasic
