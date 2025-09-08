# NumericEngine library and tests

AUTOMAKE_OPTIONS = subdir-objects

lib_LTLIBRARIES = libnumericengine.la

libnumericengine_la_SOURCES = \
	NumericEngine.cpp \
	NumericEngine.hpp \
	MBFFormat.cpp \
	MBFFormat.hpp

libnumericengine_la_CPPFLAGS = -I$(srcdir)
libnumericengine_la_CXXFLAGS = -std=c++20 -Wall -Wextra -O2

# Tests
check_PROGRAMS = test_numeric test_mbf
TESTS = test_numeric test_mbf
test_numeric_SOURCES = test_numeric.cpp
test_numeric_CPPFLAGS = $(AM_CPPFLAGS) $(CATCH2_CFLAGS)
test_numeric_LDADD = libnumericengine.la $(CATCH2_LIBS)

test_mbf_SOURCES = test_mbf.cpp
test_mbf_CPPFLAGS = $(AM_CPPFLAGS) $(CATCH2_CFLAGS)
test_mbf_LDADD = libnumericengine.la $(CATCH2_LIBS)

# Headers to install
include_HEADERS = NumericEngine.hpp MBFFormat.hpp

CLEANFILES = *.lo *.la *.o test_numeric test_mbf
