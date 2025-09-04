#include <catch2/catch_all.hpp>
#include "../src/Runtime/RuntimeStack.hpp"

using namespace gwbasic;

TEST_CASE("RuntimeStack FOR/GOSUB push/pop") {
    RuntimeStack st;
    ForFrame f{}; f.varKey = "I%"; f.control = Value::makeInt(1); f.limit = Value::makeInt(10); f.step = Value::makeInt(1); f.textPtr = 123;
    st.pushFor(f);
    REQUIRE(st.topFor() != nullptr);
    REQUIRE(st.topFor()->textPtr == 123);
    ForFrame out{};
    REQUIRE(st.popFor(out));
    REQUIRE(out.textPtr == 123);

    GosubFrame g{456, 100};
    st.pushGosub(g);
    GosubFrame gout{};
    REQUIRE(st.popGosub(gout));
    REQUIRE(gout.returnTextPtr == 456);
}

TEST_CASE("RuntimeStack string roots from frames") {
    RuntimeStack st;
    ForFrame f{};
    f.varKey = "S$";
    // Fake a string value (no heap needed for root enumeration test)
    StrDesc sd{}; sd.len = 3; sd.ptr = reinterpret_cast<uint8_t*>(0x1);
    f.control = Value::makeString(sd);
    st.pushFor(f);
    std::vector<StrDesc*> roots;
    st.collectStringRoots(roots);
    REQUIRE(roots.size() == 1);
    REQUIRE(roots[0]->len == 3);
}
