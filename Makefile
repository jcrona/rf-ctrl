#
# Copyright (C) 2016 Jean-Christophe Rona <jc@rona.fr>
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

GNU_PREFIX ?=

# Allow to create a static binary
STATIC ?= false

# Use an external implementation of iconv instead of the libc's one
USE_EXTERNAL_LIBICONV ?= false


ifeq ($(STATIC), true)
	LDFLAGS += -static
endif

CC=$(GNU_PREFIX)gcc
LD=$(GNU_PREFIX)ld

LDLIBS = -lusb-1.0 -lpthread

ifeq ($(USE_EXTERNAL_LIBICONV), true)
	LDLIBS += -liconv
endif

ifeq ($(STATIC), true)
	LDLIBS += -lgcc_eh
endif

TARGET = rf-ctrl
OBJECTS = he853.o ook-gpio.o dummy.o otax.o dio.o home-easy.o idk.o sumtech.o auchan.o auchan2.o somfy.o blyss.o rf-ctrl.o hid-libusb.o

all: $(TARGET)

$(TARGET): $(OBJECTS)
	@echo
	@echo -n "Linking ..."
	@$(CC) $(CFLAGS) $(LDFLAGS) $+ -o $@ $(LDLIBS)
	@echo " -> $@"
	@echo

clean:
	$(RM) $(OBJECTS) $(TARGET)

%.o : %.c
	@echo "[$@] ..."
	@$(CC) $(CFLAGS) -c $< -o $@

.PHONY: all clean
