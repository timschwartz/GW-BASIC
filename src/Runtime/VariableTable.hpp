// Variable table with DEFTBL-driven default typing and suffix handling.
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <array>
#include <algorithm>
#include <cctype>
#include "Value.hpp"
#include "StringHeap.hpp"
#include "ArrayManager.hpp"
#include <fstream>

namespace gwbasic {

// Default type table like DEFTBL: for letters A..Z determine default ScalarType.
class DefaultTypeTable {
public:
    DefaultTypeTable() { reset(); }

    void reset() {
        types_.fill(ScalarType::Single); // GW default is SINGLE unless overridden
    }

    // Define range (inclusive) for a letter set, e.g., DEFINT A-C
    void setRange(char from, char to, ScalarType t) {
        from = static_cast<char>(std::toupper(static_cast<unsigned char>(from)));
        to   = static_cast<char>(std::toupper(static_cast<unsigned char>(to)));
        if (from > to) std::swap(from, to);
        for (char c = from; c <= to; ++c) {
            if (c >= 'A' && c <= 'Z') types_[c - 'A'] = t;
        }
    }

    ScalarType getDefaultFor(char leadingLetter) const {
        char c = static_cast<char>(std::toupper(static_cast<unsigned char>(leadingLetter)));
        if (c < 'A' || c > 'Z') return ScalarType::Single;
        return types_[c - 'A'];
    }

private:
    std::array<ScalarType, 26> types_{};
};

// Normalized symbol key: BASIC is case-insensitive; keep original for listing but key on uppercase name + suffix.
struct SymbolKey {
    std::string name;  // uppercase name without trailing spaces
    char suffix{'\0'}; // one of % ! # $

    bool operator==(const SymbolKey& o) const noexcept {
        return suffix == o.suffix && name == o.name;
    }
};

struct SymbolKeyHash {
    std::size_t operator()(const SymbolKey& k) const noexcept {
        std::hash<std::string> h;
        return (h(k.name) * 131) ^ static_cast<unsigned>(k.suffix);
    }
};

// A variable slot holds a scalar Value or (later) a pointer to an Array.
struct VarSlot {
    bool isArray{false};
    Value scalar{}; // valid when !isArray
    std::string arrayName{}; // valid when isArray - name for lookup in ArrayManager
};

class VariableTable : public StringRootProvider {
public:
    explicit VariableTable(DefaultTypeTable* deftbl, StringHeap* stringHeap = nullptr, ArrayManager* arrayManager = nullptr) 
        : deftbl_(deftbl), stringHeap_(stringHeap), arrayManager_(arrayManager) {
        if (stringHeap_) {
            stringHeap_->addRootProvider(this);
        }
    }

    ~VariableTable() {
        if (stringHeap_) {
            stringHeap_->removeRootProvider(this);
        }
    }

    // Set or change the associated string heap
    void setStringHeap(StringHeap* heap) {
        if (stringHeap_) {
            stringHeap_->removeRootProvider(this);
        }
        stringHeap_ = heap;
        if (stringHeap_) {
            stringHeap_->addRootProvider(this);
        }
    }

    // Set or change the associated array manager
    void setArrayManager(ArrayManager* manager) {
        arrayManager_ = manager;
    }

    // Resolve or create a scalar variable by name (may include suffix). Empty suffix => infer from DEFTBL.
    VarSlot& getOrCreate(const std::string& rawName) {
        SymbolKey key = normalize(rawName);
        auto it = table_.find(key);
        if (it != table_.end()) return it->second;
        VarSlot slot{};
        if (!slot.isArray) {
            ScalarType t = key.suffix ? typeFromSuffix(key.suffix) : deftbl_->getDefaultFor(key.name.empty() ? 'A' : key.name[0]);
            slot.scalar.type = (t == ScalarType::String) ? ScalarType::String : t;
        }
        auto [ins, _] = table_.emplace(std::move(key), slot);
        return ins->second;
    }

    // Try to find an existing variable; returns nullptr if not found.
    VarSlot* tryGet(const std::string& rawName) {
        SymbolKey key = normalize(rawName);
        auto it = table_.find(key);
        if (it == table_.end()) return nullptr;
        return &it->second;
    }

    // Create a string variable with automatic heap allocation
    bool createString(const std::string& varName, const char* value) {
        if (!stringHeap_) return false;
        
        auto& slot = getOrCreate(varName);
        if (slot.scalar.type != ScalarType::String) return false;
        
        StrDesc desc;
        if (!stringHeap_->allocCopy(value, desc)) return false;
        
        slot.scalar.s = desc;
        return true;
    }

    // Create a string variable with automatic heap allocation (from uint8_t*)
    bool createString(const std::string& varName, const uint8_t* value, uint16_t len) {
        if (!stringHeap_) return false;
        
        auto& slot = getOrCreate(varName);
        if (slot.scalar.type != ScalarType::String) return false;
        
        StrDesc desc;
        if (!stringHeap_->allocCopy(value, len, desc)) return false;
        
        slot.scalar.s = desc;
        return true;
    }

    // Assign a string value that's already allocated in the heap
    void assignString(const std::string& varName, const StrDesc& desc) {
        auto& slot = getOrCreate(varName);
        if (slot.scalar.type == ScalarType::String) {
            slot.scalar.s = desc;
        }
    }

