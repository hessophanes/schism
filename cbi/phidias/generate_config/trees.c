#include "generate_config.h"

// --------------------------------------------------------------------------

void generate_memtree(struct xmlnode *scene, struct xmlnode *mmu,
			uint32_t memarea_count, uint32_t cpu,
			const char *memarea_name,
			const char *top_item_name, const char *top_item_xref) {
  struct xmlnode **sorted_maps;
  struct xmlnode *map;
  uint64_t iter;
  uint32_t j;
  uint32_t sort_interval[2];

  (void)scene;

  sorted_maps = malloc(memarea_count * sizeof(void *));
  memset(sorted_maps, 0, memarea_count * sizeof(void *));

  // iterate over <map>, sort by VA
  iterate_over_children(iter, mmu, STRUCT_MAP, map) {
    if (attr_exists(map->attrs + MAP_ATTR_CPUMAP) && !has_list(map->attrs + MAP_ATTR_CPUMAP, cpu))
      continue;

    for (j = 0; j < memarea_count; j++) {
      if (sorted_maps[j] == NULL) break;
      if (get_dict_hex(map->attrs + MAP_ATTR_BASE, cpu) <
		get_dict_hex(sorted_maps[j]->attrs + MAP_ATTR_BASE, cpu))
        break;
    }
    memmove(sorted_maps + j + 1, sorted_maps + j, (memarea_count - j - 1) * sizeof(void *));
    sorted_maps[j] = map;
  }

  // we may have had holes in our list, so update memarea_count
  for (j = 0; j < memarea_count; j++) {
    if (sorted_maps[j] == NULL) break;
  }
  memarea_count = j;
  
  sort_interval[0] = 0;
  sort_interval[1] = memarea_count;

  // split first time at top_item_xref, else mid
  if (top_item_xref) {
    for (j = 0; j < memarea_count; j++) {
      if (strcmp(sorted_maps[j]->attrs[MAP_ATTR_XREF].value.string, top_item_xref) == 0)
        break;
    }
  } else {
    j = memarea_count/2;
  }
    
  generate_memtree_item(sorted_maps, sort_interval, j, memarea_name, top_item_name);
}

void generate_memtree_item(struct xmlnode **sortlist, uint32_t *sort_interval,
			uint32_t split_item,
			const char *memarea_name, const char *name) {
  struct definition *def;
  char *left_child, *right_child;
  uint32_t recurse_sort_interval[2];

  if (split_item - sort_interval[0] >= 1) {
    left_child = malloc(strlen(name) + 4);
    sprintf(left_child, "&%s_l", name);
  } else
    left_child = "NULL";

  if (sort_interval[1] - split_item > 1) {
    right_child = malloc(strlen(name) + 4);
    sprintf(right_child, "&%s_r", name);
  } else
    right_child = "NULL";

  def = add_definition(SECTION_RO, "tree_memarea", (char *)name);
  def->initializer = malloc(128);
  sprintf(def->initializer, "{ %s, %s, %s + %ld }",
			left_child,
			right_child,
			memarea_name,
			sortlist[split_item]->attrs[MAP_ATTR_INDEX].value.number);

  if (strcmp(left_child, "NULL")) {
    recurse_sort_interval[0] = sort_interval[0];
    recurse_sort_interval[1] = split_item;
    generate_memtree_item(sortlist, recurse_sort_interval,
			recurse_sort_interval[0] + (recurse_sort_interval[1] - recurse_sort_interval[0]) / 2,
			memarea_name, left_child+1);
  }

  if (strcmp(right_child, "NULL")) {
    recurse_sort_interval[0] = split_item+1;
    recurse_sort_interval[1] = sort_interval[1];
    generate_memtree_item(sortlist, recurse_sort_interval,
			recurse_sort_interval[0] + (recurse_sort_interval[1] - recurse_sort_interval[0]) / 2,
			memarea_name, right_child+1);
  }
}

// --------------------------------------------------------------------------

