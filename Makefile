# See LICENSE file for copyright and license details.

include config.mk

SRC = am3.c
OBJ = $(SRC:.c=.o)

LIB = libam3.a
INC = am3.h

all: $(LIB)

$(LIB): $(OBJ)
	@$(AR) -rcs $@ $(OBJ)

.c.o:
	@$(CC) $(CFLAGS) -c $<

install: $(LIB) $(INC)
	@echo @ install am3 to $(DESTDIR)$(PREFIX)
	@mkdir -p $(DESTDIR)$(PREFIX)/lib
	@cp $(LIB) $(DESTDIR)$(PREFIX)/lib/$(LIB)
	@mkdir -p $(DESTDIR)$(PREFIX)/include/
	@cp $(INC) $(DESTDIR)$(PREFIX)/include/

uninstall:
	@echo @ uninstall am3 from $(DESTDIR)$(PREFIX)
	@rm -f $(DESTDIR)$(PREFIX)/lib/$(LIB)
	@rm -rf $(DESTDIR)$(PREFIX)/include/

clean:
	rm -f $(LIB) $(OBJ)
test: test.c libam3.a
	gcc -g -o test test.c libam3.a -lnit
