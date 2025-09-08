#include <catch2/catch_all.hpp>
#include "../src/Runtime/StringManager.hpp"
#include <iostream>

using namespace gwbasic;

TEST_CASE("Debug exact reproduction") {
    StringManager manager;
    
    SECTION("Exact copy of failing test") {
        {
            TempStringScope scope(&manager);
            
            StrDesc source;
            REQUIRE(manager.createString("Test String", source));
            
            // Push copies to temp pool
            StrDesc* temp1 = scope.pushCopy(source);
            StrDesc* temp2 = scope.push();
            
            REQUIRE(temp1 != nullptr);
            REQUIRE(temp2 != nullptr);
            
            std::cout << "temp1: len=" << temp1->len << " ptr=" << (void*)temp1->ptr << std::endl;
            std::cout << "temp2: len=" << temp2->len << " ptr=" << (void*)temp2->ptr << std::endl;
            
            REQUIRE(temp1->len == 11);
            REQUIRE(manager.toString(*temp1) == "Test String");
            
        } // Scope destructor clears temp pool
        
        // Temp pool should be cleared now
        auto* tempPool = manager.getTempPool();
        REQUIRE(tempPool->size() == 0);
    }
}
