CFLAGS2=-s -O3 -fno-rtti -fno-exceptions -fno-threadsafe-statics

ifeq ($(origin CC),default)
	ifeq ($(OS),Windows_NT)
		CC = gcc
		CXX = g++
	endif
endif

ifeq ($(CC),clang)
	ifeq ($(OS),Windows_NT)
		CFLAGS2+=-target i686-pc-windows-gnu
	endif
endif

H_FILES=$(sort $(wildcard *.h))
ifeq ($(NO_BUILD_BSP), 1)
	CPP_FILES=$(filter-out $(wildcard Recast*.cpp),$(sort $(wildcard *.cpp)))
else
	CPP_FILES=$(sort $(wildcard *.cpp))
endif
OBJECTS=$(patsubst %.cpp,%.o,$(CPP_FILES))

INCLUDE_PATHS=-I../src

regular: $(OBJECTS)
	$(AR) rcs libymfm.a $(OBJECTS)

clean:
	del *.o

%.o: %.cpp $(H_FILES)
	$(CXX) $(INCLUDE_PATHS) $(CFLAGS2) $< -c -o $@

ymfm_lib.o: ymfm_lib.cpp $(H_FILES) ../src/ymfm_lib.h
	$(CXX) $(INCLUDE_PATHS) $(CFLAGS2) $< -c -o $@
