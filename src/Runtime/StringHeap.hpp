// A string heap with automatic garbage collection for GW-BASIC string management.
#pragma once

#include <cstdint>
#include <vector>
#include <functional>
#include <algorithm>
#include <cstring>
#include <memory>
#include "StringTypes.hpp"

namespace gwbasic {

/**
 * String heap with automatic mark-compact garbage collection.
 * 
 * This implementation manages string storage for GW-BASIC using a downward-growing
 * heap with automatic garbage collection. The heap triggers GC when allocation
 * fails and automatically compacts live strings to maximize available space.
 * 
 * Key features:
 * - Automatic garbage collection when memory is fragmented
 * - Mark-compact collection preserving string order
 * - Root set management through provider interface
 * - Configurable GC thresholds and policies
 * - Statistics tracking for memory usage analysis
 */
class StringHeap {
public:
    // GC trigger policies
    enum class GCPolicy {
        OnDemand,      // Only GC when allocation fails
        Aggressive,    // GC when free space drops below threshold
        Conservative   // GC when fragmentation ratio exceeds threshold
    };

    // Memory and GC statistics
    struct Statistics {
        std::size_t totalAllocations{0};
        std::size_t totalDeallocations{0};
        std::size_t gcCycles{0};
        std::size_t bytesReclaimed{0};
        std::size_t maxUsed{0};
        std::size_t currentUsed{0};
        double averageFragmentation{0.0};
    };

    // Create a heap over an externally owned buffer [buffer, buffer+size).
    StringHeap(uint8_t* buffer, std::size_t size, GCPolicy policy = GCPolicy::OnDemand)
        : base_(buffer), end_(buffer + size), top_(end_), 
          totalSize_(size), policy_(policy) {
        stats_.currentUsed = 0;
    }

    // Reset to empty (clears all allocations).
    void reset() { 
        top_ = end_; 
        stats_.currentUsed = 0;
        rootProviders_.clear();
    }

    // Register a root provider for garbage collection
    void addRootProvider(StringRootProvider* provider) {
        if (provider && std::find(rootProviders_.begin(), rootProviders_.end(), provider) == rootProviders_.end()) {
            rootProviders_.push_back(provider);
        }
    }

    // Unregister a root provider
    void removeRootProvider(StringRootProvider* provider) {
        auto it = std::find(rootProviders_.begin(), rootProviders_.end(), provider);
        if (it != rootProviders_.end()) {
            rootProviders_.erase(it);
        }
    }

    // Allocate 'len' bytes (0..255). Returns true and sets out.ptr on success.
    // Automatically triggers GC if allocation fails and retries once.
    bool alloc(uint16_t len, StrDesc& out) {
        if (len == 0) { 
            out = {}; 
            return true; 
        }
        if (len > 255) return false;

        // Try allocation first
        if (tryAlloc(len, out)) {
            stats_.totalAllocations++;
            stats_.currentUsed += len;
            stats_.maxUsed = std::max(stats_.maxUsed, stats_.currentUsed);
            
            // Check if we should trigger preventive GC
            if (shouldTriggerGC()) {
                collectGarbage();
            }
            return true;
        }

        // Allocation failed - try GC and retry once
        if (collectGarbage() > 0) {
            if (tryAlloc(len, out)) {
                stats_.totalAllocations++;
                stats_.currentUsed += len;
                stats_.maxUsed = std::max(stats_.maxUsed, stats_.currentUsed);
                return true;
            }
        }

        return false; // Out of memory even after GC
    }

    // Copy data into a newly allocated buffer; on failure, returns false without side effects.
    bool allocCopy(const uint8_t* src, uint16_t len, StrDesc& out) {
        if (!alloc(len, out)) return false;
        if (len && src) std::copy(src, src + len, out.ptr);
        return true;
    }

