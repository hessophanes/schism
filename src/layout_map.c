#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include "xml.h"
#include "scenario.h"

// ---------------------------------------------------------------------------

static struct range *map_range;
static uint32_t *map_aligns;
static uint64_t globals_window[2];

static int prepare_map(struct xmlnode *scene, struct xmlnode *mmu) {
  struct xmlnode *paging_format;

  // figure out paging format for this MMU
  paging_format = resolve_idref(scene, mmu->attrs + MMU_ATTR_FORMAT);

  if (paging_format == NULL) {
    fprintf(stderr, "ERROR: unknown page table format.\n");
    return 1;
  }

  map_aligns = get_preferred_alignment(paging_format);

  map_range = init_range(0, (1L << paging_format->attrs[PAGING_FORMAT_ATTR_VA_WIDTH].value.number) - 0x1000);
  if (!map_range) {
    fprintf(stderr, "ERROR: cannot initialize map layouter.\n");
    return 1;
  }

  return 0;
}

static int layout_map_fixmaps(struct xmlnode *scene, struct xmlnode *mmu) {
  uint64_t i;
  uint64_t base_tmp, size_tmp;
  struct xmlnode *map, *memdev;

  // step 1: claim all fixed mappings
  iterate_over_children(i, mmu, STRUCT_MAP, map) {
    if (!attr_exists(map->attrs + MAP_ATTR_BASE)) continue;

    memdev = resolve_idref(scene, map->attrs + MAP_ATTR_XREF);
    if (map->attrs[MAP_ATTR_BASE].value.dict.keys != ATTR_WILDCARD) {
      fprintf(stderr, "ERROR: cannot claim pre-placed percore <map> nodes yet.\n");
      return 1;
    }
    base_tmp = get_dict_hex(map->attrs + MAP_ATTR_BASE, 0);

    if (attr_exists(map->attrs + MAP_ATTR_SUBSIZE)) {
      size_tmp = map->attrs[MAP_ATTR_SUBSIZE].value.number;
    } else if (memdev && (memdev->node_type == STRUCT_MEMREQ)) {
      size_tmp = memdev->attrs[MEMREQ_ATTR_SIZE].value.number;
    } else if (memdev && (memdev->node_type == STRUCT_DEVICE)) {
      size_tmp = memdev->attrs[DEVICE_ATTR_SIZE].value.number;
    } else {
      fprintf(stderr, "ERROR: no <memreq> or <device> for <map> \"%s\".\n", map->attrs[MAP_ATTR_XREF].value.string);
      return 1;
    }
    map_range = claim_range(map_range, base_tmp, size_tmp);
    if (!map_range) {
      fprintf(stderr, "ERROR: cannot claim \"%s\".\n", map->attrs[MAP_ATTR_XREF].value.string);
      return 1;
    }
    // fprintf(stderr, "DEBUG: Claimed %s, size 0x%lx\n", map->attrs[MAP_ATTR_XREF].value.string, size_tmp);
  }

  return 0;
}

/**
 * common_or_cpu control parameter:
 * -1		non CPU-restrained mappings (without "cpumap" attribute)
 * [0...]	CPU-restrained mappings
 *
 * If a mapping has the 'g'lobal flag set, it is placed into globals_window[]
 * (which is required on paravirt).
 */
