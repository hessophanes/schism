#include "generate_config.h"

struct vtlb_table_entry {
	const char *format;
	uint32_t total_width;
	uint32_t num_levels;
	uint32_t table_size[8];
	uint32_t entry_size_log2;
};

static const struct vtlb_table_entry vtlb_table[] = {
	{ "arm:short", 32, 2, { 14, 10 }, 2 },
};

const struct vtlb_table_entry *vtlb_format(const char *vtlb_frontend) {
  uint32_t i;

  for (i = 0; i < sizeof(vtlb_table)/sizeof(vtlb_table[0]); i++) {
    if (strcmp(vtlb_frontend, vtlb_table[i].format) == 0)
      return vtlb_table + i;
  }

  return NULL;
}

// --------------------------------------------------------------------------

int generator_vtlb(struct xmlnode *scene, struct xmlnode *guest, struct xmlnode *vdev) {
  struct definition *def;
  uint32_t k, l, m;

  for (k = 0; k < guest->attrs[GUEST_ATTR_CPUMAP].value.list.num; k++) {
    const struct vtlb_table_entry *vtlb_fmt = vtlb_format(vdev->attrs[VDEV_ATTR_FRONTEND].value.string);

    if (!vtlb_fmt) {
      fprintf(stderr, "ERROR: unknown VTLB format.\n");
      return 1;
    }

    if (count_children(vdev, STRUCT_PARAM) != vtlb_fmt->num_levels) {
      fprintf(stderr, "ERROR: number of VTLB <param> does not match pagetable level depth.\n");
      return 1;
    }

    /* VTLB */
    def = add_definition(SECTION_RW, "emulate_vtlb", NULL);
    def->identifier = malloc(64);
    sprintf(def->identifier, "vdev_%s_cpu%d",
			vdev->attrs[VDEV_ATTR_ID].value.string,
			k);
    def->initializer = malloc(256);
    sprintf(def->initializer, "{\n  VTLB_PAGING_FORMAT_ARM_SHORT,\n"
				"  8, vdev_%s_cpu%d_instances,\n"
				"  %d, vdev_%s_cpu%d_levelpools, VTLB_NO_ACTIVE_INSTANCE\n}",
			vdev->attrs[VDEV_ATTR_ID].value.string,
			k,
			vtlb_fmt->num_levels,
			vdev->attrs[VDEV_ATTR_ID].value.string,
			k);

    /* VTLB instances (slots) */
    def = add_definition(SECTION_RW, "emulate_vtlb_instance", NULL);
    def->identifier = malloc(64);
    sprintf(def->identifier, "vdev_%s_cpu%d_instances[%d]",
			vdev->attrs[VDEV_ATTR_ID].value.string,
			k, 8);
    def->initializer = "{}";

    /* VTLB backing pools */
    def = add_definition(SECTION_RO, "emulate_vtlb_pool", NULL);
    def->identifier = malloc(64);
    sprintf(def->identifier, "vdev_%s_cpu%d_levelpools[%d]",
			vdev->attrs[VDEV_ATTR_ID].value.string,
			k,
			vtlb_fmt->num_levels);
    def->initializer = malloc(256 * vtlb_fmt->num_levels);
    sprintf(def->initializer, "{\n");

    for (l = 0; l < vtlb_fmt->num_levels; l++) {
      uint32_t physical_cpu = guest->attrs[GUEST_ATTR_CPUMAP].value.list.elem[k];
      struct xmlnode *level_backing = get_nth_child(vdev, l, STRUCT_PARAM);
      struct xmlnode *backing_map = find_hypervisor_map(scene, level_backing->attrs[PARAM_ATTR_XREF].value.string);
      struct xmlnode *backing_memreq = find_memreq(scene, level_backing->attrs[PARAM_ATTR_XREF].value.string);
      uint32_t shift_accumulated = vtlb_fmt->total_width;

      for (m = 0; m <= l; m++) {
        shift_accumulated -= vtlb_fmt->table_size[m] - vtlb_fmt->entry_size_log2;
      }

      sprintf(def->initializer + strlen(def->initializer),
				"  { %d, %d, 0x%lx, vdev_%s_cpu%d_levelpool%d_bitmap,\n"
				"    %d, 0x%lx, 0x%lx },\n",
			vtlb_fmt->table_size[l],
			vtlb_fmt->entry_size_log2,
			backing_memreq->attrs[MEMREQ_ATTR_SIZE].value.number >> vtlb_fmt->table_size[l],
			vdev->attrs[VDEV_ATTR_ID].value.string,
			k,
			l,
			shift_accumulated,
			get_dict_hex(backing_map->attrs + MAP_ATTR_BASE, physical_cpu),
			get_dict_hex(backing_memreq->attrs + MEMREQ_ATTR_BASE, physical_cpu));
    }

    sprintf(def->initializer + strlen(def->initializer), "}");

    for (l = 0; l < vtlb_fmt->num_levels; l++) {
      struct xmlnode *level_backing = get_nth_child(vdev, l, STRUCT_PARAM);
      struct xmlnode *backing_memreq = find_memreq(scene, level_backing->attrs[PARAM_ATTR_XREF].value.string);
      uint32_t backing_entries = backing_memreq->attrs[MEMREQ_ATTR_SIZE].value.number >> vtlb_fmt->table_size[l];

      def = add_definition(SECTION_RW, "uint32_t", NULL);
      def->identifier = malloc(64);
      sprintf(def->identifier, "vdev_%s_cpu%d_levelpool%d_bitmap[%d]",
			vdev->attrs[VDEV_ATTR_ID].value.string,
			k,
			l,
			(backing_entries + 31) >> 5);
      def->initializer = "{}";
    }
  }

  return 0;
}
