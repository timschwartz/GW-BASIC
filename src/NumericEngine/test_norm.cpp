
#include "MBFFormat.hpp"
#include <iostream>
int main() {
    auto mbf = MBF::normalizeAndRound(1ULL, 1, false);
    std::cout << "Result isZero: " << (mbf.isZero() ? "true" : "false") << std::endl;
    std::cout << "Exponent: " << static_cast<int>(mbf.exponent) << std::endl;
    std::cout << "Mantissa: 0x" << std::hex << mbf.getMantissaBits() << std::dec << std::endl;
    return 0;
}
