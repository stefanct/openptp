# Makefile for packet_if module

SHELL = /bin/sh

#### Start of system configuration section. ####
srcdir = .
CC = gcc -O
RM = rm -rf

# Directories.
prefix = /usr/local
bindir = $(prefix)/lib

LIBS =
INCLUDES = -I$(srcdir) -I$(srcdir)/linux -I$(srcdir)/../include -I$(srcdir)/../include/linux -I$(srcdir)/../ptp -I/usr/src/linux/include/
CDEBUG = -g
CFLAGS = $(CDEBUG) $(INCLUDES) -Wall -O0 
LDFLAGS = -g -shared
#### End of system configuration section. ####

OBJ = linux/ptp_packet.o
HDR = $(srcdir)/../include/packet_if.h $(srcdir)/../include/os_if.h $(srcdir)/../include/ptp_general.h $(srcdir)/../include/ptp_message.h $(srcdir)/../include/ptp_config.h $(srcdir)/../include/ptp_internal.h $(srcdir)/../ptp/ptp.h

PROG = $(srcdir)/../bin/libpacket_if.so

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
