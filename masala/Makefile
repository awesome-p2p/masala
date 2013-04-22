#include /usr/share/hardening-includes/hardening.make

export CC = gcc
export CFLAGS = -O2
CFLAGS += -Wall -Wwrite-strings -pedantic -std=gnu99
CFLAGS += -DMASALA
#CFLAGS += $(shell dpkg-buildflags --get CFLAGS)
#CFLAGS += $(HARDENING_CFLAGS)
export POST_LINKING = -lpthread -lpolarssl
#POST_LINKING += $(shell dpkg-buildflags --get LDFLAGS)
#POST_LINKING += $(HARDENING_LDFLAGS)
#POST_LINKING += -lpolarssl
SUBDIRS =
OBJS=main.o conf.o unix.o log.o file.o lookup.o \
	hash.o list.o malloc.o opts.o str.o thrd.o \
	ben.o udp.o random.o send_p2p.o sha1.o \
	database.o node_p2p.o bucket.o neighboorhood.o \
	cache.o aes.o announce.o time.o p2p.o

.PHONY: all clean install $(SUBDIRS)

all: $(SUBDIRS) masala

%.o : ../src/%.c ../src/%.h
	$(CC) $(CFLAGS) -c $<

masala: $(OBJS)
	$(CC) $(OBJS) -o masala $(POST_LINKING)

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) clean -C $$dir; \
	done
	rm -f *.o masala

install:
	strip masala
