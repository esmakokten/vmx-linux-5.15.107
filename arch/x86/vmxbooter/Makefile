CC := gcc -m64
LD := ld -m elf_x86_64 --nmagic
AS := as --64 -g

INCPATH := -I. -I../../../include -I../include/uapi -I../../../include/uapi -I../include/generated/uapi 
INCS    := 
CFLAGS  := $(ARCH_CFLAGS) -g3 -O3 -ffreestanding -Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable -nostdinc -nostdlib -fno-pic -mno-red-zone -mcmodel=large -mno-sse -mno-sse2 -mgeneral-regs-only $(INCS)
LDFLAGS := -nostdlib -fno-builtin -nostartfiles -nostdinc -nodefaultlibs

KERNEL := kernel.img

WARNINGS += -Wall
WARNINGS += -Wcast-align
WARNINGS += -Wformat=2
WARNINGS += -Winit-self
WARNINGS += -Wmissing-declarations
WARNINGS += -Wmissing-prototypes
WARNINGS += -Wnested-externs
WARNINGS += -Wno-system-headers
WARNINGS += -Wold-style-definition
WARNINGS += -Wredundant-decls
WARNINGS += -Wsign-compare
WARNINGS += -Wstrict-prototypes
WARNINGS += -Wundef
WARNINGS += -Wvolatile-register-var
WARNINGS += -Wwrite-strings
WARNINGS += -Wno-address-of-packed-member

CFLAGS += $(WARNINGS)

KERNEL_CFILES += kmain.c

OBJS := $(KERNEL_CFILES:%.c=%.o)
DEPS := $(KERNEL_CFILES:%.c=%.d)

.PHONY: all build_kernel

all: $(KERNEL)

build_kernel:
	cd ../../.. && make -j12 && cd arch/x86/vmxbooter/ ;

$(KERNEL): build_kernel linker.ld $(OBJS) loader.o 
	$(LD) -T linker.ld loader.o guest_img.o $(OBJS) -o $@
	objcopy -O binary -R .note -R .comment -S $@ vmlinux.img

vmlinux.bin: ../boot/vmlinux.bin
	cp ../boot/vmlinux.bin vmlinux.bin

loader.o: loader.S guest_img.S vmlinux.bin
	$(info |     [AS]   Assembling $@)
	@$(CC) -c -I$(INCPATH) loader.S
	@$(CC) -c -I$(INCPATH) guest_img.S

define COMPILE_OBJ_RULE
	$(info |     [CC]   Compiling $@)
	$(CC) $(INCPATH) $(CFLAGS) -c $< -o $@
endef

define COMPILE_DEP_RULE
	@set +e; rm -f $@ $@.*; \
	$(CC) -M $(CFLAGS) $< > $@.$$$$ 2>/dev/null ; \
	if [ -s $@.$$$$ ]; then \
		sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	fi; \
	rm -f $@.$$$$
endef

%.o: %.c ; $(COMPILE_OBJ_RULE)

clean:
	@rm -f $(OBJS) $(DEPS) *.o $(KERNEL) kernel.iso vmlinux.bin
	@rm -rf iso

num_sockets=1
num_cores=1
num_threads=1
vcpus=$(shell echo $$(($(num_sockets)*$(num_cores)*$(num_threads))))

gdb:
	gdb -x ./gdbinit
run: all
	./build_iso.sh kernel.img
	qemu-system-x86_64 -cpu max -smp $(vcpus),cores=$(num_cores),threads=$(num_threads),sockets=$(num_sockets) -m  128 -cdrom kernel.iso -no-reboot -s -nographic -netdev type=tap,id=net0,ifname=tap0,script=no,downscript=no -device virtio-net-pci,netdev=net0,mac=10:10:10:10:10:11
debug: all
	./build_iso.sh kernel.img
	qemu-system-x86_64 -cpu max -smp $(vcpus),cores=$(num_cores),threads=$(num_threads),sockets=$(num_sockets) -m 128 -cdrom kernel.iso -no-reboot -s -S -nographic 
bzimage: all
	./build_iso.sh kernel.img
	qemu-system-x86_64 -cpu max -smp $(vcpus),cores=$(num_cores),threads=$(num_threads),sockets=$(num_sockets) -m 128 -no-reboot -s  -nographic -append "console=ttyS0,115200 nokaslr" -kernel ../boot/bzImage
objdump:
	objdump -S ../../../vmlinux | less

-include $(DEPS)
