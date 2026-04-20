#include "kmalloc.h"

static block_header_t *heap_start = NULL;

void kmalloc_init(uint32_t start_addr, uint32_t size)
{
    heap_start = (block_header_t *)start_addr;
    heap_start->size = size - sizeof(block_header_t);
    heap_start->is_free = 1;
    heap_start->next = NULL;
}

void *kmalloc(uint32_t size)
{
    block_header_t *current = heap_start;

    while (current)
    {
        if (current->is_free && current->size >= size)
        {

            if (current->size > size + sizeof(block_header_t))
            {
                block_header_t *new_block = (block_header_t *)((uint8_t *)current + sizeof(block_header_t) + size);
                new_block->size = current->size - size - sizeof(block_header_t);
                new_block->is_free = 1;
                new_block->next = current->next;

                current->size = size;
                current->next = new_block;
            }
            current->is_free = 0;
            return (void *)((uint8_t *)current + sizeof(block_header_t));
        }
        current = current->next;
    }
    return NULL;
}
void kfree(void *ptr)
{
    if (!ptr)
        return;
    block_header_t *block = (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));
    block->is_free = 1;

    // Coalesce adjacent free blocks to prevent fragmentation
    block_header_t *current = heap_start;
    while (current && current->next)
    {
        if (current->is_free && current->next->is_free)
        {
            current->size += sizeof(block_header_t) + current->next->size;
            current->next = current->next->next;
        }
        else
        {
            current = current->next;
        }
    }
}