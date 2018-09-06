####################################################################
# Makefile                                                         #
####################################################################

# These targets do not create files, so they are phony targets
.PHONY: all debug release clean

####################################################################
# Definitions                                                      #
####################################################################

PROJECTNAME = nomac_test
BUILD_NUMBER_FILE = build-number.txt

ROOT_DIR = ../..
MODULE_DIR = $(ROOT_DIR)/modules

OBJ_DIR = build
EXE_DIR = exe
LST_DIR = lst

####################################################################
# Definitions of toolchain.                                        #
# You might need to do changes to match your system setup          #
####################################################################

# Change path to the tools according to your system configuration
# DO NOT add trailing whitespace chars, they do matter !

RMDIRS     := rm -rf
RMFILES    := rm -rf
ALLFILES   := /*.*
NULLDEVICE := /dev/null
SHELLNAMES := $(ComSpec)$(COMSPEC)

# Create directories and do a clean which is compatible with parallell make
$(shell mkdir $(OBJ_DIR)>$(NULLDEVICE) 2>&1)
$(shell mkdir $(EXE_DIR)>$(NULLDEVICE) 2>&1)
$(shell mkdir $(LST_DIR)>$(NULLDEVICE) 2>&1)
ifeq (clean,$(findstring clean, $(MAKECMDGOALS)))
  ifneq ($(filter $(MAKECMDGOALS),all debug release),)
    $(shell $(RMFILES) $(OBJ_DIR)$(ALLFILES)>$(NULLDEVICE) 2>&1)
    $(shell $(RMFILES) $(EXE_DIR)$(ALLFILES)>$(NULLDEVICE) 2>&1)
    $(shell $(RMFILES) $(LST_DIR)$(ALLFILES)>$(NULLDEVICE) 2>&1)
  endif
endif


####################################################################
# Flags                                                            #
####################################################################

# -MMD : Don't generate dependencies on system header files.
# -MP  : Add phony targets, useful when a h-file is removed from a project.
# -MF  : Specify a file to write the dependencies to.
DEPFLAGS = -MMD -MP -MF $(@:.o=.d)

#
# Add -Wa,-ahld=$(LST_DIR)/$(@F:.o=.lst) to CFLAGS to produce assembly list files
#
override CFLAGS += -Wall -Wextra -std=gnu99 -pedantic \
-ffunction-sections -fdata-sections \
$(DEPFLAGS) \
$(BUILD_NUMBER_CFLAGS)

override ASMFLAGS += -x assembler-with-cpp -Wall -Wextra

#
# NOTE: The -Wl,--gc-sections flag may interfere with debugging using gdb.
#
#override LDFLAGS += -Wl,--gc-sections
#LDFLAGS += -Xlinker -Map=$(LST_DIR)/$(PROJECTNAME).map

#LIBS = -Wl,--start-group -lgcc -lc -lnosys   -Wl,--end-group

INCLUDEPATHS += \
-I. \
-I$(MODULE_DIR)/ifc_lib/inc

CTAGS_INCLUDEPATHS = $(subst -I,,$(INCLUDEPATHS))

####################################################################
# Files                                                            #
####################################################################
C_SRC +=  \
$(MODULE_DIR)/ifc_lib/src/ll_ifc.c \
$(MODULE_DIR)/ifc_lib/src/ll_ifc_no_mac.c \
$(MODULE_DIR)/ifc_lib/src/ifc_struct_defs.c \
$(MODULE_DIR)/ifc_lib/src/ll_ifc_time_pc.c \
$(MODULE_DIR)/ifc_lib/src/ll_ifc_transport_pc.c \
src/main.c

s_SRC +=

S_SRC +=

####################################################################
# Rules                                                            #
####################################################################

C_FILES = $(notdir $(C_SRC) )
S_FILES = $(notdir $(S_SRC) $(s_SRC) )
#make list of source paths, sort also removes duplicates
C_PATHS = $(sort $(dir $(C_SRC) ) )
S_PATHS = $(sort $(dir $(S_SRC) $(s_SRC) ) )

C_OBJS = $(addprefix $(OBJ_DIR)/, $(C_FILES:.c=.o))
S_OBJS = $(if $(S_SRC), $(addprefix $(OBJ_DIR)/, $(S_FILES:.S=.o)))
s_OBJS = $(if $(s_SRC), $(addprefix $(OBJ_DIR)/, $(S_FILES:.s=.o)))
C_DEPS = $(addprefix $(OBJ_DIR)/, $(C_FILES:.c=.d))
OBJS = $(C_OBJS) $(S_OBJS) $(s_OBJS)

vpath %.c $(C_PATHS)
vpath %.s $(S_PATHS)
vpath %.S $(S_PATHS)

# Default build is debug build
all:      debug

debug:    CFLAGS += -DDEBUG -O0 -g
debug:    $(EXE_DIR)/$(PROJECTNAME)

release:  CFLAGS += -DNDEBUG -O3 -g
release:  $(EXE_DIR)/$(PROJECTNAME)

# Create objects from C SRC files
$(OBJ_DIR)/%.o: %.c
	@echo "Building file: $<"
	$(CC) $(CFLAGS) $(INCLUDEPATHS) -c -o $@ $<

# Assemble .s/.S files
$(OBJ_DIR)/%.o: %.s
	@echo "Assembling $<"
	$(CC) $(ASMFLAGS) $(INCLUDEPATHS) -c -o $@ $<

$(OBJ_DIR)/%.o: %.S
	@echo "Assembling $<"
	$(CC) $(ASMFLAGS) $(INCLUDEPATHS) -c -o $@ $<

# Link
$(EXE_DIR)/$(PROJECTNAME): $(OBJS) $(BUILD_NUMBER_FILE)
	@echo "Linking target: $@"
	$(CC) $(LDFLAGS) $(OBJS) $(LIBS) -o $(EXE_DIR)/$(PROJECTNAME)

clean:
ifeq ($(filter $(MAKECMDGOALS),all debug release),)
	$(RMDIRS) $(OBJ_DIR) $(LST_DIR) $(EXE_DIR)
endif

# include auto-generated dependency files (explicit rules)
ifneq (clean,$(findstring clean, $(MAKECMDGOALS)))
-include $(C_DEPS)
endif

ctags:
	ctags -R $(C_PATHS) $(S_PATHS) $(CTAGS_INCLUDEPATHS)

print-% : ; @echo $* = $($*)    
    
include ../../util/buildnumber.mak
