# Makefile for PTP stack

SHELL = /bin/sh

#### Start of system configuration section. ####
srcdir = .
CC = gcc -O
RM = rm -rf

# Directories.
prefix = /usr/local
bindir = $(prefix)/bin

LIBS = -lxml_parser -lclock_if -los_if -lpacket_if
LIBDIRS = -L$(srcdir)/../xml_parser -L$(srcdir)/bin -L$(prefix)/lib 
INCLUDES = -I$(srcdir)/include -I$(srcdir)/ptp -I$(srcdir)/include/linux -I$(srcdir)/../xml_parser/ 
CDEBUG = -g
CFLAGS = $(CDEBUG) $(INCLUDES) -Wall -O0 
LDFLAGS = -g $(LIBDIRS) $(LIBS) -Wl,-rpath $(prefix)/lib
MAKE_OPTS = 

#### End of system configuration section. ####

OBJ = ptp_main.o ptp/ptp.o ptp/ptp_port_state.o ptp/ptp_port_recv.o \
      ptp/ptp_port_packet.o ptp/ptp_framer.o ptp/ptp_bmc.o ptp/ptp_config.o
HDR = include/clock_if.h include/os_if.h include/packet_if.h \
      include/ptp_config.h include/ptp_general.h include/ptp_message.h \
      include/linux/ptp_types.h include/ptp_debug.h include/ptp_internal.h \
      include/linux/print.h
HDR += ptp/ptp_framer.h ptp/ptp_bmc.h ptp/ptp.h ptp/ptp_port.h 

PROG = $(srcdir)/bin/openptp

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

all: $(PROG)

$(PROG): $(OBJ)
	mkdir $(srcdir)/bin &> /dev/null || echo -n
	$(MAKE) -C clock_if $(MAKE_OPTS)
	$(MAKE) -C os_if $(MAKE_OPTS)
	$(MAKE) -C packet_if $(MAKE_OPTS)
	$(CC) -o $@ $(OBJ) $(LDFLAGS) 

$(OBJ): $(HDR)

install: all
	$(MAKE) -C clock_if install
	$(MAKE) -C os_if install
	$(MAKE) -C packet_if install
	cp $(PROG) $(bindir)/

clean: 
	$(RM) $(PROG) $(OBJ)
	$(MAKE) -C clock_if clean
	$(MAKE) -C os_if clean
	$(MAKE) -C packet_if clean
