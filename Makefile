ifneq ($(V),1)
	Q=@
endif

DESTDIR?=./
PREFIX?=/usr/
CC:=$(CROSS_COMPILE)gcc
LD:=$(CROSS_COMPILE)ld
PKG_CONFIG ?= pkg-config
INDENT ?= indent -kr -nut -nbbo -l92

CFLAGS := $(CFLAGS) -g -O2
EXTRA_CFLAGS += -Wall -Wno-comment -std=c99 -D_GNU_SOURCE -fPIC
EXTRA_CFLAGS += -I. $(shell $(PKG_CONFIG) --cflags glib-2.0)
EXTRA_LDFLAGS := $(shell $(PKG_CONFIG) --libs glib-2.0) -lpthread

VALGRINDCMD=
ifneq ($(VALGRIND),no)
ifeq ($(shell $(PKG_CONFIG) --exists valgrind && echo 1),1)
EXTRA_CFLAGS += -DHAVE_VALGRIND $(shell $(PKG_CONFIG) --cflags valgrind)
VALGRINDCMD=valgrind --leak-check=full
endif
endif

TARGET = omem
LIBRARY = lib$(TARGET).so
OBJS = omem.o omlist.o omhtable.o omhtree.o

all: $(LIBRARY)

$(LIBRARY): $(OBJS) omem.h
	@echo "Creating library "$@""
	$(Q)$(CC) -shared $(LDFLAGS) -o $@ $(OBJS) $(EXTRA_LDFLAGS)

%.o: %.c
	@echo "Compiling "$<""
	$(Q)$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c $< -o $@

ifeq (test,$(firstword $(MAKECMDGOALS)))
TEST_ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
$(eval $(TEST_ARGS):;@:)
endif
test: $(LIBRARY) test.c
	$(Q)$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -o $@ $^ -L. -l$(TARGET) -lcunit $(EXTRA_LDFLAGS)
	@echo "Running unit test: $<"
	LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):./ $(VALGRINDCMD) ./test $(TEST_ARGS)
	@echo "Tests have been run!"

indent:
	$(INDENT) *.c *.h

install: all
	@install -d $(DESTDIR)/$(PREFIX)/lib
	@install -D $(LIBRARY) $(DESTDIR)/$(PREFIX)/lib/
	@install -d $(DESTDIR)/$(PREFIX)/include
	@install -D $(TARGET).h $(DESTDIR)/$(PREFIX)/include
	@install -d $(DESTDIR)/$(PREFIX)/bin
	@install -D $(TARGET).pc $(DESTDIR)/$(PREFIX)/lib/pkgconfig/

clean:
	@echo "Cleaning..."
	@rm -f $(LIBRARY) test $(OBJS) *.c~ *.h~

.PHONY: all clean test indent
