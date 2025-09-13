# Automake file for tests

SUBDIRS = integration

AM_CPPFLAGS = -I$(top_srcdir)/src

if HAVE_CATCH2
AM_CXXFLAGS = $(CATCH2_CFLAGS)
LDADD = $(CATCH2_LIBS)

check_PROGRAMS = test_variable_table test_runtime_stack test_string_heap test_string_manager test_array_manager test_event_traps test_error_handling test_simple_error

test_variable_table_SOURCES = test_variable_table.cpp \
	$(top_srcdir)/src/Runtime/VariableTable.hpp \
	$(top_srcdir)/src/Runtime/Value.hpp \
	$(top_srcdir)/src/Runtime/StringHeap.hpp \
	$(top_srcdir)/src/Runtime/StringTypes.hpp

test_runtime_stack_SOURCES = test_runtime_stack.cpp \
	$(top_srcdir)/src/Runtime/RuntimeStack.hpp \
	$(top_srcdir)/src/Runtime/Value.hpp \
	$(top_srcdir)/src/Runtime/StringTypes.hpp

test_string_heap_SOURCES = test_string_heap.cpp \
	$(top_srcdir)/src/Runtime/StringHeap.hpp \
	$(top_srcdir)/src/Runtime/StringTypes.hpp \
	$(top_srcdir)/src/Runtime/VariableTable.hpp \
	$(top_srcdir)/src/Runtime/Value.hpp

test_string_manager_SOURCES = test_string_manager.cpp \
	$(top_srcdir)/src/Runtime/StringManager.hpp \
	$(top_srcdir)/src/Runtime/StringHeap.hpp \
	$(top_srcdir)/src/Runtime/StringTypes.hpp

test_array_manager_SOURCES = test_array_manager.cpp \
	$(top_srcdir)/src/Runtime/ArrayManager.hpp \
	$(top_srcdir)/src/Runtime/ArrayManager.cpp \
	$(top_srcdir)/src/Runtime/ArrayTypes.hpp \
	$(top_srcdir)/src/Runtime/Value.hpp \
	$(top_srcdir)/src/Runtime/StringHeap.hpp \
	$(top_srcdir)/src/Runtime/StringTypes.hpp

test_event_traps_SOURCES = test_event_traps.cpp \
	$(top_srcdir)/src/Runtime/EventTraps.hpp \
	$(top_srcdir)/src/Runtime/EventTraps.cpp

test_error_handling_SOURCES = test_error_handling.cpp \
	$(top_srcdir)/src/InterpreterLoop/InterpreterLoop.hpp \
	$(top_srcdir)/src/InterpreterLoop/InterpreterLoop.cpp \
	$(top_srcdir)/src/InterpreterLoop/BasicDispatcher.hpp \
	$(top_srcdir)/src/ProgramStore/ProgramStore.hpp \
	$(top_srcdir)/src/ProgramStore/ProgramStore.cpp \
	$(top_srcdir)/src/Tokenizer/Tokenizer.hpp \
	$(top_srcdir)/src/Tokenizer/Tokenizer.cpp \
	$(top_srcdir)/src/Runtime/GWError.hpp \
	$(top_srcdir)/src/Runtime/RuntimeStack.hpp \
	$(top_srcdir)/src/Runtime/EventTraps.hpp \
	$(top_srcdir)/src/Runtime/EventTraps.cpp \
	$(top_srcdir)/src/Runtime/VariableTable.hpp \
	$(top_srcdir)/src/Runtime/Value.hpp \
	$(top_srcdir)/src/Runtime/StringHeap.hpp \
	$(top_srcdir)/src/Runtime/StringTypes.hpp \
	$(top_srcdir)/src/Runtime/ArrayManager.hpp \
	$(top_srcdir)/src/Runtime/ArrayManager.cpp \
	$(top_srcdir)/src/Runtime/ArrayTypes.hpp \
	$(top_srcdir)/src/Runtime/DataManager.hpp \
	$(top_srcdir)/src/Runtime/DataManager.cpp \
	$(top_srcdir)/src/Runtime/FileManager.hpp \
	$(top_srcdir)/src/Runtime/FileManager.cpp \
	$(top_srcdir)/src/Runtime/UserFunctionManager.hpp \
	$(top_srcdir)/src/Runtime/UserFunctionManager.cpp \
	$(top_srcdir)/src/ExpressionEvaluator/ExpressionEvaluator.hpp \
	$(top_srcdir)/src/ExpressionEvaluator/ExpressionEvaluator.cpp \
	$(top_srcdir)/src/NumericEngine/NumericEngine.hpp \
	$(top_srcdir)/src/NumericEngine/NumericEngine.cpp \
	$(top_srcdir)/src/NumericEngine/MBFFormat.hpp \
	$(top_srcdir)/src/NumericEngine/MBFFormat.cpp \
	$(top_srcdir)/src/Graphics/GraphicsContext.hpp \
	$(top_srcdir)/src/Graphics/GraphicsContext.cpp

test_simple_error_SOURCES = test_simple_error.cpp \
	$(top_srcdir)/src/InterpreterLoop/InterpreterLoop.hpp \
	$(top_srcdir)/src/InterpreterLoop/InterpreterLoop.cpp \
	$(top_srcdir)/src/InterpreterLoop/BasicDispatcher.hpp \
	$(top_srcdir)/src/ProgramStore/ProgramStore.hpp \
	$(top_srcdir)/src/ProgramStore/ProgramStore.cpp \
	$(top_srcdir)/src/Tokenizer/Tokenizer.hpp \
	$(top_srcdir)/src/Tokenizer/Tokenizer.cpp \
	$(top_srcdir)/src/Runtime/GWError.hpp \
	$(top_srcdir)/src/Runtime/RuntimeStack.hpp \
	$(top_srcdir)/src/Runtime/EventTraps.hpp \
	$(top_srcdir)/src/Runtime/EventTraps.cpp \
	$(top_srcdir)/src/Runtime/VariableTable.hpp \
	$(top_srcdir)/src/Runtime/Value.hpp \
	$(top_srcdir)/src/Runtime/StringHeap.hpp \
	$(top_srcdir)/src/Runtime/StringTypes.hpp \
	$(top_srcdir)/src/Runtime/ArrayManager.hpp \
	$(top_srcdir)/src/Runtime/ArrayManager.cpp \
	$(top_srcdir)/src/Runtime/ArrayTypes.hpp \
	$(top_srcdir)/src/Runtime/DataManager.hpp \
	$(top_srcdir)/src/Runtime/DataManager.cpp \
	$(top_srcdir)/src/Runtime/FileManager.hpp \
	$(top_srcdir)/src/Runtime/FileManager.cpp \
	$(top_srcdir)/src/Runtime/UserFunctionManager.hpp \
	$(top_srcdir)/src/Runtime/UserFunctionManager.cpp \
	$(top_srcdir)/src/ExpressionEvaluator/ExpressionEvaluator.hpp \
	$(top_srcdir)/src/ExpressionEvaluator/ExpressionEvaluator.cpp \
	$(top_srcdir)/src/NumericEngine/NumericEngine.hpp \
	$(top_srcdir)/src/NumericEngine/NumericEngine.cpp \
	$(top_srcdir)/src/NumericEngine/MBFFormat.hpp \
	$(top_srcdir)/src/NumericEngine/MBFFormat.cpp \
	$(top_srcdir)/src/Graphics/GraphicsContext.hpp \
	$(top_srcdir)/src/Graphics/GraphicsContext.cpp

TESTS = $(check_PROGRAMS)
endif
