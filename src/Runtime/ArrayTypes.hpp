// Array metadata and indexing helpers for GW-BASIC semantics.
#pragma once

#include <cstdint>
#include <vector>
#include <stdexcept>
#include "StringTypes.hpp"

namespace gwbasic {

enum class ValueType : uint8_t { Int16, Single, Double, String };

struct Dim {
    int16_t lb{0};   // inclusive lower bound
    int16_t ub{0};   // inclusive upper bound
    uint32_t stride{0}; // elements to skip when incrementing this index
};

struct Array {
    ValueType vt{ValueType::Single};
    uint8_t rank{1};
    uint32_t elemSize{4};
    uint32_t count{0};
    std::vector<Dim> dims; // size == rank
    uint8_t* data{nullptr}; // contiguous block; for String arrays this holds StrDesc entries

    // Compute flat index from subscripts (1 per dimension), validating bounds.
    uint32_t flatIndex(const std::vector<int32_t>& subs) const {
        if (subs.size() != rank) throw std::out_of_range("rank mismatch");
        uint64_t idx = 0;
        for (uint8_t k = 0; k < rank; ++k) {
            const auto& d = dims[k];
            int32_t s = subs[k];
            if (s < d.lb || s > d.ub) throw std::out_of_range("subscript");
            idx += static_cast<uint64_t>(s - d.lb) * d.stride;
        }
        if (idx >= count) throw std::out_of_range("index");
        return static_cast<uint32_t>(idx);
    }

    uint8_t* elemPtr(const std::vector<int32_t>& subs) const {
        uint32_t idx = flatIndex(subs);
        return data + static_cast<uint64_t>(idx) * elemSize;
    }
};

// Build strides (rightmost index varies fastest).
inline void finalizeStrides(Array& a) {
    if (a.dims.size() != a.rank) a.dims.resize(a.rank);
    uint64_t stride = 1;
    for (int i = static_cast<int>(a.rank) - 1; i >= 0; --i) {
        a.dims[i].stride = static_cast<uint32_t>(stride);
        const uint32_t extent = static_cast<uint32_t>(a.dims[i].ub - a.dims[i].lb + 1);
        stride *= extent;
    }
    a.count = static_cast<uint32_t>(stride);
}

// For string arrays, each element is a StrDesc value inside the contiguous block.
inline StrDesc* stringElem(Array& a, const std::vector<int32_t>& subs) {
    if (a.vt != ValueType::String) throw std::logic_error("not string array");
    uint8_t* p = a.elemPtr(subs);
    return reinterpret_cast<StrDesc*>(p);
}

} // namespace gwbasic
