#include "src/Runtime/StringManager.hpp"
#include <iostream>
#include <string>

using namespace gwbasic;

int main() {
    StringManager::Config config;
    config.heapSize = 1024;
    StringManager manager(config);
    
    {
        TempStringScope scope(&manager);
        
        StrDesc source;
        std::cout << "Creating source string...\n";
        bool ok = manager.createString("Test String", source);
        std::cout << "Created: " << ok << " len=" << source.len << " ptr=" << (void*)source.ptr << "\n";
        
        std::cout << "Pushing copy to temp pool...\n";
        StrDesc* temp1 = scope.pushCopy(source);
        
        if (temp1) {
            std::cout << "temp1: len=" << temp1->len << " ptr=" << (void*)temp1->ptr << "\n";
            std::cout << "Content: ";
            for (int i = 0; i < temp1->len && i < 20; i++) {
                std::cout << temp1->ptr[i];
            }
            std::cout << "\n";
        } else {
            std::cout << "temp1 is null!\n";
        }
        
        StrDesc* temp2 = scope.push();
        if (temp2) {
            std::cout << "temp2: len=" << temp2->len << " ptr=" << (void*)temp2->ptr << "\n";
        } else {
            std::cout << "temp2 is null!\n";
        }
    }
    
    return 0;
}
