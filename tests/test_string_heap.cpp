#include <catch2/catch_all.hpp>
#include "../src/Runtime/StringHeap.hpp"
#include "../src/Runtime/VariableTable.hpp"
#include "../src/Runtime/StringTypes.hpp"
#include <cstring>

using namespace gwbasic;

TEST_CASE("StringHeap basic allocation and deallocation") {
    uint8_t buffer[256];
    StringHeap heap(buffer, sizeof(buffer));
    
    SECTION("Empty heap properties") {
        REQUIRE(heap.freeBytes() == 256);
        REQUIRE(heap.usedBytes() == 0);
        REQUIRE(heap.totalBytes() == 256);
        REQUIRE(heap.validateIntegrity());
    }
    
    SECTION("Basic allocation") {
        StrDesc desc;
        REQUIRE(heap.alloc(10, desc));
        REQUIRE(desc.len == 10);
        REQUIRE(desc.ptr != nullptr);
        REQUIRE(heap.freeBytes() == 246);
        REQUIRE(heap.usedBytes() == 10);
        
        // Test allocation statistics
        auto stats = heap.getStatistics();
        REQUIRE(stats.totalAllocations == 1);
        REQUIRE(stats.currentUsed == 10);
        REQUIRE(stats.maxUsed == 10);
    }
    
    SECTION("Zero-length allocation") {
        StrDesc desc;
        REQUIRE(heap.alloc(0, desc));
        REQUIRE(desc.len == 0);
        REQUIRE(desc.ptr == nullptr);
        REQUIRE(heap.freeBytes() == 256);
    }
    
    SECTION("Allocation too large") {
        StrDesc desc;
        REQUIRE_FALSE(heap.alloc(300, desc)); // Larger than buffer
        REQUIRE_FALSE(heap.alloc(256, desc)); // Exactly buffer size (invalid)
    }
    
    SECTION("Out of range allocation") {
        StrDesc desc;
        REQUIRE_FALSE(heap.alloc(256, desc)); // Size > 255
    }
}

TEST_CASE("StringHeap allocCopy functionality") {
    uint8_t buffer[256];
    StringHeap heap(buffer, sizeof(buffer));
    
    SECTION("Copy from C string") {
        const char* test = "Hello World";
        StrDesc desc;
        REQUIRE(heap.allocCopy(test, desc));
        REQUIRE(desc.len == 11);
        REQUIRE(std::equal(desc.ptr, desc.ptr + desc.len, test));
    }
    
    SECTION("Copy from byte array") {
        const uint8_t data[] = {1, 2, 3, 4, 5};
        StrDesc desc;
        REQUIRE(heap.allocCopy(data, 5, desc));
        REQUIRE(desc.len == 5);
        REQUIRE(std::equal(desc.ptr, desc.ptr + desc.len, data));
    }
    
    SECTION("Copy null string") {
        StrDesc desc;
        REQUIRE(heap.allocCopy(nullptr, desc));
        REQUIRE(desc.len == 0);
        REQUIRE(desc.ptr == nullptr);
    }
    
    SECTION("Copy empty string") {
        StrDesc desc;
        REQUIRE(heap.allocCopy("", desc));
        REQUIRE(desc.len == 0);
        REQUIRE(desc.ptr == nullptr);
    }
}

TEST_CASE("StringHeap garbage collection") {
    uint8_t buffer[64]; // Small buffer to force GC
    StringHeap heap(buffer, sizeof(buffer));
    
    SECTION("Manual garbage collection with no roots") {
        // Allocate some strings
        StrDesc desc1, desc2, desc3;
        REQUIRE(heap.allocCopy("Hello", desc1));
        REQUIRE(heap.allocCopy("World", desc2));
        REQUIRE(heap.allocCopy("Test", desc3));
        
        REQUIRE(heap.usedBytes() == 14); // 5 + 5 + 4
        
        // Collect with no roots - should reclaim all memory
        std::size_t reclaimed = heap.collectGarbage();
        REQUIRE(reclaimed == 14);
        REQUIRE(heap.usedBytes() == 0);
    }
    
    SECTION("Garbage collection preserves rooted strings") {
        StrDesc desc1, desc2, desc3;
        REQUIRE(heap.allocCopy("Hello", desc1));
        REQUIRE(heap.allocCopy("World", desc2));
        REQUIRE(heap.allocCopy("Test", desc3));
        
        // Protect desc1 and desc3
        std::vector<StrDesc*> roots = {&desc1, &desc3};
        heap.compact(roots);
        
        // Should preserve "Hello" and "Test", total 9 bytes
        REQUIRE(heap.usedBytes() == 9);
        REQUIRE(desc1.len == 5);
        REQUIRE(desc3.len == 4);
        REQUIRE(std::equal(desc1.ptr, desc1.ptr + desc1.len, "Hello"));
        REQUIRE(std::equal(desc3.ptr, desc3.ptr + desc3.len, "Test"));
    }
}

