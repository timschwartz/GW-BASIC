# Runtime library Makefile

noinst_LTLIBRARIES = libruntime.la

libruntime_la_SOURCES = \
    ArrayManager.cpp \
    ArrayManager.hpp \
    ArrayTypes.hpp \
    RuntimeStack.hpp \
    StringHeap.hpp \
    StringManager.hpp \
    StringTypes.hpp \
    Value.hpp \
    VariableTable.hpp

libruntime_la_CPPFLAGS = -I$(srcdir)/../
libruntime_la_CXXFLAGS = -std=c++20 -Wall -Wextra -O2
