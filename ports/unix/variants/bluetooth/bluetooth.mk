# Makefile fragment that can be used by any variant to enable Bluetooth.

MICROPY_PY_BLUETOOTH ?= 1

ifeq ($(USE_NIMBLE_H4),1)
MICROPY_BLUETOOTH_NIMBLE ?= 1
endif


# If the variant enables it, enable modbluetooth.
ifeq ($(MICROPY_PY_BLUETOOTH),1)

# Add Unix bindings for modbluetooth (HCI, NimBLE, btstack, etc).
SRC_C += \
	$(wildcard variants/bluetooth/*.c)

HAVE_LIBUSB := $(shell (which pkg-config > /dev/null && pkg-config --exists libusb-1.0) 2>/dev/null && echo '1')

# Only one stack can be enabled.
ifeq ($(MICROPY_BLUETOOTH_NIMBLE),1)
ifeq ($(MICROPY_BLUETOOTH_BTSTACK),1)
$(error Cannot enable both NimBLE and BTstack at the same time)
endif
endif

# Default to btstack, but a variant (or make command line) can set NimBLE
# explicitly (which is always via H4 UART).
ifneq ($(MICROPY_BLUETOOTH_NIMBLE),1)
ifneq ($(MICROPY_BLUETOOTH_BTSTACK),1)
MICROPY_BLUETOOTH_BTSTACK ?= 1
endif
endif

CFLAGS_MOD += -DMICROPY_PY_BLUETOOTH=1
CFLAGS_MOD += -DMICROPY_PY_BLUETOOTH_ENABLE_CENTRAL_MODE=1

ifeq ($(MICROPY_BLUETOOTH_BTSTACK),1)

# Figure out which BTstack transport to use.
ifeq ($(MICROPY_BLUETOOTH_BTSTACK_H4),1)
ifeq ($(MICROPY_BLUETOOTH_BTSTACK_USB),1)
$(error Cannot enable BTstack support for USB and H4 UART at the same time)
endif
else
ifeq ($(HAVE_LIBUSB),1)
# Default to btstack-over-usb.
MICROPY_BLUETOOTH_BTSTACK_USB ?= 1
else
# Fallback to HCI controller via a H4 UART (e.g. Zephyr on nRF) over a /dev/tty serial port.
MICROPY_BLUETOOTH_BTSTACK_H4 ?= 1
endif
endif

# BTstack is enabled.
GIT_SUBMODULES += lib/btstack
include $(TOP)/extmod/btstack/btstack.mk

else

# NimBLE is enabled.
GIT_SUBMODULES += lib/mynewt-nimble
CFLAGS_MOD += -DMICROPY_PY_BLUETOOTH_ENABLE_L2CAP_CHANNELS=1
include $(TOP)/extmod/nimble/nimble.mk

endif

endif
