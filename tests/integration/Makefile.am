# Integration tests Makefile.am

AM_CPPFLAGS = -I$(top_srcdir)/src \
              -I$(top_srcdir)/src/InterpreterLoop \
              -I$(top_srcdir)/src/ProgramStore \
              -I$(top_srcdir)/src/Tokenizer \
              -I$(top_srcdir)/src/ExpressionEvaluator \
              -I$(top_srcdir)/src/Runtime

# Integration test programs (not run automatically, but can be built)
noinst_PROGRAMS = test_load

test_load_SOURCES = test_load.cpp
test_load_LDADD = $(top_builddir)/src/InterpreterLoop/libinterpreterloop.la \
                  $(top_builddir)/src/ProgramStore/libprogramstore.la \
                  $(top_builddir)/src/Tokenizer/libtokenizer.la \
                  $(top_builddir)/src/ExpressionEvaluator/libexpressionevaluator.la

# Mark these as manual tests since they require example files
EXTRA_DIST = README.md

# Custom target to run integration tests
.PHONY: check-integration
check-integration: $(noinst_PROGRAMS)
	@echo "Running integration tests..."
	@if [ -f "$(top_srcdir)/examples/test.bas" ]; then \
		cd $(top_srcdir) && ./tests/integration/test_load; \
	else \
		echo "Example files not found. Please ensure examples/test.bas exists."; \
		exit 1; \
	fi