    // Create an array variable
    bool createArray(const std::string& rawName, ScalarType type, const std::vector<int16_t>& dimensions) {
        if (!arrayManager_) return false;
        
        SymbolKey key = normalize(rawName);
        
        // Check if variable already exists
        auto it = table_.find(key);
        if (it != table_.end()) {
            return false; // Variable already exists
        }
        
        // Create array in ArrayManager
        if (!arrayManager_->createArray(rawName, type, dimensions)) {
            return false; // Failed to create array
        }
        
        // Create array slot in variable table
        VarSlot slot{};
        slot.isArray = true;
        slot.arrayName = rawName;
        
        table_[key] = slot;
        return true;
    }

    // Check if a variable is an array
    bool isArray(const std::string& rawName) const {
        SymbolKey key = normalize(rawName);
        auto it = table_.find(key);
        if (it == table_.end()) return false;
        return it->second.isArray;
    }

    // Get array element
    bool getArrayElement(const std::string& rawName, const std::vector<int32_t>& indices, Value& out) const {
        if (!arrayManager_) return false;
        
        SymbolKey key = normalize(rawName);
        auto it = table_.find(key);
        if (it == table_.end() || !it->second.isArray) {
            return false; // Not an array variable
        }
        // Debug: log get request
        try {
            std::ofstream ofs("/tmp/gwbasic_debug.log", std::ios::app);
            if (ofs) {
                ofs << "VariableTable:getArrayElement raw='" << rawName << "' norm='" << key.name << key.suffix << "' indices=";
                for (size_t i = 0; i < indices.size(); ++i) ofs << (i?",":"[") << indices[i];
                ofs << "] nameForAM='" << it->second.arrayName << "'\n";
            }
        } catch (...) { /* ignore */ }
        bool ok = arrayManager_->getElement(it->second.arrayName, indices, out);
        try {
            std::ofstream ofs("/tmp/gwbasic_debug.log", std::ios::app);
            if (ofs) {
                ofs << "VariableTable:getArrayElement -> ok=" << ok << " out.type=" << static_cast<int>(out.type);
                if (ok && out.type == ScalarType::Int16) ofs << " i16=" << out.i;
                ofs << "\n";
            }
        } catch (...) { /* ignore */ }
        return ok;
    }

    // Set array element
    bool setArrayElement(const std::string& rawName, const std::vector<int32_t>& indices, const Value& value) {
        if (!arrayManager_) return false;
        
        SymbolKey key = normalize(rawName);
        auto it = table_.find(key);
        if (it == table_.end() || !it->second.isArray) {
            return false; // Not an array variable
        }
        // Debug: log set request
        try {
            std::ofstream ofs("/tmp/gwbasic_debug.log", std::ios::app);
            if (ofs) {
                ofs << "VariableTable:setArrayElement raw='" << rawName << "' norm='" << key.name << key.suffix << "' indices=";
                for (size_t i = 0; i < indices.size(); ++i) ofs << (i?",":"[") << indices[i];
                ofs << "] nameForAM='" << it->second.arrayName << "' val.type=" << static_cast<int>(value.type);
                if (value.type == ScalarType::Int16) ofs << " i16=" << value.i;
                ofs << "\n";
            }
        } catch (...) { /* ignore */ }
        bool ok = arrayManager_->setElement(it->second.arrayName, indices, value);
        try {
            std::ofstream ofs("/tmp/gwbasic_debug.log", std::ios::app);
            if (ofs) {
                ofs << "VariableTable:setArrayElement -> ok=" << ok << "\n";
            }
        } catch (...) { /* ignore */ }
        return ok;
    }

    // Clear all variables (useful for NEW command)
    void clear() {
        table_.clear();
    }

    // Get the number of variables
    std::size_t size() const {
        return table_.size();
    }

    // StringRootProvider implementation
    void collectStringRoots(std::vector<StrDesc*>& roots) override {
        for (auto& kv : table_) {
            VarSlot& v = kv.second;
            if (!v.isArray && v.scalar.type == ScalarType::String) {
                roots.push_back(&v.scalar.s);
            }
            // String array elements are handled by ArrayManager's StringRootProvider implementation
        }
    }

private:
    static SymbolKey normalize(const std::string& raw) {
        SymbolKey key{};
        key.suffix = '\0';
        // Trim trailing whitespace first (BASIC ignores spaces around identifiers)
        std::string tmp = raw;
        while (!tmp.empty() && std::isspace(static_cast<unsigned char>(tmp.back()))) tmp.pop_back();
        if (!tmp.empty()) {
            char last = tmp.back();
            if (last == '%' || last == '!' || last == '#' || last == '$') {
                key.suffix = last;
                key.name = tmp.substr(0, tmp.size() - 1);
            } else {
                key.name = tmp;
            }
        }
        // Uppercase and build significant key: first two alnum characters are significant in GW-BASIC.
        std::string upper;
        upper.reserve(key.name.size());
    for (char c : key.name) upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        // Extract first two alnum characters as the significant name.
        std::string significant;
        for (char c : upper) {
            if (std::isalnum(static_cast<unsigned char>(c))) {
                significant.push_back(c);
                if (significant.size() == 2) break;
            }
        }
        key.name = significant;
        return key;
    }

    std::unordered_map<SymbolKey, VarSlot, SymbolKeyHash> table_;
    DefaultTypeTable* deftbl_;
    StringHeap* stringHeap_;
    ArrayManager* arrayManager_;
};

} // namespace gwbasic
