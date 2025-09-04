#include <catch2/catch_all.hpp>
#include "../src/Runtime/VariableTable.hpp"
#include "../src/Runtime/StringHeap.hpp"

using namespace gwbasic;

TEST_CASE("DefaultTypeTable ranges and defaults") {
	DefaultTypeTable dt;
	REQUIRE(dt.getDefaultFor('A') == ScalarType::Single);
	dt.setRange('A', 'C', ScalarType::Int16);
	REQUIRE(dt.getDefaultFor('A') == ScalarType::Int16);
	REQUIRE(dt.getDefaultFor('B') == ScalarType::Int16);
	REQUIRE(dt.getDefaultFor('C') == ScalarType::Int16);
	REQUIRE(dt.getDefaultFor('D') == ScalarType::Single);
}

TEST_CASE("VariableTable name normalization and suffix typing") {
	DefaultTypeTable dt;
	VariableTable vt(&dt);

	auto& a = vt.getOrCreate("A");
	REQUIRE(a.scalar.type == ScalarType::Single);
	auto& b = vt.getOrCreate("b% ");
	REQUIRE(b.scalar.type == ScalarType::Int16);
	auto* c = vt.tryGet("B% ");
	REQUIRE(c != nullptr);
}

TEST_CASE("String assignment uses heap and GC roots include variables") {
	// Create a small heap (32 bytes) to force compaction path.
	uint8_t buf[32] = {0};
	StringHeap heap(buf, sizeof buf);
	DefaultTypeTable dt;
	VariableTable vt(&dt);

	// Assign a string to S$
	auto& s = vt.getOrCreate("S$");
	StrDesc tmp;
	const char* hello = "HELLO";
	REQUIRE(heap.allocCopy(reinterpret_cast<const uint8_t*>(hello), 5, tmp));
	s.scalar = Value::makeString(tmp);

	// Collect roots and compact; pointer should remain valid and updated if needed
	std::vector<StrDesc*> roots;
	vt.collectStringRoots(roots);
	REQUIRE(roots.size() == 1);
	heap.compact(roots);
	REQUIRE(s.scalar.s.len == 5);
	REQUIRE(std::equal(s.scalar.s.ptr, s.scalar.s.ptr + 5, (const uint8_t*)hello));
}
