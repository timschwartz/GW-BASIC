// Runtime stacks for FOR/NEXT, GOSUB/RETURN, and ERR/RESUME frames.
#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include "Value.hpp"

namespace gwbasic {

struct ForFrame {
    // Variable name key for control var (normalized); here we store inline Value for string roots if needed.
    std::string varKey;
    Value control; // numeric; GW uses FAC/DFAC in practice but we store snapshot
    Value limit;   // TO limit
    Value step;    // STEP value
    uint32_t textPtr{0}; // pointer/index into program text for loop body restart
};

struct GosubFrame {
    uint32_t returnTextPtr{0};
    uint16_t returnLine{0};
};

struct ErrFrame {
    uint16_t errCode{0};
    uint32_t resumeTextPtr{0};
};

class RuntimeStack {
public:
    void clear() { forStack_.clear(); gosubStack_.clear(); errStack_.clear(); }

    // FOR/NEXT
    void pushFor(const ForFrame& f) { forStack_.push_back(f); }
    bool popFor(ForFrame& out) {
        if (forStack_.empty()) return false;
        out = forStack_.back(); forStack_.pop_back(); return true;
    }
    ForFrame* topFor() { return forStack_.empty() ? nullptr : &forStack_.back(); }

    // GOSUB/RETURN
    void pushGosub(const GosubFrame& f) { gosubStack_.push_back(f); }
    bool popGosub(GosubFrame& out) {
        if (gosubStack_.empty()) return false;
        out = gosubStack_.back(); gosubStack_.pop_back(); return true;
    }

    // ERR/RESUME
    void pushErr(const ErrFrame& e) { errStack_.push_back(e); }
    bool popErr(ErrFrame& out) {
        if (errStack_.empty()) return false;
        out = errStack_.back(); errStack_.pop_back(); return true;
    }

    // Collect string roots that may be held by frames (if any string Values exist in frames).
    void collectStringRoots(std::vector<StrDesc*>& out) {
        for (auto& f : forStack_) {
            if (f.control.type == ScalarType::String) out.push_back(&f.control.s);
            if (f.limit.type == ScalarType::String) out.push_back(&f.limit.s);
            if (f.step.type == ScalarType::String) out.push_back(&f.step.s);
        }
        // gosub and err frames typically hold no strings.
    }

private:
    std::vector<ForFrame> forStack_;
    std::vector<GosubFrame> gosubStack_;
    std::vector<ErrFrame> errStack_;
};

} // namespace gwbasic
