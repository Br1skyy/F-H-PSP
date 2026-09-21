TARGET = fh_hello
OBJS = main.o

INCDIR =
CFLAGS = -Wall -O2 -G0
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =
LIBS = -lpspdebug -lpspdisplay -lpspkernel

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = FH Hello
PSP_EBOOT_ICON = NULL

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak
