# [Micropython] Include mkenv.mk
include ../../py/mkenv.mk

# [MRE] Build tools
CROSS = 1
PYTHON = python3
OBJCOPY = objcopy
SDK = mre-makefile/sdk

# [Micropython] qstr definitions (must come before including py.mk)
QSTR_DEFS = qstrdefsport.h

# [MicroPython] Feature configurations
MICROPY_ROM_TEXT_COMPRESSION ?= 1

# [Micropython] Include py core make definitions
include $(TOP)/py/py.mk

# [MRE & Micropython] Compilers
CROSS_COMPILE = arm-none-eabi-
GCC = $(CROSS_COMPILE)gcc
GPP = $(CROSS_COMPILE)g++

# [MRE & Micropython] Include
INC += -I.
INC += -I$(TOP)
INC += -I$(BUILD)
INC += -I "$(SDK)/include" -I "mre-makefile/src/include" -I "mre-makefile/src/ResID" -I "src" # [MRE]

# [MRE] Defines
DEFINES = -D _MINIGUI_LIB_ -D _USE_MINIGUIENTRY -D _NOUNIX_ -D _FOR_WNC -D __MRE_SDK__ -D __MRE_VENUS_NORMAL__ -D __MMI_MAINLCD_240X320__ -D MRE -D GCC -D __MRE_COMPILER_GCC__

# [MRE & Micropython] Compiler flags
SHARED_PARAM = -c -fpic -mcpu=arm7tdmi-s -fvisibility=hidden -Os -mlittle-endian -mthumb $(INC) $(DEFINES) -fdata-sections -ffunction-sections -Wdouble-promotion -Wfloat-conversion $(COPT)
CFLAGS += $(SHARED_PARAM) -std=c99
CXXFLAGS += $(SHARED_PARAM) -fno-threadsafe-statics -fno-exceptions -fno-non-call-exceptions $(filter-out -std=c99,$(CFLAGS))
LD = $(GCC)

# Tune for Debugging or Optimization
#CFLAGS += -g  # always include debug info in the ELF
ifeq ($(DEBUG), 1)
CFLAGS += -O0
else
CFLAGS += -Os -DNDEBUG
CFLAGS += -fdata-sections -ffunction-sections
endif

LINK_LIB = -lm -lstdc++ $(SDK)/lib/*.a 
LDFLAGS = -fno-threadsafe-statics -Wl,--gc-sections -fpic -fpcc-struct-return --disable-libstdcxx-verbose -pie -T "$(SDK)/scat.ld"

# [MRE & Micropython] Sources
LIBS = $(LINK_LIB)

SRC_C = \
	upymain.c \
	uart_core.c \
	shared/libc/printf.c \
	shared/readline/readline.c \
	shared/runtime/pyexec.c \
	shared/runtime/stdout_helpers.c \
	$(BUILD)/_frozen_mpy.c \
	mre-makefile/src/c_fix.c \
	mre-makefile/src/gccmain.c \

SRC_CXX = \
	main.cpp \
	mre-makefile/src/cpp_fix.cpp \
	console/Console_io.cpp \
	console/Console.cpp \
	console/T2Input.cpp \

SRC_QSTR += shared/readline/readline.c shared/runtime/pyexec.c

OBJ += $(PY_CORE_O)
OBJ += $(addprefix $(BUILD)/, $(SRC_C:.c=.o))
OBJ += $(addprefix $(BUILD)/, $(SRC_CXX:.cpp=.o))

all: build/mrepl.vxp

# [Micropython] Freeze bytecode
$(BUILD)/_frozen_mpy.c: $(TOP)/tests/frozen/frozentest.mpy $(BUILD)/genhdr/qstrdefs.generated.h
	$(ECHO) "MISC freezing bytecode"
	$(Q)$(TOP)/tools/mpy-tool.py -f -q $(BUILD)/genhdr/qstrdefs.preprocessed.h -mlongint-impl=none $< > $@

# [MRE & Micropython] Compile and link source files
build/mrepl.axf: $(OBJ)
	$(ECHO) "LINK $@"
	$(Q)$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)
	$(Q)$(SIZE) $@

# [MRE] Add MRE resources and tags
build/mrepl.vxp: build/mrepl.axf
	$(OBJCOPY) -I elf32-little --add-section .vm_res=$(SDK)/resource.bin build/mrepl.axf build/mrepl.vxp
	$(PYTHON) $(SDK)/build.py $^ $@

# [Micropython]
include $(TOP)/py/mkrules.mk
