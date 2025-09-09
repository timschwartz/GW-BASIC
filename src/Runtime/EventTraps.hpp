#pragma once

#include <cstdint>
#include <vector>
#include <chrono>
#include <functional>

namespace gwbasic {

/**
 * EventTrapSystem - Manages GW-BASIC event traps
 * 
 * Handles ON KEY, ON ERROR, ON TIMER, ON PEN, etc. statements
 * Provides checking for events and dispatching to trap handlers
 */

enum class EventType {
    Key = 0,       // ON KEY(n) - Function keys F1-F10 + special keys
    Error = 1,     // ON ERROR - Runtime errors  
    Timer = 2,     // ON TIMER - Timer events
    Pen = 3,       // ON PEN - Light pen events
    Play = 4,      // ON PLAY - Music queue events
    Strig = 5,     // ON STRIG - Joystick button events
    Com = 6,       // ON COM - Communications events
    UserDefined = 7 // User-defined trap points
};

struct EventTrap {
    EventType type;
    uint16_t subEvent{0};      // For KEY(n), STRIG(n), COM(n), etc.
    bool enabled{false};       // TRAP ON/OFF state
    bool suspended{false};     // TRAP STOP state  
    uint16_t handlerLine{0};   // GOTO target line number
    bool triggered{false};     // Event has occurred but not yet handled
    
    EventTrap(EventType t = EventType::Key, uint16_t sub = 0) 
        : type(t), subEvent(sub) {}
};

struct KeyEvent {
    uint8_t scanCode;
    bool pressed;
    std::chrono::steady_clock::time_point timestamp;
};

struct TimerEvent {
    std::chrono::steady_clock::time_point lastTrigger;
    std::chrono::milliseconds interval{1000}; // Default 1 second
    bool enabled{false};
};

class EventTrapSystem {
public:
    // Trap configuration
    void setKeyTrap(uint8_t keyIndex, uint16_t handlerLine);
    void setErrorTrap(uint16_t handlerLine);
    void setTimerTrap(uint16_t handlerLine, uint16_t intervalSeconds);
    void setPenTrap(uint16_t handlerLine);
    void setPlayTrap(uint16_t handlerLine);
    void setStrigTrap(uint8_t joystickButton, uint16_t handlerLine);
    void setComTrap(uint8_t port, uint16_t handlerLine);
    
    // Trap control (ON/OFF/STOP)
    void enableTrap(EventType type, uint8_t subEvent = 0);
    void disableTrap(EventType type, uint8_t subEvent = 0);
    void suspendTrap(EventType type, uint8_t subEvent = 0);
    void enableAllTraps();
    void disableAllTraps();
    void suspendAllTraps();
    
    // Event injection (called by main event loop)
    void injectKeyEvent(uint8_t scanCode, bool pressed);
    void injectTimerTick();
    void injectError(uint16_t errorCode);
    void injectPenEvent(int16_t x, int16_t y, bool pressed);
    
    // Event checking (called by interpreter between statements)
    uint16_t checkForEvents();  // Returns line number to jump to, or 0 if none
    
    // State management
    void clear();
    bool isAnyTrapEnabled() const;
    bool hasTriggeredEvents() const;
    bool isKeyTrapEnabled(uint8_t keyIndex) const;  // Check if specific key trap is active
    
    // Callback for interpreter to handle trap jumps
    using TrapCallback = std::function<void(uint16_t lineNumber, EventType type, uint8_t subEvent)>;
    void setTrapCallback(TrapCallback callback) { trapCallback = callback; }

private:
    std::vector<EventTrap> traps;
    TimerEvent timerState;
    std::vector<KeyEvent> pendingKeyEvents;
    TrapCallback trapCallback;
    
    // Internal helpers
    EventTrap* findTrap(EventType type, uint8_t subEvent = 0);
    void triggerTrap(EventType type, uint8_t subEvent = 0);
    bool checkTimerEvent();
    bool checkKeyEvents();
    
    // GW-BASIC key mappings (scancode to key index)
    static constexpr uint8_t KEY_F1 = 1;
    static constexpr uint8_t KEY_F2 = 2;
    static constexpr uint8_t KEY_F3 = 3;
    static constexpr uint8_t KEY_F4 = 4;
    static constexpr uint8_t KEY_F5 = 5;
    static constexpr uint8_t KEY_F6 = 6;
    static constexpr uint8_t KEY_F7 = 7;
    static constexpr uint8_t KEY_F8 = 8;
    static constexpr uint8_t KEY_F9 = 9;
    static constexpr uint8_t KEY_F10 = 10;
    static constexpr uint8_t KEY_CURSOR_UP = 11;
    static constexpr uint8_t KEY_CURSOR_LEFT = 12;
    static constexpr uint8_t KEY_CURSOR_RIGHT = 13;
    static constexpr uint8_t KEY_CURSOR_DOWN = 14;
    static constexpr uint8_t KEY_INSERT = 15;
    static constexpr uint8_t KEY_DELETE = 16;
    static constexpr uint8_t KEY_HOME = 17;
    static constexpr uint8_t KEY_END = 18;
    static constexpr uint8_t KEY_PAGE_UP = 19;
    static constexpr uint8_t KEY_PAGE_DOWN = 20;
    
    uint8_t mapScanCodeToKeyIndex(uint8_t scanCode) const;
};

} // namespace gwbasic
