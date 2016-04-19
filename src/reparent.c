#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "xml.h"
#include "scenario.h"

struct xmlnode *default_memory = NULL;

// ---------------------------------------------------------------------------

static void delete_children(struct xmlnode *node, uint32_t child_type) {
  uint64_t i;

  for (i = 0; i < node->num_children; ) {
    if (node->children[i].node_type == child_type) {
      memmove(node->children + i, node->children + i+1, (node->num_children - (i+1)) * sizeof(struct xmlnode));
      node->num_children--;
    } else {
      i++;
    }
  }
}

static int check_memreqs(struct xmlnode *scene, struct xmlnode *memreq, struct xmlattr *cpumap) {
  uint64_t i;
  struct xmlnode *map, *matched_map = NULL;
  struct xmlnode *mmu;

  mmu = get_child(get_child(scene, STRUCT_HYPERVISOR), STRUCT_MMU);

  // set default "on" attribute for <memreq> nodes
  if (memreq->attrs[MEMREQ_ATTR_ON].state != ATTRSTATE_CLEAN) {
    memreq->attrs[MEMREQ_ATTR_ON].value.string = default_memory->attrs[MEMORY_ATTR_ID].value.string;
    memreq->attrs[MEMREQ_ATTR_ON].state = ATTRSTATE_MODIFIED;
    memreq->attrs[MEMREQ_ATTR_ON].attr_type = ATTRTYPE_IDREF;
    fprintf(stderr, "INFO: Added \"on\" attribute to <memreq> node \"%s\".\n",
                        memreq->attrs[MEMREQ_ATTR_ID].value.string);
  }

  iterate_over_children(i, mmu, STRUCT_MAP, map) {
    if (strcmp(map->attrs[MAP_ATTR_XREF].value.string, memreq->attrs[MEMREQ_ATTR_ID].value.string) == 0) {
      matched_map = map;
      break;
    }
  }

  if (matched_map == NULL) {
    if (attr_exists(memreq->attrs + MEMREQ_ATTR_FLAGS_DEMAND) &&
	(strlen(memreq->attrs[MEMREQ_ATTR_FLAGS_DEMAND].value.string) > 0)) {
      // <memreq> not contained in <mmu>, but requires it --> add node
      matched_map = malloc(sizeof(struct xmlnode));
      memset(matched_map, 0, sizeof(struct xmlnode));
      matched_map->node_type = STRUCT_MAP;
      matched_map->attrs[MAP_ATTR_XREF].value.string = memreq->attrs[MEMREQ_ATTR_ID].value.string;
      matched_map->attrs[MAP_ATTR_XREF].state = ATTRSTATE_MODIFIED;
      matched_map->attrs[MAP_ATTR_XREF].attr_type = ATTRTYPE_IDREF;
      matched_map->attrs[MAP_ATTR_FLAGS].value.string = memreq->attrs[MEMREQ_ATTR_FLAGS_DEMAND].value.string;
      matched_map->attrs[MAP_ATTR_FLAGS].state = ATTRSTATE_MODIFIED;
      matched_map->attrs[MAP_ATTR_FLAGS].attr_type = ATTRTYPE_STRING;

      add_child(mmu, matched_map);
      matched_map = mmu->children + mmu->num_children - 1;

      fprintf(stderr, "INFO: Added <map> for required <memreq> \"%s\".\n", memreq->attrs[MEMREQ_ATTR_ID].value.string);
    }
  }

  // if <map> is constrained by a guest cpumap, constrain its mapping for the hypervisor accordingly,
  // even if the mapping is flagged global; the 'g' bit specifies "all guest VCPUs" and furthermore
  // requires placement in the hypervisor global window, it doesn't specify "all host CPUs"
  if (matched_map && cpumap) {
    matched_map->attrs[MAP_ATTR_CPUMAP].value.list.num = cpumap->value.list.num;
    matched_map->attrs[MAP_ATTR_CPUMAP].value.list.elem = cpumap->value.list.elem;
    matched_map->attrs[MAP_ATTR_CPUMAP].state = ATTRSTATE_MODIFIED;
    matched_map->attrs[MAP_ATTR_CPUMAP].attr_type = ATTRTYPE_LIST;
    fprintf(stderr, "INFO: Constrained <map> for guest <memreq> \"%s\" to the guest's cpumap.\n",
		matched_map->attrs[MAP_ATTR_XREF].value.string);
  }

  return 0;
}

