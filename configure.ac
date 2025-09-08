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
PKG_CHECK_MODULES([SDL3], [sdl3])
PKG_CHECK_MODULES([SDL3_TTF], [SDL3_ttf], [have_sdl3_ttf=yes], [have_sdl3_ttf=no])
PKG_CHECK_MODULES([CATCH2], [catch2-with-main], [have_catch2=yes], [have_catch2=no])
AM_CONDITIONAL([HAVE_CATCH2], [test "x$have_catch2" = "xyes"])
AM_CONDITIONAL([HAVE_SDL3_TTF], [test "x$have_sdl3_ttf" = "xyes"])

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
                 src/InterpreterLoop/Makefile
                 src/ExpressionEvaluator/Makefile
                 src/NumericEngine/Makefile
                 tests/Makefile])
AC_OUTPUT
