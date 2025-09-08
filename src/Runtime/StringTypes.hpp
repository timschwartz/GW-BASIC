// String descriptor and temp pool used by the runtime string system.
#pragma once

#include <cstdint>
#include <vector>

namespace gwbasic {

struct StrDesc {
    uint16_t len{0};
    uint8_t* ptr{nullptr};
};

// Forward declaration - full definition in StringHeap.hpp
class StringRootProvider {
public:
    virtual ~StringRootProvider() = default;
    virtual void collectStringRoots(std::vector<StrDesc*>& roots) = 0;
};

class TempStrPool : public StringRootProvider {
public:
    explicit TempStrPool(std::size_t capacity = 32) : capacity_(capacity) {
        items_.reserve(capacity); // Pre-reserve to avoid reallocations
    }
    
    StrDesc* push() {
        if (items_.size() >= capacity_) return nullptr;
        items_.emplace_back();
        return &items_.back();
    }
    
    StrDesc* pushCopy(const StrDesc& d) {
        if (items_.size() >= capacity_) return nullptr;
        items_.push_back(d);
        return &items_.back();
    }
    
    std::vector<StrDesc*> roots() {
        std::vector<StrDesc*> out;
        out.reserve(items_.size());
        for (auto& it : items_) out.push_back(&it);
        return out;
    }
    
    void clear() { items_.clear(); }
    
    void pop() { 
        if (!items_.empty()) items_.pop_back(); 
    }
    
    std::size_t size() const { return items_.size(); }
    std::size_t capacity() const { return capacity_; }

    // StringRootProvider implementation
    void collectStringRoots(std::vector<StrDesc*>& roots) override {
        for (auto& item : items_) {
            if (item.len > 0) {
                roots.push_back(&item);
            }
        }
    }

private:
    std::vector<StrDesc> items_;
    std::size_t capacity_;
};

} // namespace gwbasic
