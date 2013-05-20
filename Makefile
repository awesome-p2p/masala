
CC ?= gcc
CFLAGS ?= -O2 -Wall -Wwrite-strings -pedantic -std=gnu99
POST_LINKING = -lpthread
FEATURES ?= dns web nss

OBJS_=main.o conf.o unix.o log.o file.o lookup.o \
	hash.o list.o malloc.o opts.o str.o thrd.o \
	ben.o udp.o random.o send_p2p.o sha1.o \
	database.o node_p2p.o bucket.o neighboorhood.o \
	cache.o announce.o time.o p2p.o
OBJS = $(patsubst %,build/%,$(OBJS_))

.PHONY: all clean install masala libnss_masala.so.2

all: masala


ifneq (,$(findstring dns,$(FEATURES)))
  OBJS += build/masala-dns.o
  CFLAGS += -DDNS
endif

ifneq (,$(findstring web,$(FEATURES)))
  OBJS += build/masala-web.o
  CFLAGS += -DWEB
endif

ifneq (,$(findstring web,$(FEATURES)))
  OBJS += build/masala-nss.o
  CFLAGS += -DNSS
  EXTRA += libnss_masala.so.2
endif


build/%.o : src/%.c src/%.h
	$(CC) $(CFLAGS) -c -o $@ $<

libnss_masala.so.2: build/masala-libnss.o
	$(CC) $(CFLAGS) -shared -Wl,-soname,libnss_masala.so.2 -o build/libnss_masala.so.2 $(POST_LINKING)

masala: $(OBJS) $(EXTRA)
	$(CC) $(OBJS) -o build/masala $(POST_LINKING)

clean:
	rm -f build/*.o build/masala
	rm -f build/*.o build/libnss_masala.so.2

install:
	strip build/masala
	strip build/libnss_masala.so.2
