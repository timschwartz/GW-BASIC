# ExpressionEvaluator library and tests

AUTOMAKE_OPTIONS = subdir-objects

lib_LTLIBRARIES = libexpressionevaluator.la

libexpressionevaluator_la_SOURCES = \
	ExpressionEvaluator.cpp \
	ExpressionEvaluator.hpp

libexpressionevaluator_la_CPPFLAGS = -I$(srcdir) -I$(top_srcdir)/src/Tokenizer
libexpressionevaluator_la_CXXFLAGS = -std=c++20 -Wall -Wextra -O2

# Tests
check_PROGRAMS = test_expression
TESTS = test_expression
test_expression_SOURCES = test_expression.cpp
test_expression_CPPFLAGS = $(AM_CPPFLAGS) $(CATCH2_CFLAGS) -I$(top_srcdir)/src/Tokenizer
test_expression_LDADD = libexpressionevaluator.la \
	$(top_builddir)/src/Tokenizer/libtokenizer.la $(CATCH2_LIBS)

# Headers to install
include_HEADERS = ExpressionEvaluator.hpp

CLEANFILES = *.lo *.la *.o test_expression
