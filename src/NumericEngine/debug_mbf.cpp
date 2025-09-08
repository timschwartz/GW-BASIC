#include "MBFFormat.hpp"
#include <iostream>
#include <iomanip>

using namespace MBF;

void debugValue(float value) {
    std::cout << "Testing value: " << value << std::endl;
    
    // Convert to MBF
    auto mbf = ieeeToMBF32(value);
    std::cout << "MBF exponent: " << static_cast<int>(mbf.exponent) << std::endl;
    std::cout << "MBF mantissa: 0x" << std::hex << mbf.getMantissaBits() << std::dec << std::endl;
    std::cout << "MBF negative: " << (mbf.isNegative() ? "true" : "false") << std::endl;
    
    // Convert back to IEEE
    float converted = mbf32ToIEEE(mbf);
    std::cout << "Converted back: " << converted << std::endl;
    std::cout << "Match: " << (converted == value ? "true" : "false") << std::endl;
    std::cout << "---" << std::endl;
}

int main() {
    debugValue(0.0f);
    debugValue(1.0f);
    debugValue(-1.0f);
    debugValue(0.5f);
    debugValue(2.0f);
    return 0;
}
