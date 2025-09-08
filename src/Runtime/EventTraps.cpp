#include "EventTraps.hpp"
#include <algorithm>
#include <cstdlib>

namespace gwbasic {

void EventTrapSystem::setKeyTrap(uint8_t keyIndex, uint16_t handlerLine) {
    auto* trap = findTrap(EventType::Key, keyIndex);
    if (!trap) {
        traps.emplace_back(EventType::Key, keyIndex);
        trap = &traps.back();
    }
    trap->handlerLine = handlerLine;
    trap->enabled = true;
    trap->suspended = false;
    trap->triggered = false;
}

void EventTrapSystem::setErrorTrap(uint16_t handlerLine) {
    auto* trap = findTrap(EventType::Error);
    if (!trap) {
        traps.emplace_back(EventType::Error, 0);
        trap = &traps.back();
    }
    trap->handlerLine = handlerLine;
    trap->enabled = true;
    trap->suspended = false;
    trap->triggered = false;
}

void EventTrapSystem::setTimerTrap(uint16_t handlerLine, uint16_t intervalSeconds) {
    auto* trap = findTrap(EventType::Timer);
    if (!trap) {
        traps.emplace_back(EventType::Timer, 0);
        trap = &traps.back();
    }
    trap->handlerLine = handlerLine;
    trap->enabled = true;
    trap->suspended = false;
    trap->triggered = false;
    
    // Configure timer
    timerState.interval = std::chrono::milliseconds(intervalSeconds * 1000);
    timerState.lastTrigger = std::chrono::steady_clock::now();
    timerState.enabled = true;
}

void EventTrapSystem::setPenTrap(uint16_t handlerLine) {
    auto* trap = findTrap(EventType::Pen);
    if (!trap) {
        traps.emplace_back(EventType::Pen, 0);
        trap = &traps.back();
    }
    trap->handlerLine = handlerLine;
    trap->enabled = true;
    trap->suspended = false;
    trap->triggered = false;
}

void EventTrapSystem::setPlayTrap(uint16_t handlerLine) {
    auto* trap = findTrap(EventType::Play);
    if (!trap) {
        traps.emplace_back(EventType::Play, 0);
        trap = &traps.back();
    }
    trap->handlerLine = handlerLine;
    trap->enabled = true;
    trap->suspended = false;
    trap->triggered = false;
}

void EventTrapSystem::setStrigTrap(uint8_t joystickButton, uint16_t handlerLine) {
    auto* trap = findTrap(EventType::Strig, joystickButton);
    if (!trap) {
        traps.emplace_back(EventType::Strig, joystickButton);
        trap = &traps.back();
    }
    trap->handlerLine = handlerLine;
    trap->enabled = true;
    trap->suspended = false;
    trap->triggered = false;
}

void EventTrapSystem::setComTrap(uint8_t port, uint16_t handlerLine) {
    auto* trap = findTrap(EventType::Com, port);
    if (!trap) {
        traps.emplace_back(EventType::Com, port);
        trap = &traps.back();
    }
    trap->handlerLine = handlerLine;
    trap->enabled = true;
    trap->suspended = false;
    trap->triggered = false;
}

void EventTrapSystem::enableTrap(EventType type, uint8_t subEvent) {
    auto* trap = findTrap(type, subEvent);
    if (trap) {
        trap->enabled = true;
        trap->suspended = false;
    }
}

void EventTrapSystem::disableTrap(EventType type, uint8_t subEvent) {
    auto* trap = findTrap(type, subEvent);
    if (trap) {
        trap->enabled = false;
        trap->suspended = false;
        trap->triggered = false;
    }
}

void EventTrapSystem::suspendTrap(EventType type, uint8_t subEvent) {
    auto* trap = findTrap(type, subEvent);
    if (trap) {
        trap->suspended = true;
    }
}

void EventTrapSystem::enableAllTraps() {
    for (auto& trap : traps) {
        trap.enabled = true;
        trap.suspended = false;
    }
}

void EventTrapSystem::disableAllTraps() {
    for (auto& trap : traps) {
        trap.enabled = false;
        trap.suspended = false;
        trap.triggered = false;
    }
    timerState.enabled = false;
}

void EventTrapSystem::suspendAllTraps() {
    for (auto& trap : traps) {
        trap.suspended = true;
    }
}

void EventTrapSystem::injectKeyEvent(uint8_t scanCode, bool pressed) {
    if (!pressed) return; // Only handle key press events for now
    
    uint8_t keyIndex = mapScanCodeToKeyIndex(scanCode);
    if (keyIndex > 0) {
        KeyEvent event;
        event.scanCode = scanCode;
        event.pressed = pressed;
        event.timestamp = std::chrono::steady_clock::now();
        pendingKeyEvents.push_back(event);
        
        triggerTrap(EventType::Key, keyIndex);
    }
}

void EventTrapSystem::injectTimerTick() {
    if (checkTimerEvent()) {
        triggerTrap(EventType::Timer, 0);
    }
}

void EventTrapSystem::injectError(uint16_t errorCode) {
    triggerTrap(EventType::Error, 0);
}

void EventTrapSystem::injectPenEvent(int16_t x, int16_t y, bool pressed) {
    if (pressed) {
        triggerTrap(EventType::Pen, 0);
    }
}

uint16_t EventTrapSystem::checkForEvents() {
    // Check timer events first
    injectTimerTick();
    
    // Check for any triggered traps that are enabled and not suspended
    for (auto& trap : traps) {
        if (trap.triggered && trap.enabled && !trap.suspended) {
            trap.triggered = false; // Reset trigger
            
            // Call callback if set
            if (trapCallback) {
                trapCallback(trap.handlerLine, trap.type, trap.subEvent);
            }
            
            return trap.handlerLine;
        }
    }
    
    return 0; // No events
}

void EventTrapSystem::clear() {
    traps.clear();
    pendingKeyEvents.clear();
    timerState.enabled = false;
    trapCallback = nullptr;
}

bool EventTrapSystem::isAnyTrapEnabled() const {
    return std::any_of(traps.begin(), traps.end(), 
                      [](const EventTrap& trap) { return trap.enabled && !trap.suspended; });
}

bool EventTrapSystem::hasTriggeredEvents() const {
    return std::any_of(traps.begin(), traps.end(),
                      [](const EventTrap& trap) { return trap.triggered && trap.enabled && !trap.suspended; });
}

EventTrap* EventTrapSystem::findTrap(EventType type, uint8_t subEvent) {
    auto it = std::find_if(traps.begin(), traps.end(),
                          [type, subEvent](const EventTrap& trap) {
                              return trap.type == type && trap.subEvent == subEvent;
                          });
    return (it != traps.end()) ? &(*it) : nullptr;
}

void EventTrapSystem::triggerTrap(EventType type, uint8_t subEvent) {
    auto* trap = findTrap(type, subEvent);
    if (trap && trap->enabled && !trap->suspended) {
        trap->triggered = true;
    }
}

bool EventTrapSystem::checkTimerEvent() {
    if (!timerState.enabled) return false;
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = now - timerState.lastTrigger;
    
    if (elapsed >= timerState.interval) {
        timerState.lastTrigger = now;
        return true;
    }
    
    return false;
}

bool EventTrapSystem::checkKeyEvents() {
    // For now, just check if we have pending events
    // In a full implementation, we'd process the key event queue
    return !pendingKeyEvents.empty();
}

uint8_t EventTrapSystem::mapScanCodeToKeyIndex(uint8_t scanCode) const {
    // Map SDL scan codes to GW-BASIC key indices
    // This is a simplified mapping - a full implementation would be more comprehensive
    
    switch (scanCode) {
        // Function keys
        case 58: return KEY_F1;    // SDL_SCANCODE_F1
        case 59: return KEY_F2;    // SDL_SCANCODE_F2
        case 60: return KEY_F3;    // SDL_SCANCODE_F3
        case 61: return KEY_F4;    // SDL_SCANCODE_F4
        case 62: return KEY_F5;    // SDL_SCANCODE_F5
        case 63: return KEY_F6;    // SDL_SCANCODE_F6
        case 64: return KEY_F7;    // SDL_SCANCODE_F7
        case 65: return KEY_F8;    // SDL_SCANCODE_F8
        case 66: return KEY_F9;    // SDL_SCANCODE_F9
        case 67: return KEY_F10;   // SDL_SCANCODE_F10
        
        // Arrow keys
        case 82: return KEY_CURSOR_UP;     // SDL_SCANCODE_UP
        case 80: return KEY_CURSOR_LEFT;   // SDL_SCANCODE_LEFT
        case 79: return KEY_CURSOR_RIGHT;  // SDL_SCANCODE_RIGHT
        case 81: return KEY_CURSOR_DOWN;   // SDL_SCANCODE_DOWN
        
        // Other special keys
        case 73: return KEY_INSERT;        // SDL_SCANCODE_INSERT
        case 76: return KEY_DELETE;        // SDL_SCANCODE_DELETE
        case 74: return KEY_HOME;          // SDL_SCANCODE_HOME
        case 77: return KEY_END;           // SDL_SCANCODE_END
        case 75: return KEY_PAGE_UP;       // SDL_SCANCODE_PAGEUP
        case 78: return KEY_PAGE_DOWN;     // SDL_SCANCODE_PAGEDOWN
        
        default:
            return 0; // Unmapped key
    }
}

} // namespace gwbasic
