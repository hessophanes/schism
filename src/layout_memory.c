#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include "xml.h"
#include "scenario.h"

// ---------------------------------------------------------------------------

static int layout_memory(struct xmlnode *memory, uint32_t *aligns) {
  uint64_t i;
  struct xmlnode *memreq;
  struct range *R, *R2;

  R = init_range(memory->attrs[MEMORY_ATTR_BASE].value.number,
		 memory->attrs[MEMORY_ATTR_SIZE].value.number);
  if (!R) {
    fprintf(stderr, "ERROR: cannot initialize memory layouter.\n");
    return 1;
  }

  // step 1: claim all fixed ranges
  iterate_over_children(i, memory, STRUCT_MEMREQ, memreq) {
    if (attr_exists(memreq->attrs + MEMREQ_ATTR_BASE)) {
      if (memreq->attrs[MEMREQ_ATTR_BASE].value.dict.keys != ATTR_WILDCARD) {
        fprintf(stderr, "ERROR: cannot claim pre-placed percore <memreq> nodes yet.\n");
        return 1;
      }
      if (attr_exists(memreq->attrs + MEMREQ_ATTR_ALIGN_SHIFT) &&
	(memreq->attrs[MEMREQ_ATTR_BASE].value.number & ((1UL << memreq->attrs[MEMREQ_ATTR_ALIGN_SHIFT].value.number) - 1))) {
        fprintf(stderr, "ERROR: fixed <memreq> does not fulfil own \"align_shift\".\n");
        return 1;
      }
      R2 = claim_range(R,
		get_dict_hex(memreq->attrs + MEMREQ_ATTR_BASE, 0),
		memreq->attrs[MEMORY_ATTR_SIZE].value.number);
      if (!R2) {
        fprintf(stderr, "ERROR: cannot claim \"%s\".\n", memreq->attrs[MEMREQ_ATTR_ID].value.string);
        error_dump_ranges(R);
        return 1;
      }
      R = R2;
    }
  }

  /* step 2: allocate ranges ordered by request size
   *
   * The biggest requests go first. Each request is placed on the best
   * possible alignment constraint, so page table generation can later make
   * use of superpage/block descriptors, and TLB overall pressure is lower.
   */
  while (1) {
    uint64_t base, size, size_tmp;
    int align_index;

    // determine size of largest unplaced <memreq> (if we get 0, we are done)
    size = 0;
    iterate_over_children(i, memory, STRUCT_MEMREQ, memreq) {
      if (attr_exists(memreq->attrs + MEMREQ_ATTR_BASE)) continue;
      size_tmp = memreq->attrs[MEMREQ_ATTR_SIZE].value.number;
      if (size_tmp > size) {
        size = size_tmp;
      }
    }
    if (size == 0) break;

    // figure out optimal alignment shift for this size
    for (i = 0; aligns[i] && (size < (1UL << aligns[i])); i++) ;
    if (aligns[i] == 0) {
      fprintf(stderr, "ERROR: no possible alignment for size 0x%lx.\n", size);
      return 1;
    }
    align_index = i;

    // allocate all open requests of this size
    iterate_over_children(i, memory, STRUCT_MEMREQ, memreq) {
      struct range *Q;
      int a;
      uint32_t custom_align = 0;

      if (attr_exists(memreq->attrs + MEMREQ_ATTR_BASE)) continue;
      size_tmp = memreq->attrs[MEMREQ_ATTR_SIZE].value.number;
      if (size_tmp != size) continue;

      if (attr_exists(memreq->attrs + MEMREQ_ATTR_ALIGN_SHIFT))
        custom_align = memreq->attrs[MEMREQ_ATTR_ALIGN_SHIFT].value.number;

      // all checks passed: allocate this one
      if (attr_exists(memreq->attrs + MEMREQ_ATTR_CPUMAP)) {
        // it's a percpu or private resource; allocate as specified by cpumap, possibly multiple times
        uint32_t j;

        for (j = 0; j < memreq->attrs[MEMREQ_ATTR_CPUMAP].value.list.num; j++) {
          Q = NULL;
          a = align_index;
          while (!Q && aligns[a]) {
            Q = find_range(R, size, (aligns[a] < custom_align) ? custom_align : aligns[a], &base);
            // fprintf(stderr, "DEBUG: Trying %s(%d.%d)... @%d -> %lx\n", memreq->attrs[MEMREQ_ATTR_ID].value.string, aligns[a], custom_align, j, base);
            ++a;
          }
          if (!Q) {
            fprintf(stderr, "ERROR: cannot place \"%s\".\n", memreq->attrs[MEMREQ_ATTR_ID].value.string);
            return 1;
          }
          add_dict_hex(memreq->attrs + MEMREQ_ATTR_BASE,
			memreq->attrs[MEMREQ_ATTR_CPUMAP].value.list.elem[j], base);
          R = Q;
        }
      } else {
        // it's a shared/global memory resource; allocate once
        Q = NULL;
        a = align_index;
        while (!Q && aligns[a]) {
          Q = find_range(R, size, (aligns[a] < custom_align) ? custom_align : aligns[a], &base);
            // fprintf(stderr, "DEBUG: Trying %s(%d.%d)... -> %lx\n", memreq->attrs[MEMREQ_ATTR_ID].value.string, aligns[a], custom_align, base);
          ++a;
        }
        if (!Q) {
          fprintf(stderr, "ERROR: cannot place \"%s\".\n", memreq->attrs[MEMREQ_ATTR_ID].value.string);
          return 1;
        }
        set_dict_wildcard_hex(memreq->attrs + MEMREQ_ATTR_BASE, base);
        R = Q;
      }
    }
  }

  return 0;
}

// ---------------------------------------------------------------------------

int main(int an, char **ac) {
  struct xmlnode *scene, *board, *memory;
  char fn[128];
  uint64_t i;
  uint32_t *aligns;

  if (an != 2) {
    fprintf(stderr, "ERROR: need build directory argument.\n");
    return 1;
  }

  sprintf(fn, "%s/scenario_measured.xml", ac[1]);
  scene = parse_from_file(fn, STRUCT_SCENARIO);
  if (!scene) {
    fprintf(stderr, "ERROR: could not load scenario: %s.\n", xml_errormsg);
    return 1;
  }

  // ---------------------------------------------------------------------------

  // get union of all desired alignments for this architecture
  aligns = get_preferred_alignments(get_child(get_child(scene, STRUCT_PLATFORM), STRUCT_ARCH));

  // layout physical memory space of each <memory>
  board = get_child(get_child(scene, STRUCT_PLATFORM), STRUCT_BOARD);

  iterate_over_children(i, board, STRUCT_MEMORY, memory) {
    if (layout_memory(memory, aligns)) {
      return 1;
    }
  }

  // ---------------------------------------------------------------------------

  sprintf(fn, "%s/scenario_p_laidout.xml", ac[1]);
  write_to_file(fn, scene);

  return 0;
}
