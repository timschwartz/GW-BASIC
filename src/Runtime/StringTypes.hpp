// String descriptor and temp pool used by the runtime string system.
#pragma once

#include <cstdint>
#include <vector>

namespace gwbasic {

struct StrDesc {
    uint16_t len{0};
    uint8_t* ptr{nullptr};
};

class TempStrPool {
public:
    explicit TempStrPool(std::size_t capacity = 32) : capacity_(capacity) {}
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
    std::size_t size() const { return items_.size(); }
    std::size_t capacity() const { return capacity_; }

private:
    std::vector<StrDesc> items_;
    std::size_t capacity_;
};

} // namespace gwbasic
