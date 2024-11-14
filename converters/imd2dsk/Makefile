# all of these can be over-ridden on the "make" command line if they don't suit your environment.
TOOL=imd2dsk
CFLAGS=-O2 -DNDEBUG -Wall -Wextra -pedantic
BIN=/usr/local/bin
INSTALL=install
CC=gcc

$(TOOL): $(TOOL).c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TOOL) $(TOOL).c $(LDLIBS)

.PHONY: clean install uninstall

clean:
	rm -f $(TOOL)

install: $(TOOL)
	$(INSTALL) -p -m u=rx,g=rx,o=rx $(TOOL) $(BIN)

uninstall:
	rm -f $(BIN)/$(TOOL)
