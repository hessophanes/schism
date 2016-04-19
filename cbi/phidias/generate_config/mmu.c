#include "generate_config.h"

#define	HYPERVISOR_FIXED_MEMAREA_COUNT		32

struct memarea_fixed_index {
  const char *id;
  uint32_t index;
};

static const struct memarea_fixed_index hypervisor_fixed_memareas[] = {
  { "core_rx",		0, },
  { "core_r",		1, },
  { "core_rw",		2, },
  { "core_rws",		3, },
  { "core_rwt",		4, },
  { "pagetables",	5, },
  { "blob",		6, },
  { "stack",		7, },
  { "config_r",		8, },
  { "config_rw",	9, },
  { "config_rws",	10, },
  { "config_rwt",	11, },
  { "trace",		12, },
  { "serial",		16, },
  { "mpcore",		17, },
  { "cs7a",		24, },
  { "mbox",		24, },
  { "irqc",		25, },
};

uint32_t hypervisor_memarea_index(struct xmlnode *map) {
  uint32_t array_index;

  for (array_index = 0; array_index < ARRAYLEN(hypervisor_fixed_memareas); array_index++) {
    if (strcmp(map->attrs[MAP_ATTR_XREF].value.string,
		hypervisor_fixed_memareas[array_index].id) == 0) {
      return hypervisor_fixed_memareas[array_index].index;
    }
  }

  return HYPERVISOR_FIXED_MEMAREA_COUNT;
}

char *map_flags_string(const char *flags_attribute) {
  char *map_flags = malloc(16 * strlen(flags_attribute));

  if (strlen(flags_attribute) == 0) {
    return "0";
  }

  map_flags[0] = '\0';

  if (strchr(flags_attribute, 'r'))
    sprintf(map_flags + strlen(map_flags), "|MEMAREA_FLAG_R");
  if (strchr(flags_attribute, 'w'))
    sprintf(map_flags + strlen(map_flags), "|MEMAREA_FLAG_W");
  if (strchr(flags_attribute, 'x'))
    sprintf(map_flags + strlen(map_flags), "|MEMAREA_FLAG_X");
  if (strchr(flags_attribute, 'g'))
    sprintf(map_flags + strlen(map_flags), "|MEMAREA_FLAG_G");
  if (strchr(flags_attribute, 'd'))
    sprintf(map_flags + strlen(map_flags), "|MEMAREA_FLAG_D");
  if (strchr(flags_attribute, 'u'))
    sprintf(map_flags + strlen(map_flags), "|MEMAREA_FLAG_U");
  if (strchr(flags_attribute, 's'))
    sprintf(map_flags + strlen(map_flags), "|MEMAREA_FLAG_S");

  return map_flags + 1;
}

// --------------------------------------------------------------------------

uint32_t hypervisor_memarea_count(struct xmlnode *scene, uint32_t physical_cpu) {
  uint32_t count = HYPERVISOR_FIXED_MEMAREA_COUNT;
  uint32_t memarea_index;
  struct xmlnode *mmu;
  struct xmlnode *map;
  uint64_t xmliter_maps;

  mmu = get_child(get_child(scene, STRUCT_HYPERVISOR), STRUCT_MMU);

  iterate_over_children(xmliter_maps, mmu, STRUCT_MAP, map) {
    if (attr_exists(map->attrs + MAP_ATTR_CPUMAP) &&
	!has_list(map->attrs + MAP_ATTR_CPUMAP, physical_cpu))
      continue;

    memarea_index = hypervisor_memarea_index(map);

    if (memarea_index < HYPERVISOR_FIXED_MEMAREA_COUNT) {
      map->attrs[MAP_ATTR_INDEX].state = ATTRSTATE_MODIFIED;
      map->attrs[MAP_ATTR_INDEX].attr_type = ATTRTYPE_DEC;
      map->attrs[MAP_ATTR_INDEX].value.number = memarea_index;
    } else {
      map->attrs[MAP_ATTR_INDEX].state = ATTRSTATE_MODIFIED;
      map->attrs[MAP_ATTR_INDEX].attr_type = ATTRTYPE_DEC;
      map->attrs[MAP_ATTR_INDEX].value.number = count;
      ++count;
    }
  }

  return count;
}

// --------------------------------------------------------------------------

