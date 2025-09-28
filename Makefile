# Compiler and Tools
CC = g++
AR = ar
RANLIB = ranlib
TAR = tar
RM = rm -f

# Compiler Flags
CFLAGS = -Wall -std=c++11 -g

# Source and Object Files
LIBSRC = uthreads.cpp
LIBOBJ = $(LIBSRC:.cpp=.o)
TARGETS = libuthreads.a

# Tarball Settings
TARFLAGS = -cvf
TARNAME = ex2.tar
TARSRCS = $(LIBSRC) Makefile README

# Default target
all: $(TARGETS)

# Build static library
$(TARGETS): $(LIBOBJ)
	$(AR) rcs $@ $^
	$(RANLIB) $@

# Compile .cpp to .o
%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build artifacts
clean:
	$(RM) $(TARGETS) $(LIBOBJ) *~ core

# Create tar archive
tar:
	$(TAR) $(TARFLAGS) $(TARNAME) $(TARSRCS)
