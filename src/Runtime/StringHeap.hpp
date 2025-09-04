// A simple arena for short strings that grows downward with mark-compact GC.
#pragma once

#include <cstdint>
#include <vector>
#include <functional>
#include <algorithm>
#include <cstring>
#include "StringTypes.hpp"

namespace gwbasic {

class StringHeap {
public:
    // Create a heap over an externally owned buffer [buffer, buffer+size).
    StringHeap(uint8_t* buffer, std::size_t size)
        : base_(buffer), end_(buffer + size), top_(end_) {}

    // Reset to empty (clears all allocations).
    void reset() { top_ = end_; }

    // Allocate 'len' bytes (0..255). Returns true and sets out.ptr on success.
    bool alloc(uint16_t len, StrDesc& out) {
        if (len == 0) { out = {}; return true; }
        if (len > 255) return false;
        if (static_cast<std::size_t>(top_ - base_) < len) return false; // not enough total
        uint8_t* newTop = top_ - len;
        if (newTop < base_) return false;
        out.len = len;
        out.ptr = newTop;
        top_ = newTop;
        return true;
    }

    // Copy data into a newly allocated buffer; on failure, returns false without side effects.
    bool allocCopy(const uint8_t* src, uint16_t len, StrDesc& out) {
        if (!alloc(len, out)) return false;
        if (len && src) std::copy(src, src + len, out.ptr);
        return true;
    }

    // Compaction GC: caller supplies root descriptors to relocate.
    // The function must provide a complete root set: all live StrDesc*.
    // Compacts deterministically in the order of roots provided.
    void compact(const std::vector<StrDesc*>& roots) {
        uint8_t* newTop = end_;
        for (StrDesc* r : roots) {
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

    // Amount of free space remaining.
    std::size_t freeBytes() const { return static_cast<std::size_t>(top_ - base_); }

    uint8_t* base() const { return base_; }
    uint8_t* end() const { return end_; }
    uint8_t* top() const { return top_; }

private:
    uint8_t* base_; // low address (grows upward for program/arrays outside of this class)
    uint8_t* end_;  // one past the end of the buffer
    uint8_t* top_;  // current top of the string heap (grows downward)
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
