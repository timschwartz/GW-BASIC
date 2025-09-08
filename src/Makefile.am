# Source directory Makefile

SUBDIRS = Tokenizer ProgramStore InterpreterLoop ExpressionEvaluator NumericEngine Runtime

# Main GW-BASIC executable
bin_PROGRAMS = gw-basic

gw_basic_SOURCES = main.cpp
gw_basic_CPPFLAGS = -I$(srcdir)/Tokenizer -I$(srcdir)/ProgramStore -I$(srcdir)/InterpreterLoop -I$(srcdir)/ExpressionEvaluator -I$(srcdir)/NumericEngine -I$(srcdir)/Runtime -I$(srcdir) $(SDL3_CFLAGS)
gw_basic_CXXFLAGS = -std=c++20 -Wall -Wextra -O2
gw_basic_LDADD = \
    $(srcdir)/InterpreterLoop/libinterpreterloop.la \
    $(srcdir)/ProgramStore/libprogramstore.la \
    $(srcdir)/Tokenizer/libtokenizer.la \
    $(srcdir)/ExpressionEvaluator/libexpressionevaluator.la \
    $(srcdir)/NumericEngine/libnumericengine.la \
    $(srcdir)/Runtime/libruntime.la \
    $(SDL3_LIBS)
