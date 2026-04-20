#include "tlb.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "clock.h"
#include "constants.h"
#include "log.h"
#include "memory.h"
#include "page_table.h"

typedef struct {
  bool valid;
  bool dirty;
  uint64_t last_access;
  va_t virtual_page_number;
  pa_dram_t physical_page_number;
} tlb_entry_t;

tlb_entry_t tlb_l1[TLB_L1_SIZE];
tlb_entry_t tlb_l2[TLB_L2_SIZE];

uint64_t tlb_l1_hits = 0;
uint64_t tlb_l1_misses = 0;
uint64_t tlb_l1_invalidations = 0;

uint64_t tlb_l2_hits = 0;
uint64_t tlb_l2_misses = 0;
uint64_t tlb_l2_invalidations = 0;

uint64_t get_total_tlb_l1_hits() { return tlb_l1_hits; }
uint64_t get_total_tlb_l1_misses() { return tlb_l1_misses; }
uint64_t get_total_tlb_l1_invalidations() { return tlb_l1_invalidations; }

uint64_t get_total_tlb_l2_hits() { return tlb_l2_hits; }
uint64_t get_total_tlb_l2_misses() { return tlb_l2_misses; }
uint64_t get_total_tlb_l2_invalidations() { return tlb_l2_invalidations; }

void tlb_init() {
  memset(tlb_l1, 0, sizeof(tlb_l1));
  memset(tlb_l2, 0, sizeof(tlb_l2));
  tlb_l1_hits = 0;
  tlb_l1_misses = 0;
  tlb_l1_invalidations = 0;
  tlb_l2_hits = 0;
  tlb_l2_misses = 0;
  tlb_l2_invalidations = 0;
}

tlb_entry_t* tlb_lookup(tlb_entry_t *tlb, size_t size, va_t virtual_page_number) {
  tlb_entry_t *entry = NULL;

  for (size_t i = 0; i < size; i++) {
    if (tlb[i].valid) {
      if (tlb[i].virtual_page_number == virtual_page_number) {
        tlb[i].last_access = 0;
        entry = &tlb[i];
      }
      else
        tlb[i].last_access++;
    }
  }

  if (tlb == tlb_l1)
    increment_time(TLB_L1_LATENCY_NS);
  else
    increment_time(TLB_L2_LATENCY_NS);
  
  return entry;
}

tlb_entry_t* tlb_get_insertion_entry(tlb_entry_t *tlb, size_t size) {
  tlb_entry_t *entry = NULL;
  uint64_t last_access = 0;

  for (size_t i = 0; i < size; i++) {
    if (!tlb[i].valid)
      return &tlb[i];

    if (tlb[i].last_access > last_access) {
      last_access = tlb[i].last_access;
      entry = &tlb[i];
    }
  }

  return entry;
}

tlb_entry_t* tlb_l2_insert(va_t virtual_address, pa_dram_t physical_page_number) {
  va_t virtual_page_number = 
      (virtual_address >> PAGE_SIZE_BITS) & PAGE_INDEX_MASK;
  tlb_entry_t *entry = tlb_get_insertion_entry(tlb_l2, TLB_L2_SIZE);

  if (entry->dirty) {
    pa_dram_t physical_address = entry->physical_page_number << PAGE_SIZE_BITS;
    write_back_tlb_entry(physical_address);
  }

  entry->valid = true;
  entry->dirty = false;
  entry->last_access = 0;
  entry->virtual_page_number = virtual_page_number;
  entry->physical_page_number = physical_page_number;
  return entry;
}

