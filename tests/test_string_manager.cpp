#include <catch2/catch_all.hpp>
#include "../src/Runtime/StringManager.hpp"
#include <string>

using namespace gwbasic;

TEST_CASE("StringManager basic operations") {
    StringManager manager;
    
    SECTION("String creation") {
        StrDesc desc;
        REQUIRE(manager.createString("Hello", desc));
        REQUIRE(desc.len == 5);
        REQUIRE(manager.toString(desc) == "Hello");
        
        REQUIRE(manager.getUsedBytes() == 5);
        REQUIRE(manager.getFreeBytes() == 8192 - 5);
    }
    
    SECTION("String creation from std::string") {
        StrDesc desc;
        std::string test = "C++ String";
        REQUIRE(manager.createString(test, desc));
        REQUIRE(desc.len == 10);
        REQUIRE(manager.toString(desc) == "C++ String");
    }
    
    SECTION("Empty string creation") {
        StrDesc desc;
        REQUIRE(manager.createString("", desc));
        REQUIRE(desc.len == 0);
        REQUIRE(desc.ptr == nullptr);
        REQUIRE(manager.toString(desc) == "");
    }
    
    SECTION("Large string rejection") {
        std::string large(300, 'X'); // Larger than 255 chars
        StrDesc desc;
        REQUIRE_FALSE(manager.createString(large, desc));
    }
}

TEST_CASE("StringManager concatenation") {
    StringManager manager;
    
    SECTION("Basic concatenation") {
        StrDesc left, right, result;
        REQUIRE(manager.createString("Hello", left));
        REQUIRE(manager.createString(" World", right));
        REQUIRE(manager.concatenate(left, right, result));
        
        REQUIRE(result.len == 11);
        REQUIRE(manager.toString(result) == "Hello World");
    }
    
    SECTION("Concatenation with empty strings") {
        StrDesc empty, text, result;
        REQUIRE(manager.createString("", empty));
        REQUIRE(manager.createString("Text", text));
        
        REQUIRE(manager.concatenate(empty, text, result));
        REQUIRE(manager.toString(result) == "Text");
        
        REQUIRE(manager.concatenate(text, empty, result));
        REQUIRE(manager.toString(result) == "Text");
    }
    
    SECTION("Concatenation overflow") {
        StrDesc left, right, result;
        std::string longStr(200, 'A');
        REQUIRE(manager.createString(longStr, left));
        REQUIRE(manager.createString(longStr, right));
        
        // Total would be 400 chars, exceeding 255 limit
        REQUIRE_FALSE(manager.concatenate(left, right, result));
    }
}

TEST_CASE("StringManager substring operations") {
    StringManager manager;
    StrDesc source;
    REQUIRE(manager.createString("HELLO WORLD", source));
    
    SECTION("LEFT$ function") {
        StrDesc result;
        REQUIRE(manager.left(source, 5, result));
        REQUIRE(manager.toString(result) == "HELLO");
        
        // Left with count > length
        REQUIRE(manager.left(source, 20, result));
        REQUIRE(manager.toString(result) == "HELLO WORLD");
        
        // Left with zero count
        REQUIRE(manager.left(source, 0, result));
        REQUIRE(result.len == 0);
    }
    
    SECTION("RIGHT$ function") {
        StrDesc result;
        REQUIRE(manager.right(source, 5, result));
        REQUIRE(manager.toString(result) == "WORLD");
        
        // Right with count > length
        REQUIRE(manager.right(source, 20, result));
        REQUIRE(manager.toString(result) == "HELLO WORLD");
        
        // Right with zero count
        REQUIRE(manager.right(source, 0, result));
        REQUIRE(result.len == 0);
    }
    
    SECTION("MID$ function") {
        StrDesc result;
        
        // MID$(s, start, count)
        REQUIRE(manager.mid(source, 7, 5, result));
        REQUIRE(manager.toString(result) == "WORLD");
        
        // MID$(s, start) - to end
        REQUIRE(manager.mid(source, 7, -1, result));
        REQUIRE(manager.toString(result) == "WORLD");
        
        // MID$ with start beyond string
        REQUIRE(manager.mid(source, 20, 5, result));
        REQUIRE(result.len == 0);
        
        // MID$ with start < 1 (GW-BASIC treats as empty)
        REQUIRE(manager.mid(source, 0, 5, result));
        REQUIRE(result.len == 0);
    }
}

TEST_CASE("StringManager search and comparison") {
    StringManager manager;
    
    SECTION("INSTR function") {
        StrDesc source, search;
        REQUIRE(manager.createString("HELLO WORLD HELLO", source));
        REQUIRE(manager.createString("WORLD", search));
        
        // Find first occurrence
        REQUIRE(manager.instr(source, search) == 7);
        
        // Find from specific position
        REQUIRE(manager.instr(source, search, 7) == 7);
        REQUIRE(manager.instr(source, search, 8) == 0); // Not found after position
        
        // Search for "HELLO"
        StrDesc hello;
        REQUIRE(manager.createString("HELLO", hello));
        REQUIRE(manager.instr(source, hello) == 1);
        REQUIRE(manager.instr(source, hello, 2) == 13);
    }
    
    SECTION("String comparison") {
        StrDesc str1, str2, str3;
        REQUIRE(manager.createString("ABC", str1));
        REQUIRE(manager.createString("ABC", str2));
        REQUIRE(manager.createString("XYZ", str3));
        
        REQUIRE(manager.compare(str1, str2) == 0); // Equal
        REQUIRE(manager.compare(str1, str3) < 0);  // str1 < str3
        REQUIRE(manager.compare(str3, str1) > 0);  // str3 > str1
        
        // Compare different lengths
        StrDesc short_str, long_str;
        REQUIRE(manager.createString("AB", short_str));
        REQUIRE(manager.createString("ABCD", long_str));
        REQUIRE(manager.compare(short_str, long_str) < 0);
    }
}

