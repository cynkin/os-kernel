#include "printf.h"
#include "memory.h"
#include "pmm.h"

/*
 * Simulated physical memory.
 * In a real OS this would be actual RAM.
 * Here it's a 64MB static array — the PMM manages
 * pages within this buffer using a bitmap.
 */
static u8 phys_mem[PHYS_MEM_SIZE];

/* ─── Physical address <-> pointer conversion ─────────────────────────── */

void *phys_to_virt(paddr_t addr) {
    if (addr >= PHYS_MEM_SIZE) {
        printf( "[PMM] phys_to_virt: addr 0x%lx out of range!\n",
                (unsigned long)addr);
        return NULL;
    }
    return (void *)(phys_mem + addr);
}

paddr_t virt_to_phys(void *ptr) {
    if ((u8 *)ptr < phys_mem || (u8 *)ptr >= phys_mem + PHYS_MEM_SIZE) {
        printf( "[PMM] virt_to_phys: pointer out of range!\n");
        return (paddr_t)-1;
    }
    return (paddr_t)((u8 *)ptr - phys_mem);
}

/* ─── Bitmap helpers ───────────────────────────────────────────────────── */

static inline void bitmap_set(u8 *bitmap, u64 bit) {
    bitmap[bit >> 3] |= (1u << (bit & 7));
}

static inline void bitmap_clear(u8 *bitmap, u64 bit) {
    bitmap[bit >> 3] &= ~(1u << (bit & 7));
}

static inline int bitmap_test(const u8 *bitmap, u64 bit) {
    return (bitmap[bit >> 3] >> (bit & 7)) & 1;
}

/* ─── PMM init ─────────────────────────────────────────────────────────── */

void pmm_init(pmm_t *pmm, const memory_map_t *mmap) {
    /* Mark everything as used (1) */
    memset(pmm->bitmap, 0xFF, PMM_BITMAP_BYTES);
    pmm->total_pages = 0;
    pmm->free_pages  = 0;
    pmm->used_pages  = 0;

    /* Walk memory map and free usable pages */
    for (u32 i = 0; i < mmap->count; i++) {
        if (mmap->entries[i].type != MMAP_FREE)
            continue;

        u64 base   = ALIGN_UP  (mmap->entries[i].base,   PAGE_SIZE);
        u64 top    = ALIGN_DOWN(mmap->entries[i].base + mmap->entries[i].length, PAGE_SIZE);

        /* Clamp to our simulated memory */
        if (base >= PHYS_MEM_SIZE) continue;
        if (top  >  PHYS_MEM_SIZE) top = PHYS_MEM_SIZE;

        u64 pages = (top - base) / PAGE_SIZE;
        u64 first = base / PAGE_SIZE;

        for (u64 p = 0; p < pages; p++) {
            bitmap_clear(pmm->bitmap, first + p);
            pmm->free_pages++;
            pmm->total_pages++;
        }
    }

    pmm->used_pages = 0;

    printf("[PMM] Initialized │ total=%llu pages │ free=%llu pages │ %llu MB free\n",
           (unsigned long long)pmm->total_pages,
           (unsigned long long)pmm->free_pages,
           (unsigned long long)(pmm->free_pages * PAGE_SIZE) / (1024 * 1024));
}

/* ─── Alloc single page ────────────────────────────────────────────────── */

paddr_t pmm_alloc(pmm_t *pmm) {
    if (pmm->free_pages == 0) {
        printf( "[PMM] pmm_alloc: OUT OF MEMORY!\n");
        return (paddr_t)-1;
    }

    /* Linear scan through bitmap */
    for (u64 i = 0; i < PMM_MAX_PAGES; i++) {
        if (!bitmap_test(pmm->bitmap, i)) {
            bitmap_set(pmm->bitmap, i);
            pmm->free_pages--;
            pmm->used_pages++;

            paddr_t addr = i * PAGE_SIZE;
            memset(phys_to_virt(addr), 0, PAGE_SIZE);   /* zero the page */
            return addr;
        }
    }

    printf( "[PMM] pmm_alloc: no free page found!\n");
    return (paddr_t)-1;
}

