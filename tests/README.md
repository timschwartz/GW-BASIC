Tests (Catch2)

- Integrated with Autotools; built when pkg-config finds `catch2-with-main`.

Run:

1) ./autogen.sh
2) ./configure
3) make check

If Catch2 isn’t detected, tests are skipped. Install Catch2 (v3) providing pkg-config `catch2-with-main`.

Standalone build (fallback):

g++ -std=c++17 -I../src -I. tests/test_variable_table.cpp -o test_variable_table
g++ -std=c++17 -I../src -I. tests/test_runtime_stack.cpp -o test_runtime_stack
