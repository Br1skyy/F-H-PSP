TARGET = fh_demo
OBJS = main.o render.o input.o map_runtime.o interp_rt.o text_rt.o player_rt.o battle_rt.o \
       d_a1.o d_b.o d_e.o d_ib.o d_d.o \
       d_a1c.o d_bc.o d_ec.o d_ibc.o d_dc.o \
       d_layers.o d_pass.o d_high.o \
       d_merc.o d_mercc.o d_outl.o d_outlc.o \
       d_priest.o d_priestc.o d_knight.o d_knightc.o \
       d_flame.o d_flamec.o d_creat.o d_creatc.o \
       d_mobj.o d_mobjc.o d_ghost.o d_ghostc.o d_guard1.o d_guard1c.o \
       d_font.o d_fontc.o d_fontadv.o \
       d_win.o d_winc.o d_floor.o d_floorc.o \
       d_tmerc.o d_tmercc.o d_toutl.o d_toutlc.o \
       d_tpriest.o d_tpriestc.o d_tknight.o d_tknightc.o \
       d_e0.o d_e0c.o d_e1.o d_e1c.o d_e2.o d_e2c.o d_e3.o d_e3c.o \
       d_e4.o d_e4c.o d_e5.o d_e5c.o d_e6.o d_e6c.o d_form.o \
       d_bv0.o d_bv0c.o d_bv1.o d_bv1c.o d_bv2.o d_bv2c.o d_bv3.o d_bv3c.o

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
d_flame.o: data/!Flame.t8
	bin2o -i "data/!Flame.t8" d_flame.o d_flame
d_flamec.o: data/!Flame.clut
	bin2o -i "data/!Flame.clut" d_flamec.o d_flamec
d_creat.o: data/!creature.t8
	bin2o -i "data/!creature.t8" d_creat.o d_creat
d_creatc.o: data/!creature.clut
	bin2o -i "data/!creature.clut" d_creatc.o d_creatc
d_mobj.o: data/!map_objects2.t8
	bin2o -i "data/!map_objects2.t8" d_mobj.o d_mobj
d_mobjc.o: data/!map_objects2.clut
	bin2o -i "data/!map_objects2.clut" d_mobjc.o d_mobjc
d_ghost.o: data/$$minerghost2.t8
	bin2o -i 'data/$$minerghost2.t8' d_ghost.o d_ghost
d_ghostc.o: data/$$minerghost2.clut
	bin2o -i 'data/$$minerghost2.clut' d_ghostc.o d_ghostc
d_guard1.o: data/guard1.t8
	bin2o -i data/guard1.t8 d_guard1.o d_guard1
d_guard1c.o: data/guard1.clut
	bin2o -i data/guard1.clut d_guard1c.o d_guard1c
d_font.o: data/font.t8
	bin2o -i data/font.t8 d_font.o d_font
d_fontc.o: data/font.clut
	bin2o -i data/font.clut d_fontc.o d_fontc
d_win.o: data/window.t8
	bin2o -i data/window.t8 d_win.o d_win
d_winc.o: data/window.clut
	bin2o -i data/window.clut d_winc.o d_winc
d_floor.o: data/floor1.t8
	bin2o -i data/floor1.t8 d_floor.o d_floor
d_floorc.o: data/floor1.clut
	bin2o -i data/floor1.clut d_floorc.o d_floorc
d_bv0.o: data/bv_Actor1_1.t8
	bin2o -i data/bv_Actor1_1.t8 d_bv0.o d_bv0
d_bv0c.o: data/bv_Actor1_1.clut
	bin2o -i data/bv_Actor1_1.clut d_bv0c.o d_bv0c
d_bv1.o: data/bv_knight1_1.t8
	bin2o -i data/bv_knight1_1.t8 d_bv1.o d_bv1
d_bv1c.o: data/bv_knight1_1.clut
	bin2o -i data/bv_knight1_1.clut d_bv1c.o d_bv1c
d_bv2.o: data/bv_darkpriest1_1.t8
	bin2o -i data/bv_darkpriest1_1.t8 d_bv2.o d_bv2