/* ─── Alloc n contiguous pages ─────────────────────────────────────────── */

paddr_t pmm_alloc_n(pmm_t *pmm, u64 n) {
    if (n == 0 || pmm->free_pages < n) {
        printf( "[PMM] pmm_alloc_n: not enough free pages (need %llu, have %llu)\n",
                (unsigned long long)n, (unsigned long long)pmm->free_pages);
        return (paddr_t)-1;
    }

    u64 run = 0, start = 0;

    for (u64 i = 0; i < PMM_MAX_PAGES; i++) {
        if (!bitmap_test(pmm->bitmap, i)) {
            if (run == 0) start = i;
            run++;
            if (run == n) {
                /* Found n contiguous free pages starting at 'start' */
                for (u64 j = start; j < start + n; j++) {
                    bitmap_set(pmm->bitmap, j);
                    pmm->free_pages--;
                    pmm->used_pages++;
                }
                paddr_t addr = start * PAGE_SIZE;
                memset(phys_to_virt(addr), 0, n * PAGE_SIZE);
                return addr;
            }
        } else {
            run = 0;   /* reset run on used page */
        }
    }

    printf( "[PMM] pmm_alloc_n: no contiguous block of %llu pages found!\n",
            (unsigned long long)n);
    return (paddr_t)-1;
}

/* ─── Free single page ─────────────────────────────────────────────────── */

void pmm_free(pmm_t *pmm, paddr_t addr) {
    if (addr % PAGE_SIZE != 0) {
        printf( "[PMM] pmm_free: addr 0x%lx not page-aligned!\n",
                (unsigned long)addr);
        return;
    }
    if (addr >= PHYS_MEM_SIZE) {
        printf( "[PMM] pmm_free: addr 0x%lx out of range!\n",
                (unsigned long)addr);
        return;
    }

    u64 page = addr / PAGE_SIZE;

    if (!bitmap_test(pmm->bitmap, page)) {
        printf( "[PMM] pmm_free: double-free at 0x%lx!\n",
                (unsigned long)addr);
        return;
    }

    bitmap_clear(pmm->bitmap, page);
    pmm->free_pages++;
    pmm->used_pages--;
}

/* ─── Free n pages ─────────────────────────────────────────────────────── */

void pmm_free_n(pmm_t *pmm, paddr_t addr, u64 n) {
    for (u64 i = 0; i < n; i++)
        pmm_free(pmm, addr + i * PAGE_SIZE);
}

/* ─── Query ────────────────────────────────────────────────────────────── */

int pmm_is_free(const pmm_t *pmm, paddr_t addr) {
    if (addr >= PHYS_MEM_SIZE) return 0;
    u64 page = addr / PAGE_SIZE;
    return !bitmap_test(pmm->bitmap, page);
}

/* ─── Stats ────────────────────────────────────────────────────────────── */

void pmm_print_stats(const pmm_t *pmm) {
    u64 free_mb  = (pmm->free_pages  * PAGE_SIZE) / (1024 * 1024);
    u64 used_mb  = (pmm->used_pages  * PAGE_SIZE) / (1024 * 1024);
    u64 total_mb = (pmm->total_pages * PAGE_SIZE) / (1024 * 1024);

    printf("\n┌─────────────────────────────────┐\n");
    printf("│         PMM Statistics          │\n");
    printf("├─────────────────────────────────┤\n");
    printf("│ Total : %6llu pages  (%4llu MB) │\n",
           (unsigned long long)pmm->total_pages, (unsigned long long)total_mb);
    printf("│ Free  : %6llu pages  (%4llu MB) │\n",
           (unsigned long long)pmm->free_pages,  (unsigned long long)free_mb);
    printf("│ Used  : %6llu pages  (%4llu MB) │\n",
           (unsigned long long)pmm->used_pages,  (unsigned long long)used_mb);
    printf("└─────────────────────────────────┘\n\n");
}
