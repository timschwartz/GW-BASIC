#include <cassert>
#include <iostream>
#include <chrono>
#include <thread>

#include "../src/Runtime/EventTraps.hpp"

using namespace gwbasic;

void test_basic_event_trap_system() {
    std::cout << "Testing EventTrapSystem..." << std::endl;
    
    EventTrapSystem eventSystem;
    
    // Test 1: Set up key trap
    eventSystem.setKeyTrap(1, 1000); // F1 key goes to line 1000
    assert(eventSystem.isAnyTrapEnabled());
    
    // Test 2: Set up error trap  
    eventSystem.setErrorTrap(2000); // Errors go to line 2000
    
    // Test 3: Set up timer trap
    eventSystem.setTimerTrap(3000, 1); // Timer every 1 second goes to line 3000
    
    // Test 4: Inject key event
    eventSystem.injectKeyEvent(58, true); // SDL_SCANCODE_F1
    assert(eventSystem.hasTriggeredEvents());
    
    // Test 5: Check for events
    uint16_t trapLine = eventSystem.checkForEvents();
    assert(trapLine == 1000); // Should jump to F1 handler
    
    // Test 6: Inject error event
    eventSystem.injectError(11); // Division by zero error
    trapLine = eventSystem.checkForEvents();
    assert(trapLine == 2000); // Should jump to error handler
    
    // Test 7: Timer event (need to wait a bit)
    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    trapLine = eventSystem.checkForEvents();
    assert(trapLine == 3000); // Should jump to timer handler
    
    // Test 8: Disable trap
    eventSystem.disableTrap(EventType::Key, 1);
    eventSystem.injectKeyEvent(58, true); // F1 again
    trapLine = eventSystem.checkForEvents();
    assert(trapLine == 0); // Should not trigger (disabled)
    
    std::cout << "✓ EventTrapSystem tests passed!" << std::endl;
}

int main() {
    try {
        test_basic_event_trap_system();
        std::cout << "All event trap tests passed!" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
