# Runtime library Makefile

noinst_LTLIBRARIES = libruntime.la

libruntime_la_SOURCES = \
    ArrayManager.cpp \
    ArrayManager.hpp \
    ArrayTypes.hpp \
    DataManager.cpp \
    DataManager.hpp \
    EventTraps.cpp \
    EventTraps.hpp \
    FileManager.cpp \
    FileManager.hpp \
    RuntimeStack.hpp \
    StringFunctions.cpp \
    StringFunctions.hpp \
    StringHeap.hpp \
    StringManager.hpp \
    StringTypes.hpp \
    UserFunctionManager.cpp \
    UserFunctionManager.hpp \
    Value.hpp \
    VariableTable.hpp

libruntime_la_CPPFLAGS = -I$(srcdir)/../
libruntime_la_CXXFLAGS = -std=c++20 -Wall -Wextra -O2
