AC_PREREQ([2.72])
AC_INIT([GW-BASIC-CPP], [0.1], [tim@metaverse.systems])
AC_CONFIG_SRCDIR([src/Tokenizer/Tokenizer.cpp])
AC_CONFIG_HEADERS([config.h])
LT_INIT

AC_CONFIG_MACRO_DIRS([m4])
# Checks for programs.
AC_PROG_CXX
AM_INIT_AUTOMAKE([subdir-objects])

# Checks for libraries.
PKG_CHECK_MODULES([CATCH2], [catch2-with-main])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_TYPE_INT16_T
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

AC_CONFIG_FILES([Makefile 
                 src/Makefile
                 src/Tokenizer/Makefile
                 src/ProgramStore/Makefile
                 src/InterpreterLoop/Makefile])
AC_OUTPUT
