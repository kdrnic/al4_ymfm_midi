OBJDIR=obj
BINNAME=game
CFLAGS2=$(CFLAGS)

#Change obj and exe name for debug builds
ifeq ($(filter debug,$(MAKECMDGOALS)),debug)
	OBJDIR:=$(OBJDIR)dbg
	CFLAGS2+=-g -O0 -DDEBUG
	BINNAME:=$(BINNAME)d
endif

#Profiling build options
ifeq ($(filter profile,$(MAKECMDGOALS)),profile)
	CFLAGS2+=-gdwarf-2 -fno-omit-frame-pointer -O2
endif

#Optimise more for release build and disable asserts
ifeq ($(filter regular,$(MAKECMDGOALS)),regular)
	CFLAGS2+=-O3 -DNDEBUG
endif

#Useful flags
CFLAGS2+=-Wall -Wuninitialized -Werror=implicit-function-declaration -Wno-unused -fplan9-extensions -Wstrict-prototypes

#Set CC and CXX on Windows for Windows build
ifeq ($(origin CC),default)
	ifeq ($(OS),Windows_NT)
		CC = gcc
		CXX = g++
	endif
endif

C_FILES=$(wildcard src/*.c)

C_OBJECTS=$(patsubst src/%,$(OBJDIR)/%,$(patsubst %.c,%.o,$(C_FILES)))
OBJECTS=$(C_OBJECTS)

HAVE_LIBS=-lymfm -lresample -lstdc++

INCLUDE_PATHS=
LINK_PATHS=-Lymfm -Llibresample

#Add Allegro to the libs
ifeq ($(OS),Windows_NT)
	INCLUDE_PATHS+=-I$(ALLEGRO_PATH)include
	LINK_PATHS+=-L$(ALLEGRO_PATH)lib
endif

LINK_FLAGS:=$(LINK_FLAGS)

#Build with static libgcc, to avoid needing silly DLL "libgcc_s_dw2-1.dll"
#NOTE: pthreadGC2.dll and alleg44.dll must also not depend on libgcc_s_dw2-1.dll
#(check both with pestudio or depends)
ifeq ($(OS),Windows_NT)
	ifeq (,$(findstring djgpp,$(CC)))
		LINK_FLAGS:=$(LINK_FLAGS) -static -static-libgcc -static-libstdc++
	endif
endif

#If 'small' build, attempt to optimize size
ifeq ($(filter regular,$(MAKECMDGOALS)),regular)
	LINK_FLAGS+=-Wl,--gc-sections
endif

HEADER_FILES=$(wildcard src/*.h)

$(OBJDIR)/%.o: src/%.c $(HEADER_FILES)
	$(CC) $(INCLUDE_PATHS) $(CFLAGS2) $< -c -o $@

regular: $(BINNAME).exe

clean:
	rm -f $(OBJDIR)/*.o
	rm -f ymfm/*.o
	rm -f ymfm/libymfm.a

$(OBJDIR):
	mkdir $(OBJDIR)

$(BINNAME).exe: $(OBJDIR) $(OBJECTS) ymfm/libymfm.a libresample/libresample.a
	gcc $(LINK_PATHS) $(LINK_FLAGS) $(CFLAGS2) $(OBJECTS) -o $(BINNAME).exe $(HAVE_LIBS) -lalleg -lm

debug: $(BINNAME).exe
profile: regular
fast: regular

ymfm/libymfm.a: ymfm/ymfm_lib.cpp src/ymfm_lib.h ymfm/Makefile
	cd ymfm && $(MAKE)

libresample/libresample.a:
	cd libresample && $(MAKE) CFLAGS=-O3
