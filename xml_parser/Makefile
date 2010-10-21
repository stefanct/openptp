# Makefile for Flexibilis XML parser

SHELL = /bin/sh

#### Start of system configuration section. ####

srcdir = .
CC = gcc -O
AR = ar
RM = rm -rf

LIBS =
INCLUDES = -I$(srcdir) -I$(srcdir)/include 
CDEBUG = -g
CFLAGS = $(CDEBUG) $(INCLUDES) -Wall -O0 -fPIC
AR_ARGS = rcs

#### End of system configuration section. ####

OBJ =  xml_parser.o
HDR = xml_parser.h

PROG = libxml_parser.a

.c.o:
	$(CC) -c $(CFLAGS) -o $@ $<

all: $(PROG)

$(PROG): $(OBJ)
	$(AR) $(AR_ARGS) $@ $(OBJ)

$(OBJ): $(HDR)

install: all

clean: 
	$(RM) $(PROG) $(OBJ)
