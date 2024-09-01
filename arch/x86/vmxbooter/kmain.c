#include <asm/bootparam.h>

#define NUM_CPU 1

struct acpi_table_rsdp {
	char signature[8];	/* ACPI signature, contains "RSD PTR " */
	__u8 checksum;		/* ACPI 1.0 checksum */
	char oem_id[6];	/* OEM identification */
	__u8 revision;		/* Must be (0) for ACPI 1.0 or (2) for ACPI 2.0+ */
	__u32 rsdt_physical_address;	/* 32-bit physical address of the RSDT */
	__u32 length;		/* Table length in bytes, including header (ACPI 2.0+) */
	__u64 xsdt_physical_address;	/* 64-bit physical address of the XSDT (ACPI 2.0+) */
	__u8 extended_checksum;	/* Checksum of entire table (ACPI 2.0+) */
	__u8 reserved[3];		/* Reserved, must be zero */
}__attribute__((packed));

struct acpi_table_header {
	char signature[4];	/* ASCII table signature */
	__u32 length;		/* Length of table in bytes, including this header */
	__u8 revision;		/* ACPI Specification minor version number */
	__u8 checksum;		/* To make sum of entire table == 0 */
	char oem_id[6];	/* ASCII OEM identification */
	char oem_table_id[8];	/* ASCII OEM table identification */
	__u32 oem_revision;	/* OEM revision number */
	char asl_compiler_id[4];	/* ASCII ASL compiler vendor ID */
	__u32 asl_compiler_revision;	/* ASL compiler version */
}__attribute__((packed));

struct acpi_table_rsdt {
	struct acpi_table_header header;	/* Common ACPI table header */
	__u32 table_offset_entry[1];	/* Array of pointers to ACPI tables */
}__attribute__((packed));

struct acpi_table_madt {
	struct acpi_table_header header;	/* Common ACPI table header */
	__u32 address;		/* Physical address of local APIC */
	__u32 flags;
}__attribute__((packed));

struct acpi_subtable_header {
	__u8 type;
	__u8 length;
}__attribute__((packed));
struct acpi_madt_local_apic {
	struct acpi_subtable_header header;
	__u8 processor_id;	/* ACPI processor id */
	__u8 id;			/* Processor's local APIC id */
	__u32 lapic_flags;
}__attribute__((packed));

struct acpi_madt_io_apic {
	struct acpi_subtable_header header;
	__u8 id;			/* I/O APIC ID */
	__u8 reserved;		/* reserved - must be zero */
	__u32 address;		/* APIC physical address */
	__u32 global_irq_base;	/* Global system interrupt where INTI lines start */
}__attribute__((packed));

struct acpi_madt {
	struct acpi_table_madt madt;
	struct acpi_madt_local_apic lapics[NUM_CPU];
	struct acpi_madt_io_apic ioapic;
} __attribute__((packed));

struct boot_params boot_params;
struct setup_data init_setup_data;

const char *cmd_line_str = "console=uart8250,io,0x3f8,115200n8 spectre_v2=off random.trust_cpu=on acpi_force_table_verification=true acpi.debug_level=ACPI_DEBUG";
// rng_core.default_quality=1000
void kmain(void);
extern char input_data, input_data_end;
/*
	0x0000'7FFFFFFFFFFF	user upper
	0xffff'800000000000	machine kernel base
	0xffff'ffff80000000	linux kernel base
	0xffff'ffff81000000	linux kernel base -> 16M base address


	ffff80000	000
	    |
	    v
1111 1111 1' 111 1111 10' 00 0000 000' 0 0000 0000'
0x1ff       ' 0x1fe
*/

#define IMAGE_VADDR_START (0xffffffff80000000UL)
#define _va(x) ((unsigned long)(x) + IMAGE_VADDR_START)
#define _pa(x) ((unsigned long)(x) - IMAGE_VADDR_START)

#define ACPI_START_PADDR (0xE0000)
struct acpi_table_rsdp *g_rsdp = (struct acpi_table_rsdp *)_va(ACPI_START_PADDR);
struct acpi_table_rsdt *g_rsdt = (struct acpi_table_rsdt *)_va(((char *)ACPI_START_PADDR + 64));
struct acpi_madt *g_madt = (struct acpi_madt *)_va(((char *)ACPI_START_PADDR + 128));

static __attribute__((unused))
void *memset(void *dst, int b,  unsigned long len)
{
	char *p = dst;

	while (len--)
		*(p++) = b;
	return dst;
}

static __attribute__((unused))
int memcmp(const void *s1, const void *s2, unsigned long n)
{
	size_t ofs = 0;
	int c1 = 0;

	while (ofs < n && !(c1 = ((unsigned char *)s1)[ofs] - ((unsigned char *)s2)[ofs])) {
		ofs++;
	}
	return c1;
}

static inline void *
memcpy(void *dst, const void *src, size_t count)
{
	const __u8 *s = (const __u8 *)src;
	__u8 *      d = (__u8 *)dst;

	for (; count != 0; count--) *d++ = *s++;

	return dst;
}


static __u8 compute_checksum(__u8 *buffer, __u32 length)
{
	__u8 *end = buffer + length;
	__u8 sum = 0;

	while (buffer < end)
		sum += *(buffer++);

	return sum;
}

void apic_init(struct acpi_table_rsdt *rsdt);
void acpi_init(struct acpi_table_rsdp *rsdp);

