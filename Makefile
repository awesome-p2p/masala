
CC ?= gcc
CFLAGS ?= -O2 -Wall -Wwrite-strings -pedantic -std=gnu99
POST_LINKING = -lpthread
FEATURES ?= cmd dns nss web

OBJS_ = main.o conf.o unix.o log.o file.o lookup.o \
	hash.o list.o malloc.o opts.o str.o thrd.o \
	ben.o udp.o random.o send_p2p.o sha1.o \
	database.o bucket.o neighborhood.o \
	cache.o announce.o time.o p2p.o
OBJS = $(patsubst %,build/%,$(OBJS_))

.PHONY: all clean install masala masala-ctl libnss_masala.so.2

all: masala

ifeq ($(findstring cmd,$(FEATURES)),cmd)
  OBJS += build/masala-cmd.o
  CFLAGS += -DCMD
  EXTRA += masala-ctl
endif

ifeq ($(findstring dns,$(FEATURES)),dns)
  OBJS += build/masala-dns.o
  CFLAGS += -DDNS
endif

ifeq ($(findstring nss,$(FEATURES)),nss)
  OBJS += build/masala-nss.o
  CFLAGS += -DNSS
  EXTRA += libnss_masala.so.2
endif

ifeq ($(findstring web,$(FEATURES)),web)
  OBJS += build/masala-web.o
  CFLAGS += -DWEB
endif

build/%.o: src/%.c src/%.h
	$(CC) $(CFLAGS) -c -o $@ $<

libnss_masala.so.2: build/masala-libnss.o
	$(CC) $(CFLAGS) -shared -Wl,-soname,libnss_masala.so.2 -o build/libnss_masala.so.2 $(POST_LINKING)

masala-ctl:
	$(CC) src/masala-ctl.c -o build/masala-ctl

masala: $(OBJS) $(EXTRA)
	$(CC) $(OBJS) -o build/masala $(POST_LINKING)

clean:
	rm -f build/*.o
	rm -f build/masala
	rm -f build/masala-ctl
	rm -f build/libnss_masala.so.2

install:
	strip build/masala
	strip build/masala-ctl
	strip build/libnss_masala.so.2
	cp build/masala /usr/bin/
	cp build/masala-ctl /usr/bin/
	cp build/libnss_masala.so.2 /lib/