TEST_CASE("StringHeap automatic garbage collection") {
    uint8_t buffer[32]; // Very small buffer
    StringHeap heap(buffer, sizeof(buffer), StringHeap::GCPolicy::OnDemand);
    
    SECTION("OnDemand policy triggers GC on allocation failure") {
        // Fill most of the buffer
        StrDesc desc1, desc2;
        REQUIRE(heap.allocCopy("12345678901234567890", desc1)); // 20 bytes
        REQUIRE(heap.allocCopy("1234567890", desc2)); // 10 bytes, total 30
        
        // Protect the existing strings so GC doesn't reclaim them
        heap.protectString(&desc1);
        heap.protectString(&desc2);
        
        // This should fail even after GC because strings are protected
        StrDesc desc3;
        REQUIRE_FALSE(heap.alloc(20, desc3)); // Would exceed buffer
        
        // Clear protection and verify GC works
        heap.clearProtected();
        REQUIRE(heap.alloc(20, desc3)); // Should succeed after GC reclaims everything
        
        auto stats = heap.getStatistics();
        REQUIRE(stats.totalAllocations >= 2);
    }
}

TEST_CASE("StringHeap with VariableTable integration") {
    uint8_t buffer[256];
    StringHeap heap(buffer, sizeof(buffer));
    DefaultTypeTable defTbl;
    VariableTable varTable(&defTbl, &heap);
    
    SECTION("Variable table as root provider") {
        // Create string variables
        REQUIRE(varTable.createString("S1$", "Hello"));
        REQUIRE(varTable.createString("S2$", "World"));
        REQUIRE(varTable.createString("S3$", "Test"));
        
        auto initialUsed = heap.usedBytes();
        REQUIRE(initialUsed == 14); // 5 + 5 + 4
        
        // Trigger garbage collection - all strings should be preserved
        std::size_t reclaimed = heap.collectGarbage();
        REQUIRE(reclaimed == 0); // No garbage to collect
        REQUIRE(heap.usedBytes() == initialUsed);
        
        // Verify strings are intact
        auto* s1 = varTable.tryGet("S1$");
        auto* s2 = varTable.tryGet("S2$");
        auto* s3 = varTable.tryGet("S3$");
        REQUIRE(s1 != nullptr);
        REQUIRE(s2 != nullptr);
        REQUIRE(s3 != nullptr);
        
        REQUIRE(std::equal(s1->scalar.s.ptr, s1->scalar.s.ptr + s1->scalar.s.len, "Hello"));
        REQUIRE(std::equal(s2->scalar.s.ptr, s2->scalar.s.ptr + s2->scalar.s.len, "World"));
        REQUIRE(std::equal(s3->scalar.s.ptr, s3->scalar.s.ptr + s3->scalar.s.len, "Test"));
    }
    
    SECTION("Clearing variables allows garbage collection") {
        REQUIRE(varTable.createString("TEMP$", "Temporary"));
        auto used = heap.usedBytes();
        REQUIRE(used == 9);
        
        // Clear variables and collect garbage
        varTable.clear();
        std::size_t reclaimed = heap.collectGarbage();
        REQUIRE(reclaimed == 9);
        REQUIRE(heap.usedBytes() == 0);
    }
}

TEST_CASE("StringHeap TempStrPool integration") {
    uint8_t buffer[256];
    StringHeap heap(buffer, sizeof(buffer));
    TempStrPool tempPool;
    
    heap.addRootProvider(&tempPool);
    
    SECTION("TempStrPool protects temporary strings") {
        // Create strings and add to temp pool
        StrDesc desc1, desc2;
        REQUIRE(heap.allocCopy("Temp1", desc1));
        REQUIRE(heap.allocCopy("Temp2", desc2));
        
        tempPool.pushCopy(desc1);
        tempPool.pushCopy(desc2);
        
        // Add a string not in temp pool
        StrDesc desc3;
        REQUIRE(heap.allocCopy("NoRoot", desc3));
        
        // GC should preserve temp pool strings but not desc3
        std::size_t reclaimed = heap.collectGarbage();
        REQUIRE(reclaimed == 6); // "NoRoot" should be reclaimed
        REQUIRE(heap.usedBytes() == 10); // "Temp1" + "Temp2"
    }
    
    SECTION("Clearing temp pool allows collection") {
        StrDesc desc;
        REQUIRE(heap.allocCopy("TempString", desc));
        tempPool.pushCopy(desc);
        
        REQUIRE(heap.usedBytes() == 10);
        
        // GC with temp pool should preserve string
        std::size_t reclaimed = heap.collectGarbage();
        REQUIRE(reclaimed == 0);
        REQUIRE(heap.usedBytes() == 10);
        
        // Clear temp pool and GC again
        tempPool.clear();
        reclaimed = heap.collectGarbage();
        REQUIRE(reclaimed == 10);
        REQUIRE(heap.usedBytes() == 0);
    }
    
    // Cleanup
    heap.removeRootProvider(&tempPool);
}

