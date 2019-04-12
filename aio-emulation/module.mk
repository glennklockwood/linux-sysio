ESRCS=aio-emulation/listio.c

if WITH_EMULATION
OPTIONAL_ESRCS = $(ESRCS)
else
OPTIONAL_ESRCS =
endif

