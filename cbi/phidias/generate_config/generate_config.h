#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include "xml.h"
#include "scenario.h"

enum section {
  SECTION_RO_HEAD = 1,
  SECTION_RO = 2,
  SECTION_RW = 3,
  SECTION_RW_SHARED = 4,
  SECTION_EXTERN = 5
};

struct definition {
  char *type;
  enum section section;
  char *identifier;
  char *initializer;
};

struct percpu_data_t {
  uint32_t num_vcpus;
};

struct nametable_entry {
  const char *arch_match;
  const char *board_match;
  const char *name;
  unsigned int index;
};

#define	NONCOREMAPS_BASE	32

// definitions.c

extern struct definition *definitions;
extern unsigned int num_definitions;

extern struct definition *add_definition(enum section section, char *type, char *identifier);
extern int emit_definitions(const char *filename, const char *includes);

// global.c

extern int generate_item_specification(struct xmlnode *scene);
extern int generate_items_specification_cpu(struct xmlnode *scene);

extern int generate_global_data(struct xmlnode *scene);

// main.c

extern int emit_config_base_address(struct xmlnode *scene, const char *filename);
extern int main(int, char **);

// mmu.c

extern uint32_t hypervisor_memarea_index(struct xmlnode *map);
extern uint32_t hypervisor_memarea_count(struct xmlnode *scene, uint32_t physical_cpu);
extern uint32_t guest_memarea_count(struct xmlnode *guest);

extern char *generate_body_memareas(struct xmlnode *scene, struct xmlnode *mmu,
                        uint32_t memarea_count, uint32_t cpu,
			struct xmlnode *reference_mmu);

// trees.c

extern void generate_memtree(struct xmlnode *scene, struct xmlnode *mmu,
			uint32_t memarea_count, uint32_t cpu,
			const char *memarea_name,
			const char *top_item_name, const char *top_item_xref);
extern void generate_memtree_item(struct xmlnode **sortlist, uint32_t *sort_interval,
			uint32_t split_item,
			const char *memarea_name, const char *name);

extern void generate_emulatetree(struct xmlnode *scene, struct xmlnode *guest,
			uint32_t emulate_count, uint32_t cpu,
			const char *emulate_name,
			const char *top_item_name);
extern void generate_emulatetree_item(struct xmlnode **sortlist, uint32_t *indices,
			uint32_t *sort_interval, uint32_t split_item,
			const char *emulate_name, const char *name);

// vdevs.c

typedef int (vdev_generator)(struct xmlnode *scene, struct xmlnode *guest, struct xmlnode *vdev);

extern const struct vdev_table_entry *walk_vdev_table(struct xmlnode *vdev);
extern const char *emulate_type_string(struct xmlnode *vdev);
extern int is_vdev_global(struct xmlnode *vdev);

extern char *vm_master_vdev_name(struct xmlnode *guest, uint32_t cpu, const char *category);

static inline char *vm_master_vdev_name_vtlb(struct xmlnode *guest, uint32_t cpu) {
	return vm_master_vdev_name(guest, cpu, "vtlb");
}
static inline char *vm_master_vdev_name_irq(struct xmlnode *guest, uint32_t cpu) {
	return vm_master_vdev_name(guest, cpu, "irq_controller");
}
static inline char *vm_master_vdev_name_uart(struct xmlnode *guest, uint32_t cpu) {
	return vm_master_vdev_name(guest, cpu, "serial");
}

extern int generate_items_vmX_vdevs(struct xmlnode *scene);

// vdev-vtlb.c

extern vdev_generator generator_vtlb;

// vdev-memory.c

extern vdev_generator generator_memory32;

// vdev-irq.c

extern vdev_generator generator_irq_gic;

// vdev-uart.c

extern vdev_generator generator_uart_pl011;

// vdev-timer.c

extern vdev_generator generator_timer_sp804;
extern vdev_generator generator_timer_mpcore;

// vdev-clock.c

extern vdev_generator generator_clock_mpcore;

// vm.c

extern int generate_vm_data(struct xmlnode *scene);

// utilities

static inline uint32_t _log2(uint64_t val) {
  uint32_t ret = 0;

  while (val > 1) {
    if (val & 1) {
      fprintf(stderr, "ERROR: _log2() on a value that's not a power of two.\n");
      return ~0;
    }
    val >>= 1;
    ret++;
  }

  return ret;
}

#define	ARRAYLEN(arr)		(sizeof(arr) / sizeof(*arr))
