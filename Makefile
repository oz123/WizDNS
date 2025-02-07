###
#  general configuration of the makefile itself
###
SHELL := /bin/bash
.DEFAULT_GOAL := help

.PHONY: help
help:
	@mh -f $(MAKEFILE_LIST) $(target) || echo "Please install mh from https://github.com/oz123/mh/releases"
ifndef target
	@(which mh > /dev/null 2>&1 && echo -e "\nUse \`make help target=foo\` to learn more about foo.")
endif

VERSION ?= $(shell git describe --always)#? version


wizdns:  ## build wizdns
	gcc src/main.c -o wizdns `pkg-config --cflags --libs gio-2.0`
