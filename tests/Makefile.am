# Automake file for tests

AM_CPPFLAGS = -I$(top_srcdir)/src

if HAVE_CATCH2
AM_CXXFLAGS = $(CATCH2_CFLAGS)
LDADD = $(CATCH2_LIBS)

check_PROGRAMS = test_variable_table test_runtime_stack

test_variable_table_SOURCES = test_variable_table.cpp \
	$(top_srcdir)/src/Runtime/VariableTable.hpp \
	$(top_srcdir)/src/Runtime/Value.hpp \
	$(top_srcdir)/src/Runtime/StringHeap.hpp \
	$(top_srcdir)/src/Runtime/StringTypes.hpp

test_runtime_stack_SOURCES = test_runtime_stack.cpp \
	$(top_srcdir)/src/Runtime/RuntimeStack.hpp \
	$(top_srcdir)/src/Runtime/Value.hpp \
	$(top_srcdir)/src/Runtime/StringTypes.hpp

TESTS = $(check_PROGRAMS)
endif
