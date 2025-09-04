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
    // Array* array{nullptr}; // reserved for future integration
};

class VariableTable {
public:
    explicit VariableTable(DefaultTypeTable* deftbl) : deftbl_(deftbl) {}

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

    // For GC: enumerate all string roots currently stored in variables.
    void collectStringRoots(std::vector<StrDesc*>& out) {
        out.clear();
        out.reserve(table_.size());
        for (auto& kv : table_) {
            VarSlot& v = kv.second;
            if (!v.isArray && v.scalar.type == ScalarType::String) {
                out.push_back(&v.scalar.s);
            }
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
};

} // namespace gwbasic
