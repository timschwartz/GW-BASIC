# Graphics module Makefile

noinst_LTLIBRARIES = libgraphics.la

libgraphics_la_SOURCES = GraphicsContext.cpp GraphicsContext.hpp
libgraphics_la_CPPFLAGS = -I$(srcdir) 
libgraphics_la_CXXFLAGS = -std=c++20 -Wall -Wextra -O2

# No tests for Graphics module yet, but we could add them
# TESTS = test_graphics
# check_PROGRAMS = test_graphics
# test_graphics_SOURCES = test_graphics.cpp
# test_graphics_CPPFLAGS = $(libgraphics_la_CPPFLAGS)
# test_graphics_CXXFLAGS = $(libgraphics_la_CXXFLAGS)
# test_graphics_LDADD = libgraphics.la