TEST_CASE("StringHeap protection mechanism") {
    uint8_t buffer[128];
    StringHeap heap(buffer, sizeof(buffer));
    
    SECTION("String protection during operations") {
        StrDesc desc1, desc2, desc3;
        REQUIRE(heap.allocCopy("Protected", desc1));
        REQUIRE(heap.allocCopy("Also", desc2));
        REQUIRE(heap.allocCopy("Unprotected", desc3));
        
        // Protect first two strings
        heap.protectString(&desc1);
        heap.protectString(&desc2);
        
        // GC should preserve protected strings
        std::size_t reclaimed = heap.collectGarbage();
        REQUIRE(reclaimed == 11); // "Unprotected"
        REQUIRE(heap.usedBytes() == 13); // "Protected" + "Also"
        
        // Clear protection and GC again
        heap.clearProtected();
        reclaimed = heap.collectGarbage();
        REQUIRE(reclaimed == 13);
        REQUIRE(heap.usedBytes() == 0);
    }
    
    SECTION("StringProtector RAII helper") {
        StrDesc desc1, desc2;
        REQUIRE(heap.allocCopy("RAII1", desc1));
        REQUIRE(heap.allocCopy("RAII2", desc2));
        
        {
            StringProtector protector(&heap);
            protector.protect(&desc1);
            protector.protect(&desc2);
            
            // Within scope, strings should be protected
            std::size_t reclaimed = heap.collectGarbage();
            REQUIRE(reclaimed == 0);
            REQUIRE(heap.usedBytes() == 10);
        } // StringProtector destructor clears protection
        
        // After scope, strings should be collectible
        std::size_t reclaimed = heap.collectGarbage();
        REQUIRE(reclaimed == 10);
        REQUIRE(heap.usedBytes() == 0);
    }
}

TEST_CASE("StringHeap policies and configuration") {
    uint8_t buffer[100];
    StringHeap heap(buffer, sizeof(buffer));
    
    SECTION("GC policy configuration") {
        REQUIRE(heap.getGCPolicy() == StringHeap::GCPolicy::OnDemand);
        
        heap.setGCPolicy(StringHeap::GCPolicy::Aggressive);
        REQUIRE(heap.getGCPolicy() == StringHeap::GCPolicy::Aggressive);
        
        heap.setGCPolicy(StringHeap::GCPolicy::Conservative);
        REQUIRE(heap.getGCPolicy() == StringHeap::GCPolicy::Conservative);
    }
    
    SECTION("GC threshold configuration") {
        REQUIRE(heap.getGCThreshold() == 0.2);
        
        heap.setGCThreshold(0.5);
        REQUIRE(heap.getGCThreshold() == 0.5);
    }
    
    SECTION("Statistics tracking") {
        auto stats = heap.getStatistics();
        REQUIRE(stats.totalAllocations == 0);
        REQUIRE(stats.gcCycles == 0);
        REQUIRE(stats.bytesReclaimed == 0);
        
        StrDesc desc;
        REQUIRE(heap.allocCopy("Stats", desc));
        
        stats = heap.getStatistics();
        REQUIRE(stats.totalAllocations == 1);
        REQUIRE(stats.currentUsed == 5);
        REQUIRE(stats.maxUsed == 5);
        
        heap.collectGarbage();
        stats = heap.getStatistics();
        REQUIRE(stats.gcCycles == 1);
        REQUIRE(stats.bytesReclaimed == 5);
    }
}

TEST_CASE("StringHeap error conditions and edge cases") {
    uint8_t buffer[16]; // Very small buffer
    StringHeap heap(buffer, sizeof(buffer));
    
    SECTION("Allocation failure handling") {
        StrDesc desc;
        // Fill the buffer
        REQUIRE(heap.allocCopy("1234567890123456", desc)); // Exactly 16 bytes
        REQUIRE(heap.freeBytes() == 0);
        
        // Protect the string to prevent GC from reclaiming it
        heap.protectString(&desc);
        
        // Further allocations should fail
        StrDesc desc2;
        REQUIRE_FALSE(heap.alloc(1, desc2));
        REQUIRE_FALSE(heap.allocCopy("X", desc2));
        
        heap.clearProtected();
    }
    
    SECTION("Fragmentation calculation") {
        REQUIRE(heap.fragmentation() == 0.0); // Empty heap
        
        StrDesc desc;
        REQUIRE(heap.allocCopy("12345678", desc)); // Half full
        double frag = heap.fragmentation();
        REQUIRE(frag == 0.5); // 50% used
    }
    
    SECTION("Heap integrity validation") {
        REQUIRE(heap.validateIntegrity());
        
        // Even after allocations, integrity should hold
        StrDesc desc1, desc2;
        REQUIRE(heap.allocCopy("Test", desc1));
        REQUIRE(heap.allocCopy("More", desc2));
        REQUIRE(heap.validateIntegrity());
        
        // After GC, integrity should still hold
        heap.collectGarbage();
        REQUIRE(heap.validateIntegrity());
    }
}
