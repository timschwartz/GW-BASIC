#include "ArrayManager.hpp"
#include <algorithm>
#include <cctype>
#include <cstring>

namespace gwbasic {

ArrayManager::ArrayManager(StringHeap* stringHeap) : stringHeap_(stringHeap) {
    if (stringHeap_) {
        stringHeap_->addRootProvider(this);
    }
}

ArrayManager::~ArrayManager() {
    if (stringHeap_) {
        stringHeap_->removeRootProvider(this);
    }
    clear();
}

void ArrayManager::setStringHeap(StringHeap* heap) {
    if (stringHeap_) {
        stringHeap_->removeRootProvider(this);
    }
    stringHeap_ = heap;
    if (stringHeap_) {
        stringHeap_->addRootProvider(this);
    }
}

std::string ArrayManager::normalizeName(const std::string& name) const {
    std::string normalized;
    normalized.reserve(name.size());
    
    // Extract first two alphanumeric characters and suffix
    std::string alnum;
    char suffix = '\0';
    
    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            if (alnum.size() < 2) {
                alnum.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            }
        } else if (c == '%' || c == '!' || c == '#' || c == '$') {
            suffix = c;
            break;
        }
    }
    
    return alnum + (suffix ? std::string(1, suffix) : std::string());
}

ValueType ArrayManager::scalarToValueType(ScalarType scalarType) const {
    switch (scalarType) {
        case ScalarType::Int16:  return ValueType::Int16;
        case ScalarType::Single: return ValueType::Single;
        case ScalarType::Double: return ValueType::Double;
        case ScalarType::String: return ValueType::String;
    }
    return ValueType::Single; // fallback
}

uint32_t ArrayManager::getElementSize(ValueType type) const {
    switch (type) {
        case ValueType::Int16:  return sizeof(int16_t);
        case ValueType::Single: return sizeof(float);
        case ValueType::Double: return sizeof(double);
        case ValueType::String: return sizeof(StrDesc);
    }
    return sizeof(float); // fallback
}

bool ArrayManager::allocateArrayData(Array& array) {
    if (array.count == 0) return false;
    
    try {
        size_t totalBytes = static_cast<size_t>(array.count) * array.elemSize;
        array.data = new uint8_t[totalBytes];
        return true;
    } catch (const std::bad_alloc&) {
        return false;
    }
}

void ArrayManager::freeArrayData(Array& array) {
    delete[] array.data;
    array.data = nullptr;
}

void ArrayManager::initializeArrayElements(Array& array) {
    if (!array.data) return;
    
    size_t totalBytes = static_cast<size_t>(array.count) * array.elemSize;
    
    if (array.vt == ValueType::String) {
        // Initialize all string descriptors to empty strings
        StrDesc* elements = reinterpret_cast<StrDesc*>(array.data);
        for (uint32_t i = 0; i < array.count; ++i) {
            elements[i] = StrDesc{}; // empty string descriptor
        }
    } else {
        // Zero-initialize numeric arrays
        std::memset(array.data, 0, totalBytes);
    }
}

bool ArrayManager::createArray(const std::string& name, ScalarType type, const std::vector<int16_t>& dimensions) {
    std::string normName = normalizeName(name);
    
    // Check if array already exists
    if (arrays_.find(normName) != arrays_.end()) {
        return false; // Array already exists
    }
    
    // Create new array
    auto array = std::make_unique<Array>();
    array->vt = scalarToValueType(type);
    array->rank = static_cast<uint8_t>(dimensions.size());
    array->elemSize = getElementSize(array->vt);
    
    // Set up dimensions (lower bound = 0, upper bound from parameter)
    array->dims.resize(array->rank);
    for (size_t i = 0; i < dimensions.size(); ++i) {
        array->dims[i].lb = 0;
        array->dims[i].ub = dimensions[i];
    }
    
    // Calculate strides and total count
    finalizeStrides(*array);
    
    // Allocate data
    if (!allocateArrayData(*array)) {
        return false; // Out of memory
    }
    
    // Initialize elements
    initializeArrayElements(*array);
    
    // Store the array
    arrays_[normName] = std::move(array);
    return true;
}

bool ArrayManager::arrayExists(const std::string& name) const {
    std::string normName = normalizeName(name);
    return arrays_.find(normName) != arrays_.end();
}

bool ArrayManager::getElement(const std::string& name, const std::vector<int32_t>& indices, Value& out) const {
    std::string normName = normalizeName(name);
    auto it = arrays_.find(normName);
    if (it == arrays_.end()) {
        return false; // Array doesn't exist
    }
    
    const Array& array = *it->second;
    
    try {
        uint8_t* elemPtr = array.elemPtr(indices);
        
        switch (array.vt) {
            case ValueType::Int16:
                out = Value::makeInt(*reinterpret_cast<const int16_t*>(elemPtr));
                break;
            case ValueType::Single:
                out = Value::makeSingle(*reinterpret_cast<const float*>(elemPtr));
                break;
            case ValueType::Double:
                out = Value::makeDouble(*reinterpret_cast<const double*>(elemPtr));
                break;
            case ValueType::String:
                out = Value::makeString(*reinterpret_cast<const StrDesc*>(elemPtr));
                break;
        }
        return true;
    } catch (const std::out_of_range&) {
        return false; // Index out of bounds
    }
}

bool ArrayManager::setElement(const std::string& name, const std::vector<int32_t>& indices, const Value& value) {
    std::string normName = normalizeName(name);
    auto it = arrays_.find(normName);
    if (it == arrays_.end()) {
        return false; // Array doesn't exist
    }
    
    Array& array = *it->second;
    
    try {
        uint8_t* elemPtr = array.elemPtr(indices);
        
        switch (array.vt) {
            case ValueType::Int16:
                if (value.type != ScalarType::Int16) return false; // Type mismatch
                *reinterpret_cast<int16_t*>(elemPtr) = value.i;
                break;
            case ValueType::Single:
                if (value.type != ScalarType::Single) return false; // Type mismatch
                *reinterpret_cast<float*>(elemPtr) = value.f;
                break;
            case ValueType::Double:
                if (value.type != ScalarType::Double) return false; // Type mismatch
                *reinterpret_cast<double*>(elemPtr) = value.d;
                break;
            case ValueType::String:
                if (value.type != ScalarType::String) return false; // Type mismatch
                *reinterpret_cast<StrDesc*>(elemPtr) = value.s;
                break;
        }
        return true;
    } catch (const std::out_of_range&) {
        return false; // Index out of bounds
    }
}

bool ArrayManager::getArrayInfo(const std::string& name, ValueType& type, uint8_t& rank, std::vector<Dim>& dims) const {
    std::string normName = normalizeName(name);
    auto it = arrays_.find(normName);
    if (it == arrays_.end()) {
        return false; // Array doesn't exist
    }
    
    const Array& array = *it->second;
    type = array.vt;
    rank = array.rank;
    dims = array.dims;
    return true;
}

void ArrayManager::clear() {
    for (auto& kv : arrays_) {
        freeArrayData(*kv.second);
    }
    arrays_.clear();
}

std::size_t ArrayManager::size() const {
    return arrays_.size();
}

void ArrayManager::collectStringRoots(std::vector<StrDesc*>& roots) {
    for (auto& kv : arrays_) {
        Array& array = *kv.second;
        if (array.vt == ValueType::String && array.data) {
            StrDesc* elements = reinterpret_cast<StrDesc*>(array.data);
            for (uint32_t i = 0; i < array.count; ++i) {
                roots.push_back(&elements[i]);
            }
        }
    }
}

} // namespace gwbasic
