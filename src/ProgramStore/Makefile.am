# ProgramStore Makefile

AUTOMAKE_OPTIONS = subdir-objects

# Library target
lib_LTLIBRARIES = libprogramstore.la

# Source files
libprogramstore_la_SOURCES = \
	ProgramStore.cpp \
	ProgramStore.hpp

# Include directories
libprogramstore_la_CPPFLAGS = -I$(srcdir)

# Compiler flags
libprogramstore_la_CXXFLAGS = -std=c++11 -Wall -Wextra -O2

# Test program
check_PROGRAMS = test_programstore programstore_example

# Test source files
test_programstore_SOURCES = test_programstore.cpp
test_programstore_CPPFLAGS = -I$(srcdir)
test_programstore_CXXFLAGS = -std=c++11 -Wall -Wextra
test_programstore_LDADD = libprogramstore.la

# Example program
programstore_example_SOURCES = programstore_example.cpp
programstore_example_CPPFLAGS = -I$(srcdir)
programstore_example_CXXFLAGS = -std=c++11 -Wall -Wextra
programstore_example_LDADD = libprogramstore.la

# Tests to run
TESTS = test_programstore

# Headers to install
include_HEADERS = ProgramStore.hpp

# Clean up build artifacts
CLEANFILES = *.lo *.la *.o test_programstore programstore_example