static int check_map_attributes(struct xmlnode *scene) {
  uint64_t i;
  unsigned int j;
  struct xmlnode *mmu, *map;
  struct xmlnode *memdev;		// memreq or device

  mmu = get_child(get_child(scene, STRUCT_HYPERVISOR), STRUCT_MMU);

  iterate_over_children(i, mmu, STRUCT_MAP, map) {
    char *flags_prevent, *flags_demand;
    char *flags_new;

    memdev = resolve_idref(scene, map->attrs + MAP_ATTR_XREF);

    if (memdev == NULL) {
      fprintf(stderr, "ERROR: neither <memreq> nor <device> found for <map> \"%s\".\n",
			map->attrs[MAP_ATTR_XREF].value.string);
      return 1;
    } else if (memdev->node_type == STRUCT_MEMREQ) {
      flags_prevent = (memdev->attrs[MEMREQ_ATTR_FLAGS_PREVENT].state == ATTRSTATE_CLEAN) ?
			(memdev->attrs[MEMREQ_ATTR_FLAGS_PREVENT].value.string) : "";
      flags_demand = (memdev->attrs[MEMREQ_ATTR_FLAGS_DEMAND].state == ATTRSTATE_CLEAN) ?
			(memdev->attrs[MEMREQ_ATTR_FLAGS_DEMAND].value.string) : "";
    } else if (memdev->node_type == STRUCT_DEVICE) {
      flags_prevent = "xus";
      flags_demand = "rgd";
    } else {
      fprintf(stderr, "ERROR: <map> xref \"%s\" resolves to strange XML node.\n",
			map->attrs[MAP_ATTR_XREF].value.string);
      return 1;
    }

    if ((map->attrs[MAP_ATTR_FLAGS].state == ATTRSTATE_CLEAN) && \
	(strcspn(map->attrs[MAP_ATTR_FLAGS].value.string, flags_prevent) != strlen(map->attrs[MAP_ATTR_FLAGS].value.string))) {
      fprintf(stderr, "ERROR: <map> \"%s\" contains forbidden flags.\n", map->attrs[MAP_ATTR_XREF].value.string);
      return 1;
    }

    flags_new = (map->attrs[MAP_ATTR_FLAGS].state == ATTRSTATE_CLEAN) ?
		(map->attrs[MAP_ATTR_FLAGS].value.string) : flags_demand;

    for (j = 0; j < strlen(flags_demand); j++) {
      if (!strchr(flags_new, flags_demand[j])) {
        char *tmp = malloc(strlen(flags_new) + 4);
        sprintf(tmp, "%s%c", flags_new, flags_demand[j]);
        flags_new = tmp;
        fprintf(stderr, "INFO: Added required flag '%c' to <map> \"%s\".\n",
                        flags_demand[j], map->attrs[MAP_ATTR_XREF].value.string);
      }
    }

    map->attrs[MAP_ATTR_FLAGS].value.string = flags_new;
    map->attrs[MAP_ATTR_FLAGS].state = ATTRSTATE_MODIFIED;
  }

  return 0;
}

// ---------------------------------------------------------------------------