void generate_emulatetree(struct xmlnode *scene, struct xmlnode *guest,
			uint32_t emulate_count, uint32_t cpu,
			const char *emulate_name,
			const char *top_item_name) {
  struct xmlnode **sorted_emulates;
  uint32_t *indices;
  struct xmlnode *vdev, *emulate;
  uint64_t iter, iter2;
  uint32_t count = 0;
  uint32_t j;
  uint32_t sort_interval[2];

  (void)scene; (void)cpu;

  sorted_emulates = malloc(emulate_count * sizeof(void *));
  memset(sorted_emulates, 0, emulate_count * sizeof(void *));
  indices = malloc(emulate_count * sizeof(uint32_t));
  memset(indices, 0, emulate_count * sizeof(uint32_t));

  // iterate over <emulate>, sort by VA
  iterate_over_children(iter, guest, STRUCT_VDEV, vdev) {
    iterate_over_children(iter2, vdev, STRUCT_EMULATE, emulate) {
      for (j = 0; j < emulate_count; j++) {
        if (sorted_emulates[j] == NULL) break;
        if (emulate->attrs[EMULATE_ATTR_BASE].value.number <
		sorted_emulates[j]->attrs[EMULATE_ATTR_BASE].value.number)
          break;
      }
      memmove(sorted_emulates + j + 1, sorted_emulates + j, (emulate_count - j - 1) * sizeof(void *));
      memmove(indices + j + 1, indices + j, (emulate_count - j - 1) * sizeof(uint32_t));
      sorted_emulates[j] = emulate;
      indices[j] = count + (uint32_t)iter2;
    }
    count += (count_children(vdev, STRUCT_EMULATE) ?: 1);
  }

  // there may have been <vdev> nodes without <emulate>, so update emulate_count
  for (j = 0; j < emulate_count; j++) {
    if (sorted_emulates[j] == NULL) break;
  }
  emulate_count = j;
  
  sort_interval[0] = 0;
  sort_interval[1] = emulate_count;

  // start splitting in half and recursing
  j = emulate_count/2;
    
  generate_emulatetree_item(sorted_emulates, indices, sort_interval, j, emulate_name, top_item_name);
}

void generate_emulatetree_item(struct xmlnode **sortlist, uint32_t *indices,
			uint32_t *sort_interval, uint32_t split_item,
			const char *emulate_name, const char *name) {
  struct definition *def;
  char *left_child, *right_child;
  uint32_t recurse_sort_interval[2];

  if (split_item - sort_interval[0] >= 1) {
    left_child = malloc(strlen(name) + 4);
    sprintf(left_child, "&%s_l", name);
  } else
    left_child = "NULL";

  if (sort_interval[1] - split_item > 1) {
    right_child = malloc(strlen(name) + 4);
    sprintf(right_child, "&%s_r", name);
  } else
    right_child = "NULL";

  def = add_definition(SECTION_RO, "tree_emulate", (char *)name);
  def->initializer = malloc(128);
  sprintf(def->initializer, "{ %s, %s, %s + %d }",
			left_child,
			right_child,
			emulate_name,
			indices[split_item]);

  if (strcmp(left_child, "NULL")) {
    recurse_sort_interval[0] = sort_interval[0];
    recurse_sort_interval[1] = split_item;
    generate_emulatetree_item(sortlist, indices, recurse_sort_interval,
			recurse_sort_interval[0] + (recurse_sort_interval[1] - recurse_sort_interval[0]) / 2,
			emulate_name, left_child+1);
  }

  if (strcmp(right_child, "NULL")) {
    recurse_sort_interval[0] = split_item+1;
    recurse_sort_interval[1] = sort_interval[1];
    generate_emulatetree_item(sortlist, indices, recurse_sort_interval,
			recurse_sort_interval[0] + (recurse_sort_interval[1] - recurse_sort_interval[0]) / 2,
			emulate_name, right_child+1);
  }
}
