#include "printf.h"
#include "memory.h"
#include "memory_map.h"

const char *mmap_type_str(mmap_type_t type) {
    switch (type) {
        case MMAP_FREE:     return "Free RAM";
        case MMAP_RESERVED: return "Reserved";
        case MMAP_ACPI:     return "ACPI Reclaimable";
        case MMAP_NVS:      return "ACPI NVS";
        case MMAP_BADRAM:   return "Bad RAM";
        case MMAP_KERNEL:   return "Kernel";
        default:            return "Unknown";
    }
}

void mmap_init(memory_map_t *mmap) {
    memset(mmap, 0, sizeof(*mmap));
}

void mmap_add_entry(memory_map_t *mmap, u64 base, u64 length, mmap_type_t type) {
    if (mmap->count >= MMAP_MAX_ENTRIES) {
        printf("[MMAP] ERROR: max entries (%d) reached!\n", MMAP_MAX_ENTRIES);
        return;
    }
    mmap->entries[mmap->count].base   = base;
    mmap->entries[mmap->count].length = length;
    mmap->entries[mmap->count].type   = type;
    mmap->count++;

    if (type == MMAP_FREE) {
        mmap->total_free   += length;
        mmap->total_usable += length;
    }
}

/*
 * mmap_parse - simulate parsing a BIOS e820 memory map.
 * In a real OS bootloader (GRUB/UEFI), this is handed to the kernel.
 * Here we simulate a typical x86 machine layout.
 */
void mmap_parse(memory_map_t *mmap) {
    mmap_init(mmap);

    /*
     * Typical x86 BIOS memory map (within our 64MB simulation):
     *
     * 0x00000000 - 0x0009FFFF  640 KB    Free (conventional memory)
     * 0x000A0000 - 0x000BFFFF  128 KB    VGA video RAM (reserved)
     * 0x000C0000 - 0x000FFFFF  256 KB    BIOS ROM / option ROMs (reserved)
     * 0x00100000 - 0x00EFFFFF   14 MB    Free (extended memory - kernel here)
     * 0x00F00000 - 0x00FFFFFF    1 MB    Reserved (BIOS shadow)
     * 0x01000000 - 0x027FFFFF   24 MB    Free
     * 0x02800000 - 0x028FFFFF  512 KB    ACPI tables
     * 0x02900000 - 0x03BFFFFF   18 MB    Free
     * 0x03C00000 - 0x03FFFFFF    4 MB    Reserved
     */
    mmap_add_entry(mmap, 0x00000000, 0x000A0000, MMAP_FREE);      /* 640 KB           */
    mmap_add_entry(mmap, 0x000A0000, 0x00020000, MMAP_RESERVED);  /* VGA              */
    mmap_add_entry(mmap, 0x000C0000, 0x00040000, MMAP_RESERVED);  /* BIOS ROM         */
    mmap_add_entry(mmap, 0x00100000, 0x00E00000, MMAP_FREE);      /* 14 MB extended   */
    mmap_add_entry(mmap, 0x00F00000, 0x00100000, MMAP_RESERVED);  /* BIOS shadow      */
    mmap_add_entry(mmap, 0x01000000, 0x01800000, MMAP_FREE);      /* 24 MB            */
    mmap_add_entry(mmap, 0x02800000, 0x00100000, MMAP_ACPI);      /* ACPI tables      */
    mmap_add_entry(mmap, 0x02900000, 0x01300000, MMAP_FREE);      /* 19 MB            */
    mmap_add_entry(mmap, 0x03C00000, 0x00400000, MMAP_RESERVED);  /* Reserved         */

    printf("[MMAP] Parsed %u memory map entries\n", mmap->count);
    printf("[MMAP] Total free: %llu MB\n",
           (unsigned long long)(mmap->total_free / (1024 * 1024)));
}

void mmap_print(const memory_map_t *mmap) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║              MEMORY MAP  (%2u entries)                       ║\n", mmap->count);
    printf("╠══════════════════╦══════════════════╦════════════════════════╣\n");
    printf("║ Base             ║ Length           ║ Type                   ║\n");
    printf("╠══════════════════╬══════════════════╬════════════════════════╣\n");

    for (u32 i = 0; i < mmap->count; i++) {
        printf("║ 0x%014llx ║ 0x%014llx ║ %-22s ║\n",
               (unsigned long long)mmap->entries[i].base,
               (unsigned long long)mmap->entries[i].length,
               mmap_type_str(mmap->entries[i].type));
    }

    printf("╠══════════════════╩══════════════════╩════════════════════════╣\n");
    printf("║ Total Free : %-48llu ║\n",
           (unsigned long long)mmap->total_free);
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
}