int main(int an, char **ac) {
  struct xmlnode *scene, *hypervisor, *platform, *board, *memory;
  struct xmlnode *memreq, *guest;
  char fn[128];
  uint64_t i, j, k;
  struct xmlattr percore_all;

  if (an != 2) {
    fprintf(stderr, "ERROR: need build directory argument.\n");
    return 1;
  }

  sprintf(fn, "%s/scenario_expanded.xml", ac[1]);
  scene = parse_from_file(fn, STRUCT_SCENARIO);
  if (!scene) {
    fprintf(stderr, "ERROR: could not load scenario: %s.\n", xml_errormsg);
    return 1;
  }

  // ---------------------------------------------------------------------------

  // determine default <memory> node
  platform = get_child(scene, STRUCT_PLATFORM);
  board = get_child(platform, STRUCT_BOARD);

  iterate_over_children(i, board, STRUCT_MEMORY, memory) {
    uint64_t default_memory_size = default_memory ? default_memory->attrs[MEMORY_ATTR_SIZE].value.number : 0;
    uint64_t this_memory_size = memory->attrs[MEMORY_ATTR_SIZE].value.number;
    if (this_memory_size > default_memory_size)
      default_memory = memory;
  }
  if (!default_memory) {
    fprintf(stderr, "ERROR: no <memory> node found.\n");
    return 1;
  }

  // ---------------------------------------------------------------------------

  // check <memreq> flags, create implicit <map> nodes, determine <memory> node
  hypervisor = get_child(scene, STRUCT_HYPERVISOR);

  k = 0;
  iterate_over_children(i, hypervisor, STRUCT_MEMREQ, memreq) {
    check_memreqs(scene, memreq, NULL);
  }
  k += i;

  iterate_over_children(i, scene, STRUCT_GUEST, guest) {
    iterate_over_children(j, guest, STRUCT_MEMREQ, memreq) {
      check_memreqs(scene, memreq, guest->attrs + GUEST_ATTR_CPUMAP);
    }
    k += j;
  }

  // ---------------------------------------------------------------------------

  // reparent <memreq> nodes to associated <memory> node
  percore_all.value.list.num = (uint32_t)hypervisor->attrs[HYPERVISOR_ATTR_NCPUS].value.number;
  alloc_list(&percore_all);
  for (i = 0; i < percore_all.value.list.num; i++) {
    percore_all.value.list.elem[(uint32_t)i] = (uint32_t)i;
  }

  iterate_over_children(i, hypervisor, STRUCT_MEMREQ, memreq) {
    struct xmlnode *memory = resolve_idref(scene, memreq->attrs + MEMREQ_ATTR_ON);
    if (memory == NULL) {
      fprintf(stderr, "ERROR: unknown <memory> node \"%s\".\n", memreq->attrs[MEMREQ_ATTR_ON].value.string);
      return 1;
    }
    if ((memreq->attrs[MEMREQ_ATTR_CPUMAP].state == ATTRSTATE_CLEAN) &&
	(memreq->attrs[MEMREQ_ATTR_CPUMAP].value.list.elem == ATTR_WILDCARD)) {
      memreq->attrs[MEMREQ_ATTR_CPUMAP].value.list.num = percore_all.value.list.num;
      memreq->attrs[MEMREQ_ATTR_CPUMAP].value.list.elem = percore_all.value.list.elem;
      memreq->attrs[MEMREQ_ATTR_CPUMAP].state = ATTRSTATE_MODIFIED;
      fprintf(stderr, "INFO: expanded percore attribute when reparenting memreq \"%s\".\n",
		memreq->attrs[MEMREQ_ATTR_ID].value.string);
    }
    add_child(memory, memreq);
  }
  delete_children(hypervisor, STRUCT_MEMREQ);

  iterate_over_children(i, scene, STRUCT_GUEST, guest) {
    iterate_over_children(j, guest, STRUCT_MEMREQ, memreq) {
      struct xmlnode *memory = resolve_idref(scene, memreq->attrs + MEMREQ_ATTR_ON);
      if (memory == NULL) {
        fprintf(stderr, "ERROR: unknown <memory> node \"%s\".\n", memreq->attrs[MEMREQ_ATTR_ON].value.string);
        return 1;
      }

      if ((memreq->attrs[MEMREQ_ATTR_CPUMAP].state == ATTRSTATE_CLEAN) &&
	  (memreq->attrs[MEMREQ_ATTR_CPUMAP].value.list.elem != ATTR_WILDCARD)) {
        fprintf(stderr, "ERROR: cannot reparent node with custom percore attribute.\n");
        return 1;
      }
      memreq->attrs[MEMREQ_ATTR_CPUMAP].value.list.num = guest->attrs[GUEST_ATTR_CPUMAP].value.list.num;
      memreq->attrs[MEMREQ_ATTR_CPUMAP].value.list.elem = guest->attrs[GUEST_ATTR_CPUMAP].value.list.elem;
      memreq->attrs[MEMREQ_ATTR_CPUMAP].state = ATTRSTATE_MODIFIED;
      memreq->attrs[MEMREQ_ATTR_CPUMAP].attr_type = ATTRTYPE_LIST;
      fprintf(stderr, "INFO: reduced percore attribute when reparenting memreq \"%s\".\n",
		memreq->attrs[MEMREQ_ATTR_ID].value.string);

      add_child(memory, memreq);
    }
    delete_children(guest, STRUCT_MEMREQ);
  }

  // ---------------------------------------------------------------------------

  // check <map> nodes in hypervisor again, enforce flags
  check_map_attributes(scene);

  // ---------------------------------------------------------------------------

  sprintf(fn, "%s/scenario_reparented.xml", ac[1]);
  write_to_file(fn, scene);

  return 0;
}
