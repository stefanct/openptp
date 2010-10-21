# Makefile for clock_if module

SHELL = /bin/sh

#### Start of system configuration section. ####
srcdir = .
CC = gcc -O
RM = rm -rf

# Directories.
prefix = /usr/local
bindir = $(prefix)/lib

LIBS =
INCLUDES = -I$(srcdir) -I$(srcdir)/linux -I$(srcdir)/../include -I$(srcdir)/../include/linux -I$(srcdir)/../ptp
CDEBUG = -g
CFLAGS = $(CDEBUG) $(INCLUDES) -Wall -O0 -fPIC
LDFLAGS = -g -shared
#### End of system configuration section. ####

OBJ = linux/ptp_clock.o
HDR = $(srcdir)/../include/*.h 
PROG = $(srcdir)/../bin/libclock_if.so

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

all: $(PROG)

$(PROG): $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $(OBJ)

$(OBJ): $(HDR)

install: all
	cp $(PROG) $(bindir)/

clean: 
	$(RM) $(PROG) $(OBJ)
