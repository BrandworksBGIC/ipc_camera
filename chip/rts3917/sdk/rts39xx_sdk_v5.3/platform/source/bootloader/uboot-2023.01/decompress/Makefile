INC := ../include -I../arch/arm/include
LDS := init.lds
OBJS := init.o
#OBJS += ../lib/lzma/LzmaDec.o
#OBJS += ../lib/lzma/LzmaTools.o

OBJS += ../arch/arm/lib/memset.o
OBJS += ../arch/arm/lib/memcpy.o
OBJS += ../common/malloc_simple.o
OBJS += ../common/dlmalloc_dec.o
OBJS += decompress_lzma.o
OBJS += ../lib/built-in.o

DECOMPRESS_BASE ?= 0x82800000
LOADBASE ?= 0x80800000
PTR_COUNT_SHIFT ?=2

CROSS_COMPILE ?= arm-linux-

CC := $(CROSS_COMPILE)gcc
LD := $(CROSS_COMPILE)ld
OBJCPY := $(CROSS_COMPILE)objcopy

GCCINC := $(shell $(CC) -print-file-name=include)
GCCLIB := $(shell dirname `$(CC) -print-libgcc-file-name`)

GENLDSFLAGS := -I$(INC) -DCONFIG_LZMA -DCONFIG_LOAD_BASE=$(LOADBASE) -DPTR_COUNT_SHIFT=$(PTR_COUNT_SHIFT) -ansi -E -P -

CFLAGS := -Os -ffunction-sections -fdata-sections -D__KERNEL__ \
	-DCONFIG_RAM_SIZE_COMP=$(RAM_VERSION_SIZE_COMP) \
	-DCONFIG_DELZMA_FUNC_BASE=$(DELZMA_FUNC_BASE) \
	-DCONFIG_DELZMA_FUNC_SIZE=$(DELZMA_FUNC_SIZE) \
	-DCONFIG_LOAD_BASE=$(LOADBASE) -I$(INC) -fno-builtin \
	-ffreestanding -nostdinc -isystem $(GCCINC) -pipe \
	-std=gnu11 -fshort-wchar -fno-strict-aliasing -fno-PIE \
	-fno-stack-protector -fno-delete-null-pointer-checks \
	-fmacro-prefix-map=./= -g -fstack-usage -Wno-format-nonliteral \
	-Wno-address-of-packed-member -Werror=date-time -D__ARM__ \
	-marm -mno-thumb-interwork -mabi=aapcs-linux -mword-relocations \
	-fno-pic -mno-unaligned-access -ffunction-sections -fdata-sections \
	-fno-common -ffixed-r9 -msoft-float -pipe -march=armv7-a \
	-D__LINUX_ARM_ARCH__=7 -mtune=generic-armv7-a \
	-fPIC -EL -Wstrict-prototypes

ASFLAGS := $(CFLAGS) -D__ASSEMBLY__

# LDFLAGS := -G 0 -static -n -nostdlib -EL -m elf32ltsmip -T $(LDS) \
# 	--gc-sections -pie -Bstatic -Ttext $(DECOMPRESS_BASE) -L $(GCCLIB) -lgcc

LDFLAGS := -T $(LDS) --gc-sections -pie -Bstatic --no-dynamic-linker \
		-Ttext $(DECOMPRESS_BASE)

OBJFLAGS := --remove-section=.dynsym --gap-fill=0xff -O binary

.PHONY : all
all : $(LDS) decompress.bin
# $(LDS) : include/$(LDS)
# 	$(CC) $(GENLDSFLAGS) < include/$(LDS) > $(LDS)
# .depend :
# 	$(CC) $(CFLAGS) -MM $(SRC) > .depend
# -include .depend

../common/dlmalloc_dec.o : ../common/dlmalloc.c
	$(CC) $(CFLAGS) -DSELF_EXTRACT_BUILD -c ../common/dlmalloc.c -o ../common/dlmalloc_dec.o

decompress_lzma.o : decompress_lzma.c
	$(CC) $(CFLAGS) -c decompress_lzma.c -o decompress_lzma.o
init.o : init.S
	$(CC) $(ASFLAGS) -c init.S -o init.o
decompress : $(OBJS) $(LDS)
	$(LD) $(LDFLAGS) $(OBJS) -Map decompress.map -o decompress -L $(GCCLIB) -lgcc
decompress.bin : decompress
	$(OBJCPY) $(OBJFLAGS) decompress decompress.bin

.PHONY : clean
clean :
	rm -f *.o *.su .depend decompress.map decompress *.bin
	rm -f $(OBJS)
