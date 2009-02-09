# Makefile for os_if module

SHELL = /bin/sh

# Directories.
prefix = /usr/local
bindir = $(prefix)/lib

#### Start of system configuration section. ####
srcdir = .
CC = gcc -O
RM = rm -rf

LIBS =
INCLUDES = -I$(srcdir) -I$(srcdir)/linux -I$(srcdir)/../include -I$(srcdir)/../include/linux -I$(srcdir)/../ptp
CDEBUG = -g
CFLAGS = $(CDEBUG) $(INCLUDES) -Wall -O0 
LDFLAGS = -g -shared
#### End of system configuration section. ####

OBJ = linux/print.o linux/ptp_os.o
HDR = $(srcdir)/../include/os_if.h $(srcdir)/../include/ptp_general.h $(srcdir)/../include/linux/print.h 

PROG = $(srcdir)/../bin/libos_if.so

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
