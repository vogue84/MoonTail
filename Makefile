CC ?= gcc
CFLAGS ?= -O2 -Wall -std=c11 -Iclient -Iassets

.PHONY: all clean check

all: build/moontail build/moontail-volunteer

build:
	mkdir -p build

build/moontail: client/moontail.c client/cli.c client/protocol.h assets/moontail-banner.h | build
	$(CC) $(CFLAGS) -o $@ client/moontail.c client/cli.c $(if $(filter Windows_NT,$(OS)),-lws2_32,)

build/moontail-volunteer: server/volunteer.c client/protocol.h | build
	$(CC) $(CFLAGS) -o $@ server/volunteer.c

clean:
	rm -f build/moontail build/moontail-volunteer

check:
	bash scripts/check-loc.sh
	bash scripts/check-security.sh
