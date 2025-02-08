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



CC = gcc
CFLAGS = -Wall -Wextra -g $(shell pkg-config --cflags gio-2.0)
LDFLAGS = $(shell pkg-config --libs gio-2.0)

VERSION ?= $(shell git describe --always)#? version

SRCDIR = src
OBJDIR = obj

$(OBJDIR):
	mkdir -p $@

SOURCES = $(SRCDIR)/main.c $(SRCDIR)/handler.c
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $@ $(LDFLAGS)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

wizdns:  $(OBJDIR) $(OBJECTS) ## build wizdns
	$(CC) $(OBJECTS) $(LDFLAGS) -o $@

clean: ## clean build artifacts
	rm -rf $(OBJDIR) wizdns