    // Copy a C-string into a newly allocated buffer
    bool allocCopy(const char* src, StrDesc& out) {
        if (!src) {
            out = {};
            return true;
        }
        std::size_t len = std::strlen(src);
        if (len > 255) return false;
        return allocCopy(reinterpret_cast<const uint8_t*>(src), static_cast<uint16_t>(len), out);
    }

    // Manual garbage collection - returns bytes reclaimed
    std::size_t collectGarbage() {
        std::size_t oldUsed = usedBytes();
        
        // Collect all roots from registered providers
        std::vector<StrDesc*> allRoots;
        for (StringRootProvider* provider : rootProviders_) {
            if (provider) {
                provider->collectStringRoots(allRoots);
            }
        }

        // Perform compaction
        compact(allRoots);
        
        std::size_t newUsed = usedBytes();
        std::size_t reclaimed = (oldUsed > newUsed) ? (oldUsed - newUsed) : 0;
        
        stats_.gcCycles++;
        stats_.bytesReclaimed += reclaimed;
        stats_.currentUsed = newUsed;
        
        return reclaimed;
    }

    // Compaction GC: caller supplies root descriptors to relocate.
    // The function must provide a complete root set: all live StrDesc*.
    // Compacts deterministically in the order of roots provided.
    void compact(const std::vector<StrDesc*>& roots) {
        // Include protected strings in the root set
        std::vector<StrDesc*> allRoots = roots;
        for (StrDesc* protectedStr : protectedStrings_) {
            if (protectedStr && protectedStr->len > 0) {
                allRoots.push_back(protectedStr);
            }
        }
        
        // Sort roots by current address (higher addresses first)
        // This ensures we move strings in the right order for downward-growing heap
        std::sort(allRoots.begin(), allRoots.end(), 
                  [](const StrDesc* a, const StrDesc* b) {
                      return a->ptr > b->ptr;
                  });
        
        uint8_t* newTop = end_;
        for (StrDesc* r : allRoots) {
            if (!r || r->len == 0) continue;
            uint16_t len = r->len;
            newTop -= len;
            if (r->ptr != newTop) {
                // Use memmove to be safe on potential overlaps.
                std::memmove(newTop, r->ptr, len);
                r->ptr = newTop;
            }
        }
        top_ = newTop;
    }

    // Force a specific string to be live (add to temporary root set)
    // Useful for protecting temporary strings during complex operations
    void protectString(StrDesc* desc) {
        if (desc && desc->len > 0) {
            protectedStrings_.push_back(desc);
        }
    }

    // Clear all protected strings
    void clearProtected() {
        protectedStrings_.clear();
    }

    // Memory status queries
    std::size_t freeBytes() const { return static_cast<std::size_t>(top_ - base_); }
    std::size_t usedBytes() const { return static_cast<std::size_t>(end_ - top_); }
    std::size_t totalBytes() const { return totalSize_; }
    
    // Get actual free space available for allocation
    std::size_t getFreeSpace() const {
        return static_cast<std::size_t>(top_ - base_);
    }
    
    double fragmentation() const {
        // Simple fragmentation metric: ratio of largest possible allocation to total free space
        return (freeBytes() > 0) ? 1.0 - (static_cast<double>(freeBytes()) / totalSize_) : 0.0;
    }

    // Get current statistics
    const Statistics& getStatistics() const { return stats_; }

    // Configuration
    void setGCPolicy(GCPolicy policy) { policy_ = policy; }
    GCPolicy getGCPolicy() const { return policy_; }
    
    void setGCThreshold(double threshold) { gcThreshold_ = threshold; }
    double getGCThreshold() const { return gcThreshold_; }

    // Debugging and diagnostics
    uint8_t* base() const { return base_; }
    uint8_t* end() const { return end_; }
    uint8_t* top() const { return top_; }

