
#include "MBFFormat.hpp"
#include <iostream>
int main() {
    auto mbfMin = MBF::ieeeToMBF32(1e-38f);
    std::cout << "1e-38f -> zero: " << (mbfMin.isZero() ? "true" : "false") << std::endl;
    std::cout << "exponent: " << static_cast<int>(mbfMin.exponent) << std::endl;
    return 0;
}
