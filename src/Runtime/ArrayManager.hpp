#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <stdexcept>
#include "ArrayTypes.hpp"
#include "Value.hpp"
#include "StringHeap.hpp"

namespace gwbasic {

class ArrayManager : public StringRootProvider {
public:
    explicit ArrayManager(StringHeap* stringHeap = nullptr);
    ~ArrayManager();
    
    // Set or change the associated string heap
    void setStringHeap(StringHeap* heap);
    
    // Create a new array with specified dimensions
    // dimensions is a vector of upper bounds (lower bound is always 0 for now)
    bool createArray(const std::string& name, ScalarType type, const std::vector<int16_t>& dimensions);
    
    // Check if an array exists
    bool arrayExists(const std::string& name) const;
    
    // Get array element value
    bool getElement(const std::string& name, const std::vector<int32_t>& indices, Value& out) const;
    
    // Set array element value
    bool setElement(const std::string& name, const std::vector<int32_t>& indices, const Value& value);
    
    // Get array information
    bool getArrayInfo(const std::string& name, ValueType& type, uint8_t& rank, std::vector<Dim>& dims) const;
    
    // Clear all arrays (useful for NEW command)
    void clear();
    
    // Get the number of arrays
    std::size_t size() const;
    
    // StringRootProvider implementation for GC integration
    void collectStringRoots(std::vector<StrDesc*>& roots) override;

private:
    std::unordered_map<std::string, std::unique_ptr<Array>> arrays_;
    StringHeap* stringHeap_;
    
    // Normalize array name (uppercase, handle suffix)
    std::string normalizeName(const std::string& name) const;
    
    // Convert ScalarType to ValueType
    ValueType scalarToValueType(ScalarType scalarType) const;
    
    // Get element size for a given value type
    uint32_t getElementSize(ValueType type) const;
    
    // Allocate data block for array
    bool allocateArrayData(Array& array);
    
    // Free data block for array
    void freeArrayData(Array& array);
    
    // Initialize array elements to zero/empty values
    void initializeArrayElements(Array& array);
};

} // namespace gwbasic
