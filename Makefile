# single_chan_pkt_fwd
# Single Channel LoRaWAN Gateway

BUILD = build/
CC = gcc
CPP = g++
CFLAGS = -Wall -I include/ -I linklabs_1_5_0/modules/ifc_lib/inc/ -Wno-write-strings

#LIBS = -lwiringPi
LL_SRC = linklabs_1_5_0/modules/ifc_lib/src/ll_ifc.c \
	linklabs_1_5_0/modules/ifc_lib/src/ll_ifc_time_pc.c \
	linklabs_1_5_0/modules/ifc_lib/src/ifc_struct_defs.c \
	linklabs_1_5_0/modules/ifc_lib/src/ll_ifc_no_mac.c \
	linklabs_1_5_0/modules/ifc_lib/src/ll_ifc_symphony.c \
	linklabs_1_5_0/modules/ifc_lib/src/ll_ifc_utils.c \
	linklabs_1_5_0/modules/ifc_lib/src/ll_ifc_transport_pc.c \
	linklabs_1_5_0/nomac_test/src/nomac.c

LOG_SRC = 	linklabs_1_5_0/modules/embc/src/log.c \
	linklabs_1_5_0/modules/embc/src/dbc.c \
	linklabs_1_5_0/modules/embc/src/fifo.c \


SRC = single_chan_pkt_fwd.cpp base64.c $(LL_SRC)

OBJ := $(patsubst %.cpp,%.o,$(SRC))
OBJ := $(addprefix $(BUILD), $(patsubst %.c,%.o,$(OBJ)))

gw: $(OBJ)
	@echo Need $(OBJ)
	$(CPP) $(CFLAGS) $(LIBS) -o $@ $(OBJ)

# for C code
$(BUILD)%.o : %.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -fpermissive -std=gnu99 $(LIBS) -o $@ -c $<

# for C++ code
$(BUILD)%.o : %.cpp
	@mkdir -p $(@D)
	$(CPP) -std=c++11 $(CFLAGS) $(LIBS) -o $@ -c $<
