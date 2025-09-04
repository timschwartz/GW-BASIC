# InterpreterLoop library and example

AUTOMAKE_OPTIONS = subdir-objects

lib_LTLIBRARIES = libinterpreterloop.la

libinterpreterloop_la_SOURCES = \
	InterpreterLoop.cpp \
	InterpreterLoop.hpp

libinterpreterloop_la_CPPFLAGS = -I$(srcdir) -I$(top_srcdir)/src/ProgramStore -I$(top_srcdir)/src/Tokenizer
libinterpreterloop_la_CXXFLAGS = -std=c++20 -Wall -Wextra -O2

# Tests
check_PROGRAMS = test_interpreterloop
TESTS = test_interpreterloop
test_interpreterloop_SOURCES = test_interpreterloop.cpp
test_interpreterloop_CPPFLAGS = $(AM_CPPFLAGS) $(CATCH2_CFLAGS) -I$(top_srcdir)/src/ProgramStore -I$(top_srcdir)/src/Tokenizer
test_interpreterloop_LDADD = libinterpreterloop.la \
	$(top_builddir)/src/ProgramStore/libprogramstore.la \
	$(top_builddir)/src/Tokenizer/libtokenizer.la $(CATCH2_LIBS)

# Example runnable
bin_PROGRAMS = interpreter_example
interpreter_example_SOURCES = interpreter_example.cpp
interpreter_example_CPPFLAGS = -I$(srcdir) -I$(top_srcdir)/src/ProgramStore -I$(top_srcdir)/src/Tokenizer
interpreter_example_LDADD = libinterpreterloop.la \
	$(top_builddir)/src/ProgramStore/libprogramstore.la \
	$(top_builddir)/src/Tokenizer/libtokenizer.la

# Headers to install (public API)
include_HEADERS = InterpreterLoop.hpp

# Clean
CLEANFILES = *.lo *.la *.o interpreter_example
