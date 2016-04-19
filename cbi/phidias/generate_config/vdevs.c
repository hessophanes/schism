#include "generate_config.h"

struct vdev_table_entry {
	const char *type;
	const char *frontend;
	const char *phidias_type;
	int is_global;
	uint32_t expected_bars;
	vdev_generator *generator;
};

static const struct vdev_table_entry vdev_table[] = {
	{ "timer", "sp804", "EMULATE_TYPE_TIMER_SP804", 1, 1, &generator_timer_sp804 },
	{ "timer", "mpcore", "EMULATE_TYPE_TIMER_MPCORE", 0, 1, &generator_timer_mpcore },
	{ "clock", "mpcore", "EMULATE_TYPE_CLOCK_MPCORE", 1, 1, &generator_clock_mpcore },
	{ "vtlb", NULL, "EMULATE_TYPE_VTLB", 0, 0, &generator_vtlb },
	{ "irq_controller", "arm_gic", "EMULATE_TYPE_IRQ_GIC", 0, 2, &generator_irq_gic },
	{ "serial", "16550", "EMULATE_TYPE_UART_16450", 1, 1, NULL },
	{ "serial", "pl011", "EMULATE_TYPE_UART_PL011", 1, 1, &generator_uart_pl011 },
	{ "memory32", NULL, "EMULATE_TYPE_MEMORY_32BIT", 1, 1, &generator_memory32 },
};

const struct vdev_table_entry *walk_vdev_table(struct xmlnode *vdev) {
  uint32_t i;

  for (i = 0; i < sizeof(vdev_table)/sizeof(vdev_table[0]); i++) {
    if (strcmp(vdev->attrs[VDEV_ATTR_TYPE].value.string,
		vdev_table[i].type) != 0)
      continue;

    if (vdev_table[i].frontend == NULL)
      return vdev_table + i;

    if (strcmp(vdev->attrs[VDEV_ATTR_FRONTEND].value.string,
		vdev_table[i].frontend) == 0)
      return vdev_table + i;
  }

  return NULL;
}

const char *emulate_type_string(struct xmlnode *vdev) {
  const struct vdev_table_entry *entry = walk_vdev_table(vdev);

  if (entry)
    return entry->phidias_type;

  return "UNKNOWN_TYPE";
}

int is_vdev_global(struct xmlnode *vdev) {
  const struct vdev_table_entry *entry = walk_vdev_table(vdev);

  if (entry)
    return entry->is_global;

  return 0;
}

char *vm_master_vdev_name(struct xmlnode *guest, uint32_t cpu, const char *category) {
  uint64_t i;
  struct xmlnode *vdev;
  char *name;
  uint32_t counter = 0;

  iterate_over_children(i, guest, STRUCT_VDEV, vdev) {
    uint32_t emulates = count_children(vdev, STRUCT_EMULATE);
    
    if ( (strcmp(vdev->attrs[VDEV_ATTR_TYPE].value.string, category) == 0) &&
	(attr_exists(vdev->attrs + VDEV_ATTR_MASTER)) ) {
      name = malloc(32);
      sprintf(name, "vm_%s_cpu%d_emulates + %ld",
                        guest->attrs[GUEST_ATTR_ID].value.string, cpu, i);
      return name;
    }

    counter += (emulates ? emulates : 1);
  }

  return "NULL";
}

// --------------------------------------------------------------------------

int generate_items_vmX_vdevs(struct xmlnode *scene) {
  struct xmlnode *guest;
  struct xmlnode *vdev;
  uint64_t i, j;

  iterate_over_children(i, scene, STRUCT_GUEST, guest) {
    iterate_over_children(j, guest, STRUCT_VDEV, vdev) {
      const struct vdev_table_entry *entry = walk_vdev_table(vdev);

      if (!entry) {
        fprintf(stderr, "ERROR: no vdev entry for %s.\n", vdev->attrs[VDEV_ATTR_ID].value.string);
        return 1;
      }
      if (!entry->generator) {
        fprintf(stderr, "ERROR: no vdev generator for %s.\n", vdev->attrs[VDEV_ATTR_ID].value.string);
        return 1;
      }
      if (entry->expected_bars != count_children(vdev, STRUCT_EMULATE)) {
        fprintf(stderr, "ERROR: number of BARs doesn't match expectation for %s (%d != %d).\n",
			vdev->attrs[VDEV_ATTR_ID].value.string,
			count_children(vdev, STRUCT_EMULATE), entry->expected_bars);
        return 1;
      }

      if (entry->generator(scene, guest, vdev))
          return 1;
    }
  }

  return 0;
}
