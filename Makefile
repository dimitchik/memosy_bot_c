CFLAGS += $(shell pkg-config --cflags json-c)
LDFLAGS += $(shell pkg-config --libs json-c)

LDFLAGS += -lcurl

CFLAGS += -O3

all:
	mkdir -p build
	$(MAKE) main

main: main.c
	gcc -o build/main main.c $(CFLAGS) $(LDFLAGS)