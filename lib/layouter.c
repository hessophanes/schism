#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "xml.h"
#include "scenario.h"

static void sort_into_aligns(uint32_t *aligns, struct xmlnode *paging_format) {
  struct xmlnode *level;
  uint64_t j, k;
  uint32_t val;

  iterate_over_children(j, paging_format, STRUCT_LEVEL, level) {
    val = (uint32_t)level->attrs[LEVEL_ATTR_SHIFT].value.number;
    for (k = 0; val; k++) {
      if (aligns[k] == val) break;
      if (aligns[k] < val) {
        uint32_t tmp = aligns[k];
        aligns[k] = val;
        val = tmp;
      }
    }
  }
}

uint32_t *get_preferred_alignment(struct xmlnode *paging_format) {
  uint32_t *aligns;

  aligns = malloc((paging_format->num_children + 1) * sizeof(uint32_t));
  memset(aligns, 0, (paging_format->num_children + 1) * sizeof(uint32_t));

  sort_into_aligns(aligns, paging_format);

  return aligns;
}

uint32_t *get_preferred_alignments(struct xmlnode *arch) {
  struct xmlnode *paging_format;
  uint32_t *aligns;
  uint64_t i, k;

  k = 0;
  iterate_over_children(i, arch, STRUCT_PAGING_FORMAT, paging_format) {
    k += paging_format->num_children;
  }
  ++k;

  aligns = malloc(k * sizeof(uint32_t));
  memset(aligns, 0, k * sizeof(uint32_t));

  iterate_over_children(i, arch, STRUCT_PAGING_FORMAT, paging_format) {
    sort_into_aligns(aligns, paging_format);
  }
  return aligns;
}

// ---------------------------------------------------------------------------

struct range *init_range(unsigned long base, unsigned long size) {
  struct range *range = malloc(sizeof(struct range));

  range->base = base;
  range->size = size;
  range->prev = NULL;
  range->next = NULL;

  return range;
}

struct range *claim_range(struct range *head, unsigned long base, unsigned long size) {
  struct range *R, *Q;

  for (R = head; R && (R->base + R->size <= base); R = R->next) ;

  if (!R) {
    return NULL;
  } // asserted: we didn't fall off the chain

  if (R->base + R->size < base + size) {
    return NULL;
  } // asserted: requested memory is contained in a single node

  // case 1: request fully covers node -> remove node
  if ((R->base == base) && (R->size == size)) {
    if (R->prev == NULL) {
      R->next->prev = NULL;
      return R->next;
    }
    R->prev->next = R->next;
    R->next->prev = R->prev;
    return head;
  }

  // case 2: request is at bottom of node
  if (R->base == base) { 
    R->base += size;
    R->size -= size;
    return head;
  }

  // case 3: request is at ceiling of node
  if (R->base + R->size == base + size) {
    R->size -= size;
    return head;
  }

  // case 4: request is in between -> split node
  Q = malloc(sizeof(struct range));
  Q->base = base + size;
  Q->size = (R->base + R->size) - Q->base;
  Q->next = R->next;
  Q->prev = R;
  R->next = Q;
  R->size = (base - R->base);

  return head;
}

struct range *find_range(struct range *head, unsigned long size, int align, unsigned long *base_ret) {
  struct range *R;
  unsigned long base;

  for (R = head; R; R = R->next) {
    if (R->size < size) continue;
    base = R->base;
    if (base & ALIGN_MASK(align))
      base = (base + (1UL << align)) & ~ALIGN_MASK(align);
    if (base + size <= R->base + R->size) {
      R = claim_range(head, base, size);
      if (R && base_ret) {
        *base_ret = base;
      }
      return R;
    }
  }

  return NULL;
}

struct range *find_padded_window_range(struct range *head, unsigned long size, int align,
		unsigned long *base_ret, int pad_align, unsigned long *window) {
  struct range *R;
  unsigned long base;
  unsigned long size1pad = size + (1UL << pad_align);
  unsigned long size2pad = size + (2UL << pad_align);

  for (R = head; R; R = R->next) {
    if (window && (R->base + R->size <= window[0])) continue;
    if (window && (R->base > window[1])) continue;
    if (R->size < size2pad) continue;
    base = R->base + (1UL << pad_align);
    if (window && (base < window[0]))
      base = window[0];
    if (base & ALIGN_MASK(align))
      base = (base + (1UL << align)) & ~ALIGN_MASK(align);
    if (window && (base + size > window[1])) continue;
    if (base + size1pad <= R->base + R->size) {
      unsigned long claim_base = base;
      unsigned long claim_size = size;
      if (R->base == base - (1UL << pad_align)) {
        claim_base -= (1UL << pad_align);
        claim_size += (1UL << pad_align);
      }
      if (R->base + R->size == base + size1pad) {
        claim_size += (1UL << pad_align);
      }
      R = claim_range(head, claim_base, claim_size);
      if (R && base_ret) {
        *base_ret = base;
      }
      return R;
    }
  }

  return NULL;
}

struct range *clone_range(struct range *head) {
  struct range *ret_head;
  struct range *prev, *iter;

  if (head == NULL) return NULL;

  ret_head = malloc(sizeof(struct range));
  ret_head->base = head->base;
  ret_head->size = head->size;
  ret_head->prev = NULL;
  ret_head->next = NULL;

  prev = ret_head;
  iter = head->next;
  while (iter) {
    prev->next = malloc(sizeof(struct range));
    prev->next->base = iter->base;
    prev->next->size = iter->size;
    prev->next->prev = prev;
    prev->next->next = NULL;
    prev = prev->next;
    iter = iter->next;
  }

  return ret_head;
}

void error_dump_ranges(struct range *head) {
  fprintf(stderr, "Range list dump:\n");
  while (head) {
    fprintf(stderr, "[%p] PRV %p NXT %p BASE %lx SIZE %lx\n",
	head, head->prev, head->next, head->base, head->size);
    head = head->next;
  }
}

// ---------------------------------------------------------------------------

struct xmlnode *find_memreq(struct xmlnode *scene, const char *map_xref) {
  struct xmlnode *platform, *board, *memory, *memreq;
  uint64_t i, j;

  platform = get_child(scene, STRUCT_PLATFORM);
  board = get_child(platform, STRUCT_BOARD);

  iterate_over_children(i, board, STRUCT_MEMORY, memory) {
    iterate_over_children(j, memory, STRUCT_MEMREQ, memreq) {
      if (strcmp(memreq->attrs[MEMREQ_ATTR_ID].value.string, map_xref) == 0)
        return memreq;
    }
  }

  return NULL;
}

struct xmlnode *find_device(struct xmlnode *scene, const char *map_xref) {
  struct xmlnode *platform, *board, *device;
  uint64_t i;

  platform = get_child(scene, STRUCT_PLATFORM);
  board = get_child(platform, STRUCT_BOARD);

  iterate_over_children(i, board, STRUCT_DEVICE, device) {
    if (strcmp(device->attrs[DEVICE_ATTR_ID].value.string, map_xref) == 0)
      return device;
  }

  return NULL;
}

struct xmlnode *find_mmu_map(struct xmlnode *mmu, const char *id) {
  struct xmlnode *map;
  uint64_t i;

  iterate_over_children(i, mmu, STRUCT_MAP, map) {
    if (strcmp(map->attrs[MAP_ATTR_XREF].value.string, id) == 0)
      return map;
  }

  return NULL;
}
