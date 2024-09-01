#include "multiboot2.h"
.section .text
/* 
 * grub enters 32-bit mode, so your first section should use 32-bit code.
 * jmp to 64 bit yourself from here.
 */
.code32
.global loader 
loader:
	/* save multiboot2 args first */
	mov %eax, multiboot_magic
	mov %ebx, multiboot_tags

	/* set up a interim stack */
	mov $0x2000, %esp
	/* prepare to enter long(64-bit) mode */

	/* close paging first */
	mov %cr0, %eax
	and $(~(1<<31)), %eax
	mov %eax, %cr0

	/* Assuming huge is supported and thus directly jmp to the lable, needs to fix this after VM supports cpuid */
	jmp huge_page_enabled
	/* test if 1G huge page is supported */
	/* some cpus don't support 1G huge page, we enable this by set qemu -cpu max */
	mov $0x80000001, %eax
	cpuid
	bt $26, %edx
	jc huge_page_enabled
	/* TODO: support for cpus which don't have 1G paging scheme here */
	jmp .

huge_page_enabled:
	/* we have to set up a temparaily page scheme to before enter long mode */
	/* we use 1G paging scheme, so only to set p0_pgt, p1_pgt */
	mov $p1_pgt, %eax
	or $0x03, %eax
	mov %eax, p0_pgt
	mov %eax, (p0_pgt+0x08*0x1ff)
	
	/* set kernel paging space */
	mov $p1_pgt, %eax
	movl $0x00000083, (%eax) /* 0-1 GB vm -> 0-1 GB pm */
	add $8, %eax
	movl $0x40000083, (%eax) /* 1-2 GB vm -> 1-2 GB pm */
	add $8, %eax
	movl $0x80000083, (%eax) /* 2-3 GB vm -> 2-3 GB pm */
	add $8, %eax
	movl $0xC0000083, (%eax) /* 3-4 GB vm -> 3-4 GB pm */

	/* also set a 1G mapping for linux kernel's default virtual base address:0xffffffff80000000 */
	movl  $0x00000083, (p1_pgt+0x08*0x1fe) /* 0-1 GB vm -> 0-1 GB pm */

	/* set %cr3 to the physical address of p0_pgt */
	mov $p0_pgt, %eax
	mov %eax, %cr3

	/* enable PAE-paging by setting the PAE-bit in %cr4 */
	mov %cr4, %eax
	or $(1 << 5), %eax
	mov %eax, %cr4

	/* set the long mode bit in the EFER MSR */
	mov $0xC0000080, %ecx
	rdmsr
	or $(1 << 8), %eax
	wrmsr

	/* enable paging and protected mode in cr0 */
	mov %cr0, %eax
	or $(1 << 31), %eax
	mov %eax, %cr0

	/* load GDT so we can use selectors within them */
	mov $gdt_ptr, %eax
	lgdt (%eax)

	/* need a jump to flush cs selector */
.code64
	mov $entry_64, %rax
	pushq $0x08
	pushq %rax
	lretq

entry_64:
	cli

	/* flush data selectors */
	mov $0x10, %rax
	mov %rax, %ds
	mov %rax, %es
	mov %rax, %fs
	mov %rax, %gs
	mov %rax, %ss

	/* set up a kernel stack for kmain */
	movabs $(stack + STACKSIZE), %rsp
	xor %rsi, %rsi
	xor %rdi, %rdi
	movl multiboot_magic, %esi
	movl multiboot_tags, %edi

	movabs $kmain, %rax
	pushq $0x08
	pushq %rax
	lretq
	jmp .

/* multiboot2 header must be contained completely within the first 32768 bytes of the OS image, and must be 64-bit aligned */
.align 64
multiboot_start:
	/* must have multiboot2 header field */
	.long MULTIBOOT2_HEADER_MAGIC
	.long MULTIBOOT_ARCHITECTURE_I386
	.long multiboot_end - multiboot_start
	.long -(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT_ARCHITECTURE_I386 + (multiboot_end - multiboot_start))

	/* end tag of multiboot2 */
	.short MULTIBOOT_HEADER_TAG_END
	.short 0
	.long 8
multiboot_end:

.align 8
.global multiboot_magic
multiboot_magic:
	.fill 1, 4, 0
.global multiboot_tags
multiboot_tags:
	.fill 1, 4, 0

/* a simple temporary GDT */
.align 8
gdt_table:
	.quad	0x0000000000000000	// null descriptor
	.quad	0x0020980000000000	// kernel code descriptor
	.quad	0x0000920000000000	// kernel data descriptor
gdt_end:

.global gdt_ptr
gdt_ptr:
	.word	gdt_end - gdt_table - 1
	.quad	gdt_table

/* temporary page directory/tables */
/* need to be 4k aligned */
.align 4096
/* P0 is PGD */
.global p0_pgt
p0_pgt:
	.fill 512, 8, 0

.global p1_pgt
p1_pgt:
	.fill 512, 8, 0

.section .data
.set STACKSIZE, 0x1000
.global stack
.align 0x1000
stack:
.fill (STACKSIZE * 1), 1, 0