TEST_CASE("StringManager temporary string pool") {
    StringManager manager;
    
    SECTION("Temporary string operations") {
        {
            TempStringScope scope(&manager);
            
            StrDesc source;
            REQUIRE(manager.createString("Test String", source));
            
            // Push copies to temp pool
            StrDesc* temp1 = scope.pushCopy(source);
            StrDesc* temp2 = scope.push();
            
            REQUIRE(temp1 != nullptr);
            REQUIRE(temp2 != nullptr);
            REQUIRE(temp1->len == 11);
            REQUIRE(manager.toString(*temp1) == "Test String");
            
        } // Scope destructor clears temp pool
        
        // Temp pool should be cleared now
        auto* tempPool = manager.getTempPool();
        REQUIRE(tempPool->size() == 0);
    }
    
    SECTION("Manual temp pool management") {
        StrDesc source;
        REQUIRE(manager.createString("Temporary", source));
        
        StrDesc* temp = manager.pushTempCopy(source);
        REQUIRE(temp != nullptr);
        REQUIRE(manager.getTempPool()->size() == 1);
        
        manager.clearTemp();
        REQUIRE(manager.getTempPool()->size() == 0);
    }
}

TEST_CASE("StringManager garbage collection") {
    // Use small heap to test GC
    StringManager::Config config;
    config.heapSize = 128;
    StringManager manager(config);
    
    SECTION("Automatic memory management") {
        std::vector<StrDesc> strings;
        
        // Create multiple strings
        for (int i = 0; i < 10; ++i) {
            StrDesc desc;
            std::string content = "String" + std::to_string(i);
            REQUIRE(manager.createString(content, desc));
            strings.push_back(desc);
        }
        
        auto usedBefore = manager.getUsedBytes();
        REQUIRE(usedBefore > 0);
        
        // Clear our references (simulate variables going out of scope)
        strings.clear();
        
        // Force garbage collection
        std::size_t reclaimed = manager.collectGarbage();
        REQUIRE(reclaimed == usedBefore); // All strings should be reclaimed
        REQUIRE(manager.getUsedBytes() == 0);
    }
    
    SECTION("String protection during operations") {
        StrDesc protected1, protected2, unprotected;
        REQUIRE(manager.createString("Protected1", protected1));
        REQUIRE(manager.createString("Protected2", protected2));
        REQUIRE(manager.createString("Unprotected", unprotected));
        
        // Protect some strings
        manager.protectString(&protected1);
        manager.protectString(&protected2);
        
        auto usedBefore = manager.getUsedBytes();
        std::size_t reclaimed = manager.collectGarbage();
        
        // Should reclaim only the unprotected string
        REQUIRE(reclaimed == 11); // "Unprotected"
        REQUIRE(manager.getUsedBytes() == usedBefore - 11);
        
        manager.clearProtected();
        reclaimed = manager.collectGarbage();
        REQUIRE(manager.getUsedBytes() == 0); // All reclaimed now
    }
}

TEST_CASE("StringManager configuration and statistics") {
    StringManager::Config config;
    config.heapSize = 256;
    config.gcPolicy = StringHeap::GCPolicy::Aggressive;
    config.gcThreshold = 0.5;
    
    StringManager manager(config);
    
    SECTION("Configuration access") {
        REQUIRE(manager.getConfig().heapSize == 256);
        REQUIRE(manager.getConfig().gcPolicy == StringHeap::GCPolicy::Aggressive);
        REQUIRE(manager.getConfig().gcThreshold == 0.5);
        
        REQUIRE(manager.getTotalBytes() == 256);
    }
    
    SECTION("Statistics tracking") {
        auto stats = manager.getStatistics();
        REQUIRE(stats.totalAllocations == 0);
        
        StrDesc desc;
        REQUIRE(manager.createString("Statistics", desc));
        
        stats = manager.getStatistics();
        REQUIRE(stats.totalAllocations == 1);
        REQUIRE(stats.currentUsed == 10);
        
        manager.collectGarbage();
        stats = manager.getStatistics();
        REQUIRE(stats.gcCycles == 1);
    }
    
    SECTION("Memory usage queries") {
        REQUIRE(manager.getUsedBytes() == 0);
        REQUIRE(manager.getFreeBytes() == 256);
        REQUIRE(manager.getFragmentation() == 0.0);
        
        StrDesc desc;
        REQUIRE(manager.createString("Half", desc));
        REQUIRE(manager.getUsedBytes() == 4);
        REQUIRE(manager.getFreeBytes() == 252);
    }
}

TEST_CASE("StringManager reset and validation") {
    StringManager manager;
    
    SECTION("Reset functionality") {
        // Create some strings
        StrDesc desc1, desc2;
        REQUIRE(manager.createString("Test1", desc1));
        REQUIRE(manager.createString("Test2", desc2));
        
        auto* temp = manager.pushTempCopy(desc1);
        REQUIRE(temp != nullptr);
        
        REQUIRE(manager.getUsedBytes() > 0);
        REQUIRE(manager.getTempPool()->size() > 0);
        
        manager.reset();
        
        REQUIRE(manager.getUsedBytes() == 0);
        REQUIRE(manager.getTempPool()->size() == 0);
    }
    
    SECTION("Validation") {
        REQUIRE(manager.validate());
        
        StrDesc desc;
        REQUIRE(manager.createString("Validation test", desc));
        REQUIRE(manager.validate());
        
        manager.collectGarbage();
        REQUIRE(manager.validate());
    }
}
