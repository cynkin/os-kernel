#include "printf.h"
#include "memory.h"
#include "vmm.h"

/* ─── Internal helpers ──────────────────────────────────────────────────── */

/*
 * get_or_create_table:
 *   Given a page table 'table' and an index, return a pointer to the
 *   next-level page table. If the entry is not present, allocate a new
 *   page from the PMM, install it, and return it.
 */
static pte_t *get_or_create_table(vmm_t *vmm, pte_t *table, u64 index, u64 flags) {
    if (!(table[index] & PTE_PRESENT)) {
        paddr_t page = pmm_alloc(vmm->pmm);
        if (page == (paddr_t)-1) {
            printf("[VMM] get_or_create_table: PMM out of memory!\n");
            return NULL;
        }
        table[index] = (page & PTE_ADDR_MASK) | flags | PTE_PRESENT;
    }
    paddr_t phys = table[index] & PTE_ADDR_MASK;
    return (pte_t *)phys_to_virt(phys);
}

/*
 * walk_table:
 *   Walk existing page tables. Returns NULL if any level is not present.
 *   Does NOT allocate new tables.
 */
static pte_t *walk_table(vmm_t *vmm, vaddr_t virt,
                          pte_t **out_pdpt, pte_t **out_pd, pte_t **out_pt) {
    u64 l4 = PML4_INDEX(virt);
    u64 l3 = PDPT_INDEX(virt);
    u64 l2 = PD_INDEX(virt);
    u64 l1 = PT_INDEX(virt);

    if (!(vmm->pml4[l4] & PTE_PRESENT)) return NULL;
    pte_t *pdpt = (pte_t *)phys_to_virt(vmm->pml4[l4] & PTE_ADDR_MASK);
    if (out_pdpt) *out_pdpt = pdpt;

    if (!(pdpt[l3] & PTE_PRESENT)) return NULL;
    pte_t *pd = (pte_t *)phys_to_virt(pdpt[l3] & PTE_ADDR_MASK);
    if (out_pd) *out_pd = pd;

    if (!(pd[l2] & PTE_PRESENT)) return NULL;
    pte_t *pt = (pte_t *)phys_to_virt(pd[l2] & PTE_ADDR_MASK);
    if (out_pt) *out_pt = pt;

    (void)l1;
    return pt;
}

/* ─── VMM Init ──────────────────────────────────────────────────────────── */

void vmm_init(vmm_t *vmm, pmm_t *pmm) {
    vmm->pmm    = pmm;
    vmm->mapped = 0;

    /* Allocate the top-level PML4 page table */
    vmm->pml4_phys = pmm_alloc(pmm);
    if (vmm->pml4_phys == (paddr_t)-1) {
        printf("[VMM] vmm_init: failed to allocate PML4!\n");
        vmm->pml4 = NULL;
        return;
    }

    vmm->pml4 = (pte_t *)phys_to_virt(vmm->pml4_phys);
    printf("[VMM] Initialized │ PML4 @ phys=0x%lx\n",
           (unsigned long)vmm->pml4_phys);
}

/* ─── Map single page ───────────────────────────────────────────────────── */