uint32_t guest_memarea_count(struct xmlnode *guest) {
  struct xmlnode *mmu = get_child(guest, STRUCT_MMU);
  struct xmlnode *map;
  uint64_t xmliter_maps;

  iterate_over_children(xmliter_maps, mmu, STRUCT_MAP, map) {
    map->attrs[MAP_ATTR_INDEX].state = ATTRSTATE_MODIFIED;
    map->attrs[MAP_ATTR_INDEX].attr_type = ATTRTYPE_DEC;
    map->attrs[MAP_ATTR_INDEX].value.number = xmliter_maps;
  }

  return xmliter_maps;
}

// --------------------------------------------------------------------------

char *generate_body_memareas(struct xmlnode *scene, struct xmlnode *mmu,
			uint32_t memarea_count, uint32_t cpu,
			struct xmlnode *reference_mmu) {
  char *body;
  struct xmlnode *map;
  struct xmlnode **sorted_maps;
  struct xmlnode *reference_map = NULL;
  char *reference_string;
  uint64_t xmliter_maps;

  body = malloc(memarea_count * 128);
  sorted_maps = malloc(memarea_count * sizeof(void *));

  memset(sorted_maps, 0, memarea_count * sizeof(void *));

  iterate_over_children(xmliter_maps, mmu, STRUCT_MAP, map) {
    if (attr_exists(map->attrs + MAP_ATTR_CPUMAP) &&
	!has_list(map->attrs + MAP_ATTR_CPUMAP, cpu))
      continue;

    sorted_maps[(uint32_t)map->attrs[MAP_ATTR_INDEX].value.number] = map;
  }

  sprintf(body, "{\n");

  for (xmliter_maps = 0; xmliter_maps < memarea_count; xmliter_maps++) {
    struct xmlnode *map_xref;
    uint64_t paddr;
    uint64_t vaddr;
    uint64_t size;

    if (sorted_maps[xmliter_maps] == NULL) {
      sprintf(body + strlen(body), "  { 0, 0, 0, 0, NULL },\n");
      continue;
    }

    map_xref = resolve_idref(scene, sorted_maps[xmliter_maps]->attrs + MAP_ATTR_XREF);
    if (map_xref->node_type == STRUCT_DEVICE) {
      paddr = map_xref->attrs[DEVICE_ATTR_BASE].value.number;
      size = map_xref->attrs[DEVICE_ATTR_SIZE].value.number;
    } else if (map_xref->node_type == STRUCT_MEMREQ) {
      paddr = get_dict_hex(map_xref->attrs + MEMREQ_ATTR_BASE, cpu);
      size = map_xref->attrs[MEMREQ_ATTR_SIZE].value.number;
    }
    vaddr = get_dict_hex(sorted_maps[xmliter_maps]->attrs + MAP_ATTR_BASE, cpu);
    if (attr_exists(sorted_maps[xmliter_maps]->attrs + MAP_ATTR_SUBSIZE)) {
      size = sorted_maps[xmliter_maps]->attrs[MAP_ATTR_SUBSIZE].value.number;
    }
    if (attr_exists(sorted_maps[xmliter_maps]->attrs + MAP_ATTR_OFFSET)) {
      paddr += sorted_maps[xmliter_maps]->attrs[MAP_ATTR_OFFSET].value.number;
      vaddr += sorted_maps[xmliter_maps]->attrs[MAP_ATTR_OFFSET].value.number;
    }

    if (reference_mmu) {
      reference_map = find_mmu_map(reference_mmu, sorted_maps[xmliter_maps]->attrs[MAP_ATTR_XREF].value.string);
    }

    if (reference_map) {
      reference_string = malloc(64);
      sprintf(reference_string, "cpu%d_memareas + %ld", cpu, reference_map->attrs[MAP_ATTR_INDEX].value.number);
    } else {
      reference_string = "NULL";
    }

    sprintf(body + strlen(body), "  { 0x%lx, 0x%lx, 0x%lx, %s, %s },\n",
			paddr, vaddr, size,
			map_flags_string(sorted_maps[xmliter_maps]->attrs[MAP_ATTR_FLAGS].value.string),
			reference_string);
  }
  sprintf(body + strlen(body), "}");

  free(sorted_maps);

  return body;
}
