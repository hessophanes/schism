#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "xml.h"
#include "scenario.h"

static int populate_from_hypervisor(struct xmlnode *scene, const char *fn) {
  struct xmlnode *hypervisor, *default_hypervisor;
  struct xmlnode *mmu, *default_mmu;
  struct xmlnode *memreq, *map;
  uint64_t i;

  hypervisor = get_child(scene, STRUCT_HYPERVISOR);

  default_hypervisor = parse_from_file(fn, STRUCT_HYPERVISOR);
  if (default_hypervisor == NULL) {
    fprintf(stderr, "ERROR: cannot load default hypervisor declarations: %s.\n", xml_errormsg);
    return 1;
  }

  iterate_over_children(i, default_hypervisor, STRUCT_MEMREQ, memreq) {
    struct xmlnode *scene_memreq = resolve_idref(scene, memreq->attrs + MEMREQ_ATTR_ID);

    if (scene_memreq == NULL) {
      // copy over
      add_child(hypervisor, memreq);

      fprintf(stderr, "INFO: copied over default <memreq> for \"%s\".\n", memreq->attrs[MEMREQ_ATTR_ID].value.string);
    } else {
      if ((scene_memreq->attrs[MEMREQ_ATTR_FLAGS_DEMAND].state !=
	   memreq->attrs[MEMREQ_ATTR_FLAGS_DEMAND].state) || \
		strcmp(scene_memreq->attrs[MEMREQ_ATTR_FLAGS_DEMAND].value.string,
			memreq->attrs[MEMREQ_ATTR_FLAGS_DEMAND].value.string))
        fprintf(stderr, "WARN: <memreq> for \"%s\" has odd \"flags_demand\" attribute.\n", memreq->attrs[MEMREQ_ATTR_ID].value.string);
      if ((scene_memreq->attrs[MEMREQ_ATTR_FLAGS_PREVENT].state !=
	   memreq->attrs[MEMREQ_ATTR_FLAGS_PREVENT].state) || \
		strcmp(scene_memreq->attrs[MEMREQ_ATTR_FLAGS_PREVENT].value.string,
			memreq->attrs[MEMREQ_ATTR_FLAGS_PREVENT].value.string))
        fprintf(stderr, "WARN: <memreq> for \"%s\" has odd \"flags_prevent\" attribute.\n", memreq->attrs[MEMREQ_ATTR_ID].value.string);
      if ((scene_memreq->attrs[MEMREQ_ATTR_CPUMAP].state !=
	   memreq->attrs[MEMREQ_ATTR_CPUMAP].state) || \
		strcmp(scene_memreq->attrs[MEMREQ_ATTR_CPUMAP].value.string,
			memreq->attrs[MEMREQ_ATTR_CPUMAP].value.string))
        fprintf(stderr, "WARN: <memreq> for \"%s\" has odd \"percore\" attribute.\n", memreq->attrs[MEMREQ_ATTR_ID].value.string);
    }
  }

  default_mmu = get_child(default_hypervisor, STRUCT_MMU);
  mmu = get_child(hypervisor, STRUCT_MMU);

  iterate_over_children(i, default_mmu, STRUCT_MAP, map) {
    struct xmlnode *scene_map = find_hypervisor_map(scene, map->attrs[MAP_ATTR_XREF].value.string);

    if (scene_map == NULL) {
      // copy over
      add_child(mmu, map);

      fprintf(stderr, "INFO: copied over default <map> for \"%s\".\n", map->attrs[MAP_ATTR_XREF].value.string);
    }
  }

  return 0;
}

// ---------------------------------------------------------------------------

int main(int an, char **ac) {
  struct xmlnode *scene, *platform;
  struct xmlnode *guest;
  char fn[128];
  uint64_t i, j;

  if (an != 2) {
    fprintf(stderr, "ERROR: need build directory argument.\n");
    return 1;
  }

  sprintf(fn, "%s/scenario.xml", ac[1]);
  scene = parse_from_file(fn, STRUCT_SCENARIO);
  if (!scene) {
    fprintf(stderr, "ERROR: could not load scenario: %s.\n", xml_errormsg);
    return 1;
  }

  // ---------------------------------------------------------------------------

  // import <board>
  platform = get_child(scene, STRUCT_PLATFORM);
  if (get_child(platform, STRUCT_BOARD) == NULL) {
    struct xmlnode *board;
    if (platform->attrs[PLATFORM_ATTR_BOARD].state != ATTRSTATE_CLEAN) {
      fprintf(stderr, "ERROR: No <board> node and no board attribute.\n");
      return 1;
    }
    sprintf(fn, "xmllib/board_%s.xml", platform->attrs[PLATFORM_ATTR_BOARD].value.string);
    board = parse_from_file(fn, STRUCT_BOARD);
    if (board == NULL) {
      fprintf(stderr, "ERROR: Could not import <board> node: %s.\n", xml_errormsg);
      return 1;
    }
    add_child(platform, board);
    fprintf(stderr, "INFO: imported <board> node.\n");
  }

  // import <arch>
  if (get_child(platform, STRUCT_ARCH) == NULL) {
    struct xmlnode *arch;
    if (platform->attrs[PLATFORM_ATTR_ARCH].state != ATTRSTATE_CLEAN) {
      fprintf(stderr, "ERROR: No <arch> node and no arch attribute.\n");
      return 1;
    }
    sprintf(fn, "xmllib/arch_%s.xml", platform->attrs[PLATFORM_ATTR_ARCH].value.string);
    arch = parse_from_file(fn, STRUCT_ARCH);
    if (arch == NULL) {
      fprintf(stderr, "ERROR: Could not import <arch> node: %s.\n", xml_errormsg);
      return 1;
    }
    add_child(platform, arch);
    fprintf(stderr, "INFO: imported <arch> node.\n");
  }

  // ---------------------------------------------------------------------------

  // set "cpumap" attribute of <guest> nodes
  iterate_over_children(i, scene, STRUCT_GUEST, guest) {
    if (guest->attrs[GUEST_ATTR_CPUMAP].state != ATTRSTATE_CLEAN) {
      uint64_t guest_ncpus = guest->attrs[GUEST_ATTR_NCPUS].value.number;
      guest->attrs[GUEST_ATTR_CPUMAP].value.list.num = guest_ncpus;
      alloc_list(guest->attrs + GUEST_ATTR_CPUMAP);
      for (j = 0; j < guest_ncpus; j++) {
        guest->attrs[GUEST_ATTR_CPUMAP].value.list.elem[j] = j;
      }
      guest->attrs[GUEST_ATTR_CPUMAP].state = ATTRSTATE_MODIFIED;
      fprintf(stderr, "INFO: created default cpumap attribute for guest \"%s\".\n",
		guest->attrs[GUEST_ATTR_ID].value.string);
    }
  }

  // ---------------------------------------------------------------------------

  // grab omitted <memreq> and <map> nodes from standard hypervisor
  sprintf(fn, "xmllib/default_hypervisor_%s.xml", scene->attrs[SCENARIO_ATTR_CBI].value.string);
  if (populate_from_hypervisor(scene, fn)) {
    return 1;
  }

  // ---------------------------------------------------------------------------

  sprintf(fn, "%s/scenario_expanded.xml", ac[1]);
  write_to_file(fn, scene);

  return 0;
}
