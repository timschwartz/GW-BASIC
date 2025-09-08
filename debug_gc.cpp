#include "src/Runtime/StringHeap.hpp"
#include "src/Runtime/VariableTable.hpp"
#include <iostream>
#include <cstring>

using namespace gwbasic;

int main() {
    uint8_t buffer[256];
    StringHeap heap(buffer, sizeof(buffer));
    DefaultTypeTable defTbl;
    VariableTable varTable(&defTbl, &heap);
    
    // Create string variables
    std::cout << "Creating strings...\n";
    bool ok1 = varTable.createString("S1$", "Hello");
    bool ok2 = varTable.createString("S2$", "World");
    std::cout << "Created: " << ok1 << ", " << ok2 << "\n";
    
    // Check contents before GC
    auto* s1 = varTable.tryGet("S1$");
    auto* s2 = varTable.tryGet("S2$");
    
    std::cout << "Before GC:\n";
    std::cout << "S1$ len=" << s1->scalar.s.len << " content=";
    for (int i = 0; i < s1->scalar.s.len; i++) {
        std::cout << s1->scalar.s.ptr[i];
    }
    std::cout << "\n";
    
    std::cout << "S2$ len=" << s2->scalar.s.len << " content=";
    for (int i = 0; i < s2->scalar.s.len; i++) {
        std::cout << s2->scalar.s.ptr[i];
    }
    std::cout << "\n";
    
    // Trigger GC
    std::cout << "Running GC...\n";
    heap.collectGarbage();
    
    // Check contents after GC
    std::cout << "After GC:\n";
    std::cout << "S1$ len=" << s1->scalar.s.len << " content=";
    for (int i = 0; i < s1->scalar.s.len; i++) {
        std::cout << s1->scalar.s.ptr[i];
    }
    std::cout << "\n";
    
    std::cout << "S2$ len=" << s2->scalar.s.len << " content=";
    for (int i = 0; i < s2->scalar.s.len; i++) {
        std::cout << s2->scalar.s.ptr[i];
    }
    std::cout << "\n";
    
    return 0;
}
