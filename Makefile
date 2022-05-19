#
# Project files
#

BUILD_PREFIX = build2
LIB_SRCS = $(shell find src -type f -name '*.c')
LIB_OBJS = $(patsubst %.c, %.o, $(LIB_SRCS))
LIBTABFS = libtabfs.a

INSPECT_SRCS = $(shell find utils -type f -name '*.cpp')
INSPECT_OBJS = $(patsubst %.cpp, %.o, $(INSPECT_SRCS))
INSPECT_EXE = tabfs_inspect

HEADERS_RAW = $(shell find include -type f -name '*.h')
HEADERS = $(patsubst include/%.h, %.h, $(HEADERS_RAW))

CFLAGS = -Wall -Iinclude

ifeq ($(PREFIX),)
	PREFIX := /usr/local
endif

#
# Release build settings
#

REL_DIR = $(BUILD_PREFIX)/release
REL_LIB = $(REL_DIR)/$(LIBTABFS)
REL_LIB_OBJS = $(addprefix $(REL_DIR)/, $(LIB_OBJS))
REL_INSPECT = $(REL_DIR)/$(INSPECT_EXE)
REL_INSPECT_OBJS = $(addprefix $(REL_DIR)/, $(INSPECT_OBJS))
REL_CFLAGS = -O3 -static

#
# Debug build settings
#

DBG_DIR = $(BUILD_PREFIX)/debug
DBG_LIB = $(DBG_DIR)/$(LIBTABFS)
DBG_LIB_OBJS = $(addprefix $(DBG_DIR)/, $(LIB_OBJS))
DBG_INSPECT = $(DBG_DIR)/$(INSPECT_EXE)
DBG_INSPECT_OBJS = $(addprefix $(DBG_DIR)/, $(INSPECT_OBJS))
DBG_CFLAGS = -g -O0

.PHONY: all clean release debug install

# Default build
all: release

#
# Release rules
#

release: $(REL_LIB) $(REL_INSPECT)

$(REL_LIB): $(REL_LIB_OBJS)
	@mkdir -p "$(@D)"
	$(AR) -cr $@ $^

$(REL_INSPECT): $(REL_INSPECT_OBJS)
	@mkdir -p "$(@D)"
	$(CXX) -m64 -Wall $(REL_CFLAGS) -o $@ $^ $(REL_LIB)

$(REL_DIR)/%.o: %.c
	@mkdir -p "$(@D)"
	$(CC) -c -m64 $(CFLAGS) $(REL_CFLAGS) -o $@ $^

$(REL_DIR)/%.o: %.cpp
	@mkdir -p "$(@D)"
	$(CXX) -c -m64 $(CFLAGS) $(REL_CFLAGS) -o $@ $^

#
# Debug rules
#

debug: $(DBG_LIB) $(DBG_INSPECT)

$(DBG_LIB): $(DBG_LIB_OBJS)
	@mkdir -p "$(@D)"
	$(AR) -cr $@ $^

$(DBG_INSPECT): $(DBG_INSPECT_OBJS)
	@mkdir -p "$(@D)"
	$(CXX) -m64 -Wall $(DBG_CFLAGS) -o $@ $^ $(DBG_LIB)

$(DBG_DIR)/%.o: %.c
	@mkdir -p "$(@D)"
	$(CC) -c -m64 $(CFLAGS) $(DBG_CFLAGS) -o $@ $^

$(DBG_DIR)/%.o: %.cpp
	@mkdir -p "$(@D)"
	$(CXX) -c -m64 $(CFLAGS) $(DBG_CFLAGS) -o $@ $^

#
# Other rules
#

install: release
	install -d $(PREFIX)/bin/
	install -m 755 $(REL_INSPECT) $(PREFIX)/bin/tabfs_inspect
	install -d $(PREFIX)/lib/
	install -m 644 $(REL_LIB) $(PREFIX)/lib/libtabfs.a
	install -d $(PREFIX)/include/libtabfs/
	for headerfile in $(HEADERS) ; do \
		install -d $(PREFIX)/include/libtabfs/$$(dirname $$headerfile)/ ; \
		install -m 644 include/$$headerfile $(PREFIX)/include/libtabfs/$$headerfile ; \
	done

clean:
	rm -rf ./$(BUILD_PREFIX)