void apic_init(struct acpi_table_rsdt *rsdt)
{
	memset(rsdt, 0, sizeof(*rsdt));
	memset(g_madt, 0, sizeof(g_madt));

	memcpy(&rsdt->header.signature, "RSDT", 4);
	rsdt->header.length = sizeof(struct acpi_table_rsdt);
	rsdt->header.revision = 0;
	memcpy(&rsdt->header.oem_id, "QEMU", 4);
	rsdt->header.oem_revision = 0;
	rsdt->table_offset_entry[0] = _pa(g_madt);
	
	rsdt->header.checksum = 0x100 - compute_checksum((void *)rsdt, rsdt->header.length);

	struct acpi_table_madt *madt = (struct acpi_table_madt *)g_madt;

	memcpy(&madt->header.signature, "APIC", 4);
	madt->header.revision = 0;
	memcpy(&madt->header.oem_id, "QEMU", 4);
	madt->header.oem_revision = 0;
	madt->address = 0xFEE00000;
	madt->flags = 0;

	madt->header.length = sizeof(struct acpi_table_madt) + sizeof(struct acpi_madt_local_apic) * NUM_CPU + sizeof(struct acpi_madt_io_apic) ;

	for (int i = 0; i < NUM_CPU; i++) {
		struct acpi_madt_local_apic *lapic = &g_madt->lapics[i];
		lapic->header.type = 0;
		lapic->header.length = sizeof(struct acpi_madt_local_apic);
		lapic->processor_id = i;
		lapic->id = i;
		lapic->lapic_flags = 1;
	}

	struct acpi_madt_io_apic *ioapic = &g_madt->ioapic;
	ioapic->header.type = 1;
	ioapic->header.length = sizeof(struct acpi_madt_io_apic);
	ioapic->id = 0x03;
	/* set the last page of physical memory from VMM to be io apic page */
	ioapic->address = 0xFEC00000;
	ioapic->global_irq_base = 0;

	madt->header.checksum = 0x100 - compute_checksum((void *)madt, madt->header.length);

}

void acpi_init(struct acpi_table_rsdp *rsdp)
{
	memset(rsdp, 0, sizeof(*rsdp));
	memcpy(&rsdp->signature, "RSD PTR ", 8);
	memcpy(&rsdp->oem_id, "QEMU ", 5);

	rsdp->revision = 0;
	rsdp->rsdt_physical_address = (__u32)_pa(g_rsdt);

	rsdp->checksum = 0x100 - compute_checksum((void *)rsdp, sizeof(struct acpi_table_rsdp));

	apic_init(g_rsdt);
}

void kmain(void)
{

	char *vga_base = (char *)_va(0xb8000);
	char *vmlinux_base = (char *)_va(0x1000000);
	char *guest_pos = &input_data;
	char *guest_pos_end = &input_data_end;
	unsigned long image_sz = (unsigned long)(guest_pos_end - guest_pos);

	vmlinux_base = (char *)_va(0x900000);

	/* since the image is the last data section, make sure the vmlinux_base address is after this section */
	if (vmlinux_base < guest_pos) while (1); 

	/* copy image */
	while (image_sz > 0) {
		--image_sz;
		vmlinux_base[image_sz] = guest_pos[image_sz];
	}

	acpi_init(g_rsdp);

	memset(&boot_params, 0, sizeof(boot_params));
	vga_base[0] = '!';
	boot_params.sentinel = 0xff;
	boot_params.hdr.version = 0x0215;
	boot_params.hdr.vid_mode = 0xFFFF;
	boot_params.hdr.boot_flag = 0xAA55;
	((char *)(&boot_params.hdr.header))[0] = 'H';
	((char *)(&boot_params.hdr.header))[1] = 'd';
	((char *)(&boot_params.hdr.header))[2] = 'r';
	((char *)(&boot_params.hdr.header))[3] = 's';
	boot_params.hdr.type_of_loader = 0xFF;
	boot_params.hdr.loadflags = 1; // the protected-mode code is loaded at 0x100000
	boot_params.hdr.ramdisk_image = 0; //The 32-bit linear address of the initial ramdisk or ramfs. Leave at zero if there is no initial ramdisk/ramfs.
	boot_params.hdr.ramdisk_size = 0; // Size of the initial ramdisk or ramfs. Leave at zero if there is no initial ramdisk/ramfs.
	boot_params.hdr.heap_end_ptr = 0xe000 - 0x200;
	boot_params.hdr.loadflags |= 0x80; /* CAN_USE_HEAP */;
	boot_params.hdr.cmd_line_ptr = _pa(cmd_line_str);
	boot_params.hdr.cmdline_size = sizeof(*cmd_line_str);
	// init_setup_data.next = 0;
	// boot_params.hdr.setup_data = _pa(&init_setup_data);
	boot_params.hdr.init_size = 36*1024*1024;
	boot_params.hdr.setup_data = 0; //no setup data
	boot_params.e820_entries = 3;
	boot_params.e820_table[0].addr = 0; 
	boot_params.e820_table[0].size = 0x9e000;
	boot_params.e820_table[0].type = 1;

	boot_params.e820_table[1].addr = 0x9e000; 
	boot_params.e820_table[1].size = 0x62000;
	boot_params.e820_table[1].type = 2;

	boot_params.e820_table[2].addr = 0x100000; // begin at 1MB
	boot_params.e820_table[2].size = 60 * 1024 * 1024; // 60MB
	boot_params.e820_table[2].type = 1;

	asm volatile("mov %0, %%rsi;movabs $0x900200, %%rax; jmpq   *%%rax;"::"r"((unsigned long)_pa(&boot_params)));
	while (1)
	{
		/* code */
	}
}