    // Validate heap integrity (for debugging)
    bool validateIntegrity() const {
        return base_ <= top_ && top_ <= end_ && 
               (end_ - base_) == static_cast<std::ptrdiff_t>(totalSize_);
    }

private:
    // Try to allocate without triggering GC
    bool tryAlloc(uint16_t len, StrDesc& out) {
        uint8_t* newTop = top_ - len;
        
        // Check bounds: newTop must be >= base_ for valid allocation
        if (newTop < base_ || newTop > top_) {
            return false;
        }
        
        out.len = len;
        out.ptr = newTop;
        top_ = newTop;
        return true;
    }

    // Check if GC should be triggered based on policy
    bool shouldTriggerGC() const {
        switch (policy_) {
            case GCPolicy::OnDemand:
                return false; // Only GC on allocation failure
            case GCPolicy::Aggressive:
                return freeBytes() < (totalSize_ * gcThreshold_);
            case GCPolicy::Conservative:
                return fragmentation() > gcThreshold_;
            default:
                return false;
        }
    }

    uint8_t* base_;       // low address (grows upward for program/arrays outside of this class)
    uint8_t* end_;        // one past the end of the buffer
    uint8_t* top_;        // current top of the string heap (grows downward)
    std::size_t totalSize_;
    
    // GC configuration
    GCPolicy policy_;
    double gcThreshold_{0.2}; // 20% free space threshold for aggressive, 20% fragmentation for conservative
    
    // Root management
    std::vector<StringRootProvider*> rootProviders_;
    std::vector<StrDesc*> protectedStrings_; // Temporary protection during operations
    
    // Statistics
    Statistics stats_;
};

/**
 * RAII helper for protecting temporary strings during complex operations.
 * Automatically clears protection when destroyed.
 */
class StringProtector {
public:
    StringProtector(StringHeap* heap) : heap_(heap) {}
    
    ~StringProtector() {
        if (heap_) heap_->clearProtected();
    }
    
    void protect(StrDesc* desc) {
        if (heap_) heap_->protectString(desc);
    }
    
private:
    StringHeap* heap_;
};

// Helpers for GW-BASIC LHS string assignment semantics (in-place overwrite without changing length).
inline void overwrite_left(StrDesc& target, const StrDesc& src, uint16_t n) {
    if (target.len == 0 || n == 0) return;
    uint16_t count = std::min<uint16_t>(n, target.len);
    uint16_t copy = std::min<uint16_t>(count, src.len);
    // Left overwrite: fill with spaces, then copy src truncated to count.
    std::fill(target.ptr, target.ptr + count, ' ');
    if (copy) std::copy(src.ptr, src.ptr + copy, target.ptr);
}

inline void overwrite_right(StrDesc& target, const StrDesc& src, uint16_t n) {
    if (target.len == 0 || n == 0) return;
    uint16_t count = std::min<uint16_t>(n, target.len);
    uint16_t copy = std::min<uint16_t>(count, src.len);
    // Right overwrite: fill region with spaces, then right-align the copy.
    uint8_t* regionStart = target.ptr + (target.len - count);
    std::fill(regionStart, regionStart + count, ' ');
    if (copy) std::copy(src.ptr + (src.len - copy), src.ptr + src.len, regionStart + (count - copy));
}

inline void overwrite_mid(StrDesc& target, const StrDesc& src, uint16_t start1based, int optCount /* -1 => remainder */) {
    if (target.len == 0) return;
    if (start1based < 1) start1based = 1; // GW treats <1 as 1 for LHS forms
    uint16_t start0 = static_cast<uint16_t>(start1based - 1);
    if (start0 >= target.len) return; // no-op
    uint16_t remain = target.len - start0;
    uint16_t count = (optCount < 0) ? remain : std::min<uint16_t>(remain, static_cast<uint16_t>(optCount));
    if (count == 0) return;
    uint16_t copy = std::min<uint16_t>(count, src.len);
    if (copy) std::copy(src.ptr, src.ptr + copy, target.ptr + start0);
}

} // namespace gwbasic