tlb_entry_t* tlb_l1_insert(va_t virtual_address, 
    pa_dram_t physical_page_number, op_t op) {
  va_t virtual_page_number = 
      (virtual_address >> PAGE_SIZE_BITS) & PAGE_INDEX_MASK;
  tlb_entry_t *entry = tlb_get_insertion_entry(tlb_l1, TLB_L1_SIZE);
  
  if (entry->valid && entry->dirty) {
    tlb_entry_t *l2_entry = NULL;

    for (size_t i = 0; i < TLB_L2_SIZE; i++) {
      if (tlb_l2[i].valid && 
          tlb_l2[i].virtual_page_number == entry->virtual_page_number) {
        tlb_l2[i].dirty = true;
        tlb_l2[i].last_access = 0;
        l2_entry = &tlb_l2[i];
        break;
      }
    }

    if (!l2_entry) {
      pa_dram_t translated_address = page_table_translate(virtual_address, op);  
      pa_dram_t physical_page_number = 
          (translated_address >> PAGE_SIZE_BITS) & DRAM_ADDRESS_MASK;
      l2_entry = tlb_l2_insert(virtual_address, physical_page_number);
      l2_entry->dirty = true;
    }
  }

  entry->valid = true;
  entry->last_access = 0;
  entry->virtual_page_number = virtual_page_number;
  entry->physical_page_number = physical_page_number;

  if (op == OP_WRITE) 
    entry->dirty = true; 
  else 
    entry->dirty = false;
  
  return entry;
}

void tlb_invalidate(va_t virtual_page_number) {
  tlb_entry_t *l1_entry = NULL;
  tlb_entry_t *l2_entry = NULL;

  for (size_t i = 0; i < TLB_L1_SIZE; i++) {
    if (tlb_l1[i].virtual_page_number == virtual_page_number && tlb_l1[i].valid) {
      tlb_l1[i].valid = false;
      l1_entry = &tlb_l1[i];
      tlb_l1_invalidations++;
      break;
    }
  }

  for (size_t i = 0; i < TLB_L2_SIZE; i++) {
    if (tlb_l2[i].virtual_page_number == virtual_page_number && tlb_l2[i].valid) {
      tlb_l2[i].valid = false;
      l2_entry = &tlb_l2[i];
      tlb_l2_invalidations++;
      break;
    }
  }

  if (l1_entry && l1_entry->dirty) {
    if (!l2_entry) {
      l2_entry = tlb_l2_insert(l1_entry->virtual_page_number << PAGE_SIZE_BITS, 
                               l1_entry->physical_page_number);
    }

    l2_entry->dirty = true;
    l2_entry->valid = false;
    write_back_tlb_entry(l1_entry->physical_page_number << PAGE_SIZE_BITS);
  }
  else if (l2_entry && l2_entry->dirty)
    write_back_tlb_entry(l1_entry->physical_page_number << PAGE_SIZE_BITS);

  increment_time(TLB_L1_LATENCY_NS);
  increment_time(TLB_L2_LATENCY_NS);
}

pa_dram_t tlb_translate(va_t virtual_address, op_t op) {
  va_t virtual_page_number = 
      (virtual_address >> PAGE_SIZE_BITS) & PAGE_INDEX_MASK;
  va_t virtual_page_offset = virtual_address & PAGE_OFFSET_MASK;
  tlb_entry_t *l1_entry = tlb_lookup(tlb_l1, TLB_L1_SIZE, virtual_page_number);

  if (!l1_entry) {
    tlb_l1_misses++;
    tlb_entry_t *l2_entry = 
        tlb_lookup(tlb_l2, TLB_L2_SIZE, virtual_page_number);

    if (!l2_entry) {
      tlb_l2_misses++;
      pa_dram_t translated_address = page_table_translate(virtual_address, op);  
      pa_dram_t physical_page_number = 
          (translated_address >> PAGE_SIZE_BITS) & DRAM_ADDRESS_MASK;
      l2_entry = tlb_l2_insert(virtual_address, physical_page_number);
    }
    else
      tlb_l2_hits++;

    l1_entry = 
        tlb_l1_insert(virtual_address, l2_entry->physical_page_number, op);
  }
  else
    tlb_l1_hits++;

  if (op == OP_WRITE) 
    l1_entry->dirty = true; 

  pa_dram_t translated_address =
      (l1_entry->physical_page_number << PAGE_SIZE_BITS) | virtual_page_offset;
  return translated_address;
}
