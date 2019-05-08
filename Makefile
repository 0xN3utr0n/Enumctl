#
# 0xN3utr0n - makefile sample
#

# MACROS

BPATH   = ./bin
CC      = cc
CFLAGS  = -g -Wall -Wextra -Werror -Wformat-security -O3 -std=gnu99 -march=x86-64
CFLAGS += -fPIE -pie -fstack-clash-protection -fstack-protector --param ssp-buffer-size=4 
CFLAGS += -D_FORTIFY_SOURCE=2 -Wl,-z,relro,-z,now
CFLAGS += -Wl,-z,noexecstack -fomit-frame-pointer
LIB     = `pkg-config --cflags --libs libsystemd`

# Targets

all: clean enumctl

clean:
	@rm -rf $(BPATH)/*

enumctl: Enumctl.c
	@$(CC) $(CFLAGS) -o $(BPATH)/Enumctl Enumctl.c $(LIB)
	@echo OK