d_bv2c.o: data/bv_darkpriest1_1.clut
	bin2o -i data/bv_darkpriest1_1.clut d_bv2c.o d_bv2c
d_bv3.o: data/bv_outlander1_1.t8
	bin2o -i data/bv_outlander1_1.t8 d_bv3.o d_bv3
d_bv3c.o: data/bv_outlander1_1.clut
	bin2o -i data/bv_outlander1_1.clut d_bv3c.o d_bv3c
d_fontadv.o: data/font_adv.bin
	bin2o -i data/font_adv.bin d_fontadv.o d_fontadv
d_tmerc.o: data/mercenary_torch.t8
	bin2o -i data/mercenary_torch.t8 d_tmerc.o d_tmerc
d_tmercc.o: data/mercenary_torch.clut
	bin2o -i data/mercenary_torch.clut d_tmercc.o d_tmercc
d_toutl.o: data/outlander_torch.t8
	bin2o -i data/outlander_torch.t8 d_toutl.o d_toutl
d_toutlc.o: data/outlander_torch.clut
	bin2o -i data/outlander_torch.clut d_toutlc.o d_toutlc
d_tpriest.o: data/dark_priest_torch.t8
	bin2o -i data/dark_priest_torch.t8 d_tpriest.o d_tpriest
d_tpriestc.o: data/dark_priest_torch.clut
	bin2o -i data/dark_priest_torch.clut d_tpriestc.o d_tpriestc
d_tknight.o: data/knight_torch.t8
	bin2o -i data/knight_torch.t8 d_tknight.o d_tknight
d_tknightc.o: data/knight_torch.clut
	bin2o -i data/knight_torch.clut d_tknightc.o d_tknightc
d_e0.o: data/guard1_torso.t8
	bin2o -i data/guard1_torso.t8 d_e0.o d_e0
d_e0c.o: data/guard1_torso.clut
	bin2o -i data/guard1_torso.clut d_e0c.o d_e0c
d_e1.o: data/guard1_head.t8
	bin2o -i data/guard1_head.t8 d_e1.o d_e1
d_e1c.o: data/guard1_head.clut
	bin2o -i data/guard1_head.clut d_e1c.o d_e1c
d_e2.o: data/guard1_R_hand.t8
	bin2o -i data/guard1_R_hand.t8 d_e2.o d_e2
d_e2c.o: data/guard1_R_hand.clut
	bin2o -i data/guard1_R_hand.clut d_e2c.o d_e2c
d_e3.o: data/guard1_L_hand.t8
	bin2o -i data/guard1_L_hand.t8 d_e3.o d_e3
d_e3c.o: data/guard1_L_hand.clut
	bin2o -i data/guard1_L_hand.clut d_e3c.o d_e3c
d_e4.o: data/guard1_R_leg.t8
	bin2o -i data/guard1_R_leg.t8 d_e4.o d_e4
d_e4c.o: data/guard1_R_leg.clut
	bin2o -i data/guard1_R_leg.clut d_e4c.o d_e4c
d_e5.o: data/guard1_L_leg.t8
	bin2o -i data/guard1_L_leg.t8 d_e5.o d_e5
d_e5c.o: data/guard1_L_leg.clut
	bin2o -i data/guard1_L_leg.clut d_e5c.o d_e5c
d_e6.o: data/guard1_stinger.t8
	bin2o -i data/guard1_stinger.t8 d_e6.o d_e6
d_e6c.o: data/guard1_stinger.clut
	bin2o -i data/guard1_stinger.clut d_e6c.o d_e6c
d_form.o: ../../converted/code/formulas.bin
	bin2o -i ../../converted/code/formulas.bin d_form.o d_form

# Runtime modules with proper include paths
player_rt.o: ../../runtime/player.c
	$(CC) $(CFLAGS) -I../../runtime -c $< -o $@
battle_rt.o: ../../runtime/battle.c
	$(CC) $(CFLAGS) -I../../runtime -c $< -o $@
