# This is the default variant when you `make` the Unix port.

PROG ?= micropython

MICROPY_ROM_TEXT_COMPRESSION = 1
MICROPY_VFS_FAT = 1
MICROPY_VFS_LFS1 = 1
MICROPY_VFS_LFS2 = 1

# Build 32-bit binaries on a 64-bit host
MICROPY_FORCE_32BIT = 0

# This variable can take the following values:
#  0 - no readline, just simple stdin input
#  1 - use MicroPython version of readline
MICROPY_USE_READLINE = 1

# btree module using Berkeley DB 1.xx
MICROPY_PY_BTREE = 1

# _thread module using pthreads
MICROPY_PY_THREAD = 1

# Subset of CPython termios module
MICROPY_PY_TERMIOS = 1

# Subset of CPython socket module
MICROPY_PY_USOCKET = 1

# ffi module requires libffi (libffi-dev Debian package)
MICROPY_PY_FFI = 1

# ussl module requires one of the TLS libraries below
MICROPY_PY_USSL = 1
# axTLS has minimal size but implements only a subset of modern TLS
# functionality, so may have problems with some servers.
MICROPY_SSL_AXTLS = 1
# mbedTLS is more up to date and complete implementation, but also
# more bloated.
MICROPY_SSL_MBEDTLS = 0

# jni module requires JVM/JNI
MICROPY_PY_JNI = 0

# Avoid using system libraries, use copies bundled with MicroPython
# as submodules (currently affects only libffi).
MICROPY_STANDALONE = 0

ifeq ($(MICROPY_SSL_AXTLS),1)
GIT_SUBMODULES += lib/axtls
endif

ifeq ($(MICROPY_PY_BTREE),1)
GIT_SUBMODULES += lib/berkeley-db-1.xx
endif

ifeq ($(MICROPY_PY_TERMIOS),1)
CFLAGS_MOD += -DMICROPY_PY_TERMIOS=1
SRC_MOD += variants/standard/modtermios.c
endif

ifeq ($(MICROPY_PY_FFI),1)
GIT_SUBMODULES += lib/libffi

ifeq ($(MICROPY_STANDALONE),1)
LIBFFI_CFLAGS_MOD := -I$(shell ls -1d $(BUILD)/lib/libffi/out/lib/libffi-*/include)
 ifeq ($(MICROPY_FORCE_32BIT),1)
  LIBFFI_LDFLAGS_MOD = $(BUILD)/lib/libffi/out/lib32/libffi.a
 else
  LIBFFI_LDFLAGS_MOD = $(BUILD)/lib/libffi/out/lib/libffi.a
 endif
else
LIBFFI_CFLAGS_MOD := $(shell pkg-config --cflags libffi)
LIBFFI_LDFLAGS_MOD := $(shell pkg-config --libs libffi)
endif

ifeq ($(UNAME_S),Linux)
LIBFFI_LDFLAGS_MOD += -ldl
endif

CFLAGS_MOD += $(LIBFFI_CFLAGS_MOD) -DMICROPY_PY_FFI=1
LDFLAGS_MOD += $(LIBFFI_LDFLAGS_MOD)
SRC_MOD += variants/standard/modffi.c
endif

ifeq ($(MICROPY_PY_JNI),1)
# Path for 64-bit OpenJDK, should be adjusted for other JDKs
CFLAGS_MOD += -I/usr/lib/jvm/java-7-openjdk-amd64/include -DMICROPY_PY_JNI=1
SRC_MOD += variants/standard/modjni.c
endif

deplibs: libffi axtls

libffi: $(BUILD)/lib/libffi/include/ffi.h

$(TOP)/lib/libffi/configure: $(TOP)/lib/libffi/autogen.sh
	cd $(TOP)/lib/libffi; ./autogen.sh

# install-exec-recursive & install-data-am targets are used to avoid building
# docs and depending on makeinfo
$(BUILD)/lib/libffi/include/ffi.h: $(TOP)/lib/libffi/configure
	mkdir -p $(BUILD)/lib/libffi; cd $(BUILD)/lib/libffi; \
	$(abspath $(TOP))/lib/libffi/configure $(CROSS_COMPILE_HOST) --prefix=$$PWD/out --disable-structs CC="$(CC)" CXX="$(CXX)" LD="$(LD)" CFLAGS="-Os -fomit-frame-pointer -fstrict-aliasing -ffast-math -fno-exceptions"; \
	$(MAKE) install-exec-recursive; $(MAKE) -C include install-data-am

axtls: $(TOP)/lib/axtls/README

$(TOP)/lib/axtls/README:
	@echo "You cloned without --recursive, fetching submodules for you."
	(cd $(TOP); git submodule update --init --recursive)
