ifeq ($(USE_NIMBLE_H4),1)
PROG_SUFFIX = "-nimble-h4"
else
PROG_SUFFIX = ""
endif

PROG ?= micropython-bluetooth${PROG_SUFFIX}

include variants/standard/mpconfigvariant.mk
include variants/bluetooth/bluetooth.mk