static int layout_map(struct xmlnode *scene, struct xmlnode *mmu, int common_or_cpu) {
  uint64_t i;
  struct xmlnode *map, *memdev;

  // step 2: allocate mappings
  while (1) {
    uint64_t base, size;
    int align_index;
    unsigned long size_tmp;

    size = 0;
    iterate_over_children(i, mmu, STRUCT_MAP, map) {
      // if there's already a "map" attribute, skip this item if we're doing
      // common mappings or if the attribute contains an entry for this CPU
      if (attr_exists(map->attrs + MAP_ATTR_BASE) &&
	  ((common_or_cpu < 0) || has_dict(map->attrs + MAP_ATTR_BASE, (uint32_t)common_or_cpu)) )
        continue;

      // if we're doing common mappings and this one has a cpumap, skip it
      if ((common_or_cpu < 0) && attr_exists(map->attrs + MAP_ATTR_CPUMAP))
        continue;

      // if we're doing CPU-specific mappings...
      if (common_or_cpu >= 0) {
        // ... and this one doesn't have a cpumap, skip it
        if (!attr_exists(map->attrs + MAP_ATTR_CPUMAP))
          continue;

        // ... and this one has a cpumap which doesn't contain this CPU, skip it
        if (!has_list(map->attrs + MAP_ATTR_CPUMAP, (uint32_t)common_or_cpu))
          continue;
      }

      // determine size from mapped <memreq> or <device> (or from <map> subsize parameter)
      memdev = resolve_idref(scene, map->attrs + MAP_ATTR_XREF);
      if (attr_exists(map->attrs + MAP_ATTR_SUBSIZE)) {
        size_tmp = map->attrs[MAP_ATTR_SUBSIZE].value.number;
      } else if (memdev && (memdev->node_type == STRUCT_MEMREQ)) {
        size_tmp = memdev->attrs[MEMREQ_ATTR_SIZE].value.number;
      } else if (memdev && (memdev->node_type == STRUCT_DEVICE)) {
        size_tmp = memdev->attrs[DEVICE_ATTR_SIZE].value.number;
      } else {
        fprintf(stderr, "ERROR: no <memreq> or <device> for <map> \"%s\".\n", map->attrs[MAP_ATTR_XREF].value.string);
        return 1;
      }
      if (size_tmp > size) {
        size = size_tmp;
      }
    }
    if (size == 0) break;

    for (i = 0; map_aligns[i] && (size < (1UL << map_aligns[i])); i++) ;
    if (map_aligns[i] == 0) {
      fprintf(stderr, "ERROR: size of <map> \"%s\" is not minimally aligned.\n", map->attrs[MAP_ATTR_XREF].value.string);
      return 1;
    }
    align_index = i;

    iterate_over_children(i, mmu, STRUCT_MAP, map) {
      struct range *Q;
      int a;
      int place_into_window;

      // if there's already a "map" attribute, skip this item if we're doing
      // common mappings or if the attribute contains an entry for this CPU
      if (attr_exists(map->attrs + MAP_ATTR_BASE) &&
	  ((common_or_cpu < 0) || has_dict(map->attrs + MAP_ATTR_BASE, (uint32_t)common_or_cpu)) )
        continue;

      // if we're doing common mappings and this one has a cpumap, skip it
      if ((common_or_cpu < 0) && attr_exists(map->attrs + MAP_ATTR_CPUMAP))
        continue;

      // if we're doing CPU-specific mappings...
      if (common_or_cpu >= 0) {
        // ... and this one doesn't have a cpumap, skip it
        if (!attr_exists(map->attrs + MAP_ATTR_CPUMAP))
          continue;

        // ... and this one has a cpumap which doesn't contain this CPU, skip it
        if (!has_list(map->attrs + MAP_ATTR_CPUMAP, (uint32_t)common_or_cpu))
          continue;
      }

      // determine size from mapped <memreq> or <device> (or <map> subsize);
      // skip if different from the current maximum size
      memdev = resolve_idref(scene, map->attrs + MAP_ATTR_XREF);
      if (attr_exists(map->attrs + MAP_ATTR_SUBSIZE)) {
        size_tmp = map->attrs[MAP_ATTR_SUBSIZE].value.number;
      } else if (memdev && (memdev->node_type == STRUCT_MEMREQ)) {
        size_tmp = memdev->attrs[MEMREQ_ATTR_SIZE].value.number;
      } else if (memdev && (memdev->node_type == STRUCT_DEVICE)) {
        size_tmp = memdev->attrs[DEVICE_ATTR_SIZE].value.number;
      } else {
        fprintf(stderr, "ERROR: no <memreq> or <device> for <map> \"%s\".\n", map->attrs[MAP_ATTR_XREF].value.string);
        return 1;
      }
      if (size_tmp != size) continue;

      // fprintf(stderr, "DEBUG: Placing %s (CPU %d), size 0x%lx\n", map->attrs[MAP_ATTR_XREF].value.string, common_or_cpu, size);
      place_into_window = (strchr(map->attrs[MAP_ATTR_FLAGS].value.string, 'g')) ? 1 : 0;
      Q = NULL;
      a = align_index;

      while (!Q && map_aligns[a]) {
        Q = find_padded_window_range(map_range, size, map_aligns[a], &base, 12, place_into_window ? globals_window : NULL);
        // fprintf(stderr, "DEBUG: Trying %s(%d)... -> %lx\n", map->attrs[MAP_ATTR_XREF].value.string, map_aligns[a], base);
        ++a;
      }
      if (!Q) {
        fprintf(stderr, "ERROR: cannot place \"%s\".\n", map->attrs[MAP_ATTR_XREF].value.string);
        return 1;
      }

      // add placement to "map" attribute
      if (common_or_cpu >= 0) {
        add_dict_hex(map->attrs + MAP_ATTR_BASE, (uint32_t)common_or_cpu, base);
      } else {
        set_dict_wildcard_hex(map->attrs + MAP_ATTR_BASE, base);
      }
      map_range = Q;
    }
  }

  return 0;
}

// ---------------------------------------------------------------------------

int main(int an, char **ac) {
  struct xmlnode *scene, *hypervisor, *mmu, *map, *guest;
  char fn[128];
  uint64_t i, j;

  if (an != 2) {
    fprintf(stderr, "ERROR: need build directory argument.\n");
    return 1;
  }

  sprintf(fn, "%s/scenario_p_laidout.xml", ac[1]);
  scene = parse_from_file(fn, STRUCT_SCENARIO);
  if (!scene) {
    fprintf(stderr, "ERROR: could not load scenario: %s.\n", xml_errormsg);
    return 1;
  }

  // ---------------------------------------------------------------------------

  hypervisor = get_child(scene, STRUCT_HYPERVISOR);
  mmu = get_child(hypervisor, STRUCT_MMU);

  iterate_over_children(i, mmu, STRUCT_MAP, map) {
    if (strcmp(map->attrs[MAP_ATTR_XREF].value.string, "core_rx") == 0) {
      globals_window[0] = get_dict_hex(map->attrs + MAP_ATTR_BASE, 0);
      break;
    }
  }
  globals_window[1] = ~0UL;

  prepare_map(scene, mmu);
  layout_map_fixmaps(scene, mmu);
  layout_map(scene, mmu, -1);

  for (i = 0; i < hypervisor->attrs[HYPERVISOR_ATTR_NCPUS].value.number; i++) {
    struct range *Rclone = clone_range(map_range);
    layout_map(scene, mmu, i);
    map_range = Rclone;
  }

  map_range = NULL;

  iterate_over_children(i, scene, STRUCT_GUEST, guest) {
    mmu = get_child(guest, STRUCT_MMU);
    iterate_over_children(j, mmu, STRUCT_MAP, map) {
      if (!attr_exists(map->attrs + MAP_ATTR_BASE)) {
        fprintf(stderr, "ERROR: no dynamic placement for guests.\n");
        return 1;
      }
    }
  }

  // ---------------------------------------------------------------------------

  sprintf(fn, "%s/scenario_v_laidout.xml", ac[1]);
  write_to_file(fn, scene);

  return 0;
}