int vmm_map(vmm_t *vmm, vaddr_t virt, paddr_t phys, u64 flags) {
    if (!vmm->pml4) return -1;

    virt = ALIGN_DOWN(virt, PAGE_SIZE);
    phys = ALIGN_DOWN(phys, PAGE_SIZE);

    u64 l4 = PML4_INDEX(virt);
    u64 l3 = PDPT_INDEX(virt);
    u64 l2 = PD_INDEX(virt);
    u64 l1 = PT_INDEX(virt);

    pte_t *pdpt = get_or_create_table(vmm, vmm->pml4, l4, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    if (!pdpt) return -1;

    pte_t *pd = get_or_create_table(vmm, pdpt, l3, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    if (!pd) return -1;

    pte_t *pt = get_or_create_table(vmm, pd, l2, PTE_PRESENT | PTE_WRITABLE | PTE_USER);
    if (!pt) return -1;

    if (pt[l1] & PTE_PRESENT) {
        /* Remapping: just update the entry */
        pt[l1] = (phys & PTE_ADDR_MASK) | flags | PTE_PRESENT;
    } else {
        pt[l1] = (phys & PTE_ADDR_MASK) | flags | PTE_PRESENT;
        vmm->mapped++;
    }

    return 0;
}

/* ─── Map a range of pages ───────────────────────────────────────────────── */

int vmm_map_range(vmm_t *vmm, vaddr_t virt, paddr_t phys, u64 pages, u64 flags) {
    for (u64 i = 0; i < pages; i++) {
        if (vmm_map(vmm, virt + i * PAGE_SIZE, phys + i * PAGE_SIZE, flags) != 0)
            return -1;
    }
    return 0;
}

/* ─── Unmap single page ─────────────────────────────────────────────────── */

int vmm_unmap(vmm_t *vmm, vaddr_t virt) {
    if (!vmm->pml4) return -1;

    virt = ALIGN_DOWN(virt, PAGE_SIZE);
    u64 l1 = PT_INDEX(virt);

    pte_t *pt = walk_table(vmm, virt, NULL, NULL, NULL);
    if (!pt) {
        printf("[VMM] vmm_unmap: 0x%lx not mapped!\n", (unsigned long)virt);
        return -1;
    }

    if (!(pt[l1] & PTE_PRESENT)) {
        printf("[VMM] vmm_unmap: 0x%lx not present!\n", (unsigned long)virt);
        return -1;
    }

    pt[l1] = 0;
    vmm->mapped--;
    return 0;
}

/* ─── Unmap range ───────────────────────────────────────────────────────── */

int vmm_unmap_range(vmm_t *vmm, vaddr_t virt, u64 pages) {
    for (u64 i = 0; i < pages; i++) {
        vmm_unmap(vmm, virt + i * PAGE_SIZE);
    }
    return 0;
}

/* ─── Translate virtual → physical ─────────────────────────────────────── */

paddr_t vmm_translate(vmm_t *vmm, vaddr_t virt) {
    if (!vmm->pml4) return (paddr_t)-1;

    u64 l1 = PT_INDEX(virt);
    u64 offset = PAGE_OFFSET(virt);

    pte_t *pt = walk_table(vmm, virt, NULL, NULL, NULL);
    if (!pt)                     return (paddr_t)-1;
    if (!(pt[l1] & PTE_PRESENT)) return (paddr_t)-1;

    return (pt[l1] & PTE_ADDR_MASK) | offset;
}

/* ─── Is mapped? ────────────────────────────────────────────────────────── */

int vmm_is_mapped(vmm_t *vmm, vaddr_t virt) {
    return vmm_translate(vmm, virt) != (paddr_t)-1;
}

/* ─── Print single mapping ──────────────────────────────────────────────── */

void vmm_print_mapping(vmm_t *vmm, vaddr_t virt) {
    paddr_t phys = vmm_translate(vmm, virt);

    if (phys == (paddr_t)-1) {
        printf("  virt=0x%016lx  →  [ NOT MAPPED ]\n", (unsigned long)virt);
    } else {
        /* Decode flags from PT entry */
        u64 l1 = PT_INDEX(virt);
        pte_t *pt = walk_table(vmm, virt, NULL, NULL, NULL);
        u64 flags = pt ? (pt[l1] & ~PTE_ADDR_MASK) : 0;

        printf("  virt=0x%016lx  →  phys=0x%016lx  [%c%c%c%c]\n",
               (unsigned long)virt, (unsigned long)phys,
               (flags & PTE_PRESENT)  ? 'P' : '-',
               (flags & PTE_WRITABLE) ? 'W' : 'R',
               (flags & PTE_USER)     ? 'U' : 'K',
               (flags & PTE_NX)       ? 'X' : '-');
    }
}

/* ─── Stats ─────────────────────────────────────────────────────────────── */

void vmm_print_stats(const vmm_t *vmm) {
    printf("\n┌─────────────────────────────────┐\n");
    printf("│         VMM Statistics          │\n");
    printf("├─────────────────────────────────┤\n");
    printf("│ PML4 phys : 0x%-18lx │\n", (unsigned long)vmm->pml4_phys);
    printf("│ Pages mapped : %-18llu │\n", (unsigned long long)vmm->mapped);
    printf("└─────────────────────────────────┘\n\n");
}

/* ─── Dump PML4 (top-level only) ────────────────────────────────────────── */

void vmm_dump_pml4(vmm_t *vmm) {
    printf("\n=== PML4 Dump ===\n");
    for (int i = 0; i < 512; i++) {
        if (vmm->pml4[i] & PTE_PRESENT) {
            printf("  PML4[%3d] = 0x%016llx  phys=0x%012llx  [%c%c%c]\n",
                   i,
                   (unsigned long long)vmm->pml4[i],
                   (unsigned long long)(vmm->pml4[i] & PTE_ADDR_MASK),
                   (vmm->pml4[i] & PTE_WRITABLE) ? 'W' : 'R',
                   (vmm->pml4[i] & PTE_USER)     ? 'U' : 'K',
                   (vmm->pml4[i] & PTE_NX)       ? 'X' : '-');
        }
    }
    printf("=================\n\n");
}
