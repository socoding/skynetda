PLAT ?= none
PLATS = linux macosx mingw

.PHONY: $(PLATS) clean cleanall lua cjson

none:
	@echo "usage: make <PLAT>"
	@echo "  PLAT is one of: $(PLATS)"

$(PLATS):
	$(MAKE) all PLAT=$@

CC= gcc
LUA_SRC_DIR=3rd/lua/src
IPATH= -I. -I$(LUA_SRC_DIR)
LPATH= -L. -L$(LUA_SRC_DIR)

ifeq ($(PLAT), macosx)
MYFLAGS := -std=gnu99 -O2 -Wall $(IPATH) 
else
MYFLAGS := -std=gnu99 -O2 -Wall -Wl,-E $(IPATH) 
endif

LIBS= -llua $(LPATH) -ldl -lm
HEADER = $(wildcard src/*.h)
SRCS= $(wildcard src/*.c)
ifeq ($(PLAT), mingw)
BINROOT= vscext/bin/windows
PROG= $(BINROOT)/skynetda.exe
LUAT=lua_dll
else
BINROOT= vscext/bin/$(PLAT)
PROG= $(BINROOT)/skynetda
LUAT=lua
endif

all: $(LUAT) cjson $(PROG)
	
lua: 
	$(MAKE) -C 3rd/lua $(PLAT)

lua_dll:
	$(MAKE) -C 3rd/lua $(PLAT)
	install -p -D $(LUA_SRC_DIR)/*.dll $(BINROOT)/lua.dll

cjson:
	$(MAKE) -C 3rd/lua-cjson install PLAT=$(PLAT)

$(PROG): $(SRCS) $(HEADER)
	$(CC) $(MYFLAGS) -o $@ $(SRCS) $(LIBS)

clean:
	rm -f $(PROG)

cleanall: clean
	$(MAKE) -C 3rd/lua clean
	$(MAKE) -C 3rd/lua-cjson clean
	rm -f $(PROG)
	rm -f $(BINROOT)/*.dll
	rm -f $(BINROOT)/*.so