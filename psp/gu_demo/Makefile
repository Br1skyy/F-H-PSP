TARGET = fh_demo
OBJS = main.o render.o input.o map_runtime.o interp_rt.o text_rt.o player_rt.o \
       d_a1.o d_b.o d_e.o d_ib.o d_d.o \
       d_a1c.o d_bc.o d_ec.o d_ibc.o d_dc.o \
       d_layers.o d_pass.o d_high.o \
       d_merc.o d_mercc.o d_outl.o d_outlc.o \
       d_priest.o d_priestc.o d_knight.o d_knightc.o

INCDIR =
CFLAGS = -Wall -O2 -G0
CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti
ASFLAGS = $(CFLAGS)

LIBDIR =
LDFLAGS =
LIBS = -lpspgu -lpspctrl -lpspdisplay -lpspge -lpsprtc -lpspdebug -lm

EXTRA_TARGETS = EBOOT.PBP
PSP_EBOOT_TITLE = F&H Port
PSP_EBOOT_ICON = NULL

PSPSDK = $(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

# We use no networking: drop the SDK's default net libs. They drag in
# kernel-only imports (sceNetInet, SysclibForKernel, ...) which a user-mode
# homebrew must not carry — PPSSPP tolerates them, real hardware does not.
LIBS := $(filter-out -lpspnet -lpspnet_apctl,$(LIBS))

# Embedded demo data: single-file EBOOT (no MS file I/O). Regenerate on
# data change (make clean && make). Full game will stream from MS (§7).
d_a1.o: data/map030/Mines_A1.t8
	bin2o -i data/map030/Mines_A1.t8 d_a1.o d_a1
d_b.o: data/map030/Mines_B.t8
	bin2o -i data/map030/Mines_B.t8 d_b.o d_b
d_e.o: data/map030/Mines_E.t8
	bin2o -i data/map030/Mines_E.t8 d_e.o d_e
d_ib.o: data/map030/Inside_B.t8
	bin2o -i data/map030/Inside_B.t8 d_ib.o d_ib
d_d.o: data/map030/Mines_D.t8
	bin2o -i data/map030/Mines_D.t8 d_d.o d_d
d_a1c.o: data/map030/Mines_A1.clut
	bin2o -i data/map030/Mines_A1.clut d_a1c.o d_a1c
d_bc.o: data/map030/Mines_B.clut
	bin2o -i data/map030/Mines_B.clut d_bc.o d_bc
d_ec.o: data/map030/Mines_E.clut
	bin2o -i data/map030/Mines_E.clut d_ec.o d_ec
d_ibc.o: data/map030/Inside_B.clut
	bin2o -i data/map030/Inside_B.clut d_ibc.o d_ibc
d_dc.o: data/map030/Mines_D.clut
	bin2o -i data/map030/Mines_D.clut d_dc.o d_dc
d_layers.o: data/map030/layers.bin
	bin2o -i data/map030/layers.bin d_layers.o d_layers
d_pass.o: data/map030/Map030.bin
	bin2o -i data/map030/Map030.bin d_pass.o d_pass
d_high.o: data/map030/higher.bin
	bin2o -i data/map030/higher.bin d_high.o d_high

# Character sprites
d_merc.o: data/mercenary.t8
	bin2o -i data/mercenary.t8 d_merc.o d_merc
d_mercc.o: data/mercenary.clut
	bin2o -i data/mercenary.clut d_mercc.o d_mercc
d_outl.o: data/outlander.t8
	bin2o -i data/outlander.t8 d_outl.o d_outl
d_outlc.o: data/outlander.clut
	bin2o -i data/outlander.clut d_outlc.o d_outlc
d_priest.o: data/dark_priest.t8
	bin2o -i data/dark_priest.t8 d_priest.o d_priest
d_priestc.o: data/dark_priest.clut
	bin2o -i data/dark_priest.clut d_priestc.o d_priestc
d_knight.o: data/knight.t8
	bin2o -i data/knight.t8 d_knight.o d_knight
d_knightc.o: data/knight.clut
	bin2o -i data/knight.clut d_knightc.o d_knightc

# Runtime modules with proper include paths
player_rt.o: ../../runtime/player.c
	$(CC) $(CFLAGS) -I../../runtime -c $< -o $@
