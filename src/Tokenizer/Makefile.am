# Tokenizer library and tests

# Library
lib_LTLIBRARIES = libtokenizer.la
libtokenizer_la_SOURCES = Tokenizer.cpp Tokenizer.hpp
libtokenizer_la_CPPFLAGS = $(AM_CPPFLAGS)

# Headers to install
include_HEADERS = Tokenizer.hpp

# Test programs
check_PROGRAMS = test_tokenizer
TESTS = test_tokenizer

test_tokenizer_SOURCES = test_tokenizer.cpp
test_tokenizer_CPPFLAGS = $(AM_CPPFLAGS) $(CATCH2_CFLAGS)
test_tokenizer_LDADD = libtokenizer.la $(CATCH2_LIBS)

# Example program
bin_PROGRAMS = tokenizer_example
tokenizer_example_SOURCES = tokenizer_example.cpp
tokenizer_example_CPPFLAGS = $(AM_CPPFLAGS)
tokenizer_example_LDADD = libtokenizer.la

# Documentation
EXTRA_DIST = TOKENIZER_README.md TOKENIZER_SUMMARY.md

# C++ standard
AM_CPPFLAGS = -std=c++20 -Wall -Wextra -O2

# Clean up generated files
CLEANFILES = *.log *.trs

# Additional make targets
.PHONY: run-tests
run-tests: check

.PHONY: run-example
run-example: tokenizer_example
	./tokenizer_example