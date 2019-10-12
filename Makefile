#
# 0xN3utr0n - makefile sample
#
# MACROS

BPATH    = ./bin
CC       = cc
CLFAGS 	 = -Wall -Wextra -Werror -pedantic -Wconversion -Wformat-security  -std=gnu99 -march=x86-64  
DYN 	 = -fPIC -pie
SECFLAGS = -fstack-clash-protection -fstack-protector --param ssp-buffer-size=4 
SECFLAGS += -Wl,-z,relro,-z,now
SECFLAGS += -Wl,-z,noexecstack -fomit-frame-pointer 
CFLAGS   += `pkg-config --cflags --libs libsystemd`

# make debug=1
ifeq ($(debug),1)
CFLAGS   += -g -DDEBUG
else
SECFLAGS += -O3 -D_FORTIFY_SOURCE=2
endif

# Targets

all: clean enumctl.so enumctl 

clean:
	@rm -rf $(BPATH)/*

enumctl.so: Enumctl.c
	@$(CC) $(CFLAGS) -c $(DYN) -o $(BPATH)/Enumctl.o $^
	@$(CC) $(CFLAGS) -shared -o $(BPATH)/Enumctl.so $(BPATH)/Enumctl.o
	@rm $(BPATH)/Enumctl.o 

enumctl: Enumctl.c Enumctl.h main.c
	@$(CC) $(CFLAGS) $(DYN) $(SECFLAGS) -o $(BPATH)/Enumctl $^
	@echo OK