# Makefile for Openptp

SHELL = /bin/sh
MAKE_OPTS = 

.PHONY: all

all: 
	$(MAKE) -C xml_parser $(MAKE_OPTS)
	$(MAKE) -C src $(MAKE_OPTS)

install: all
	$(MAKE) -C xml_parser install
	$(MAKE) -C src install

clean: 
	$(MAKE) clean -C xml_parser $(MAKE_OPTS)
	$(MAKE) clean -C src $(MAKE_OPTS)
