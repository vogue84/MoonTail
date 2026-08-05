CC ?= gcc
CFLAGS ?= -O2 -Wall -std=c11 -Iclient

.PHONY: all clean check

all: build/moontail build/moontail-volunteer

build:
	mkdir -p build

build/moontail: client/moontail.c client/moon.h client/protocol.h | build
	$(CC) $(CFLAGS) -o $@ client/moontail.c $(if $(filter Windows_NT,$(OS)),-lws2_32,)

build/moontail-volunteer: server/volunteer.c client/protocol.h | build
	$(CC) $(CFLAGS) -o $@ server/volunteer.c

clean:
	rm -f build/moontail build/moontail-volunteer

check:
	bash scripts/check-loc.sh
	bash scripts/check-security.sh
