#include <sys/time.h>
#include "generate_config.h"

uint64_t config_pa(struct xmlnode *scene) {
  struct xmlnode *memreq;

  memreq = find_memreq(scene, "config_r");
  if (memreq == NULL)
    return (uint64_t)0;
  return get_dict_hex(memreq->attrs + MEMREQ_ATTR_BASE, 0);
}

uint64_t config_va(struct xmlnode *scene) {
  struct xmlnode *map;

  map = find_hypervisor_map(scene, "config_r");
  if (map == NULL)
    return (uint64_t)0;
  return get_dict_hex(map->attrs + MAP_ATTR_BASE, 0);
}

uint64_t ptable_base(struct xmlnode *scene, int32_t cpu) {
  struct xmlnode *hyp_mmu;

  hyp_mmu = get_child(get_child(scene, STRUCT_HYPERVISOR), STRUCT_MMU);
  if (hyp_mmu == NULL)
    return (uint64_t)0;
  return get_dict_hex(hyp_mmu->attrs + MMU_ATTR_BASE, cpu);
}

uint32_t phys_cpu_count(struct xmlnode *scene) {
  struct xmlnode *hypervisor;

  hypervisor = get_child(scene, STRUCT_HYPERVISOR);
  return (uint32_t)hypervisor->attrs[HYPERVISOR_ATTR_NCPUS].value.number;
}

uint32_t vm_cpu_count(struct xmlnode *scene, uint32_t cpu) {
  struct xmlnode *guest;
  uint64_t i;
  uint32_t count = 0;

  iterate_over_children(i, scene, STRUCT_GUEST, guest) {
    if (has_list(guest->attrs + GUEST_ATTR_CPUMAP, cpu)) {
      ++count;
    }
  }

  return count;
}

char *formatted_time() {
  time_t time_now = time(NULL);
  struct tm *time_now_broken = localtime(&time_now);
  char *time_string = malloc(24);

  strftime(time_string, 24, "%Y/%m/%d %H:%M:%S", time_now_broken);
  return time_string;
}

// --------------------------------------------------------------------------

int generate_item_specification(struct xmlnode *scene) {
  struct definition *def;

  def = add_definition(SECTION_RO_HEAD, "specification", "_specification");
  def->initializer = malloc(256);
  sprintf(def->initializer, "{\n  \"PHIDSPEC\", 0x%lx, 0x%lx, 0x%lx, %d,"
				" _specification_cpus, \"%s\"\n}",
		config_pa(scene),
		config_va(scene),
		ptable_base(scene, -1),
		phys_cpu_count(scene),
		formatted_time());

  return 0;
}

int generate_items_specification_cpu(struct xmlnode *scene) {
  struct definition *def;
  uint32_t physical_cpu;

  def = add_definition(SECTION_RO, "specification_cpu", NULL);
  def->identifier = malloc(64);
  sprintf(def->identifier, "_specification_cpus[%d]",
		phys_cpu_count(scene));
  def->initializer = malloc(256 * phys_cpu_count(scene));
  sprintf(def->initializer, "{\n");
  for (physical_cpu = 0; physical_cpu < phys_cpu_count(scene); physical_cpu++) {
    uint32_t num_vm_cpu = vm_cpu_count(scene, physical_cpu);

    sprintf(def->initializer + strlen(def->initializer),
				"  { 0x%lx, %d, cpu%d_memareas, &cpu%d_memtree,",
		ptable_base(scene, physical_cpu),
		hypervisor_memarea_count(scene, physical_cpu),
		physical_cpu,
		physical_cpu);

    if (num_vm_cpu > 0) {
      sprintf(def->initializer + strlen(def->initializer),
				" %d, cpu%d_vm_cpus, {} },\n",
		num_vm_cpu,
		physical_cpu);
    } else {
      sprintf(def->initializer + strlen(def->initializer),
				" 0, NULL, {} },\n");
    }
  }
  sprintf(def->initializer + strlen(def->initializer), "}");

  return 0;
}

int generate_items_cpuX_memareas(struct xmlnode *scene) {
  struct definition *def;
  uint32_t physical_cpu;
  struct xmlnode *hypervisor_mmu;
  char *tree_top_item;
  char *memarea_basename;

  hypervisor_mmu = get_child(get_child(scene, STRUCT_HYPERVISOR), STRUCT_MMU);

  for (physical_cpu = 0; physical_cpu < phys_cpu_count(scene); physical_cpu++) {
    uint32_t memarea_count = hypervisor_memarea_count(scene, physical_cpu);

    def = add_definition(SECTION_RO, "memarea", NULL);
    def->identifier = malloc(64);
    sprintf(def->identifier, "cpu%d_memareas[%d]",
		physical_cpu,
		memarea_count);
    def->initializer = generate_body_memareas(scene, hypervisor_mmu,
				memarea_count, physical_cpu, NULL);

    tree_top_item = malloc(32);
    sprintf(tree_top_item, "cpu%d_memtree", physical_cpu);
    memarea_basename = malloc(32);
    sprintf(memarea_basename, "cpu%d_memareas", physical_cpu);

    generate_memtree(scene, hypervisor_mmu, memarea_count,
			physical_cpu, memarea_basename, tree_top_item, NULL);
  }

  return 0;
}

int generate_items_cpuX_vm_cpus(struct xmlnode *scene) {
  struct definition *def;
  uint32_t physical_cpu;
  struct xmlnode *guest;

  for (physical_cpu = 0; physical_cpu < phys_cpu_count(scene); physical_cpu++) {
    uint32_t num_vm_cpu = vm_cpu_count(scene, physical_cpu);
    uint64_t i;

    if (num_vm_cpu > 0) {
      def = add_definition(SECTION_RO, "vm_cpu *", NULL);
      def->identifier = malloc(64);
      sprintf(def->identifier, "cpu%d_vm_cpus[%d]",
		physical_cpu,
		num_vm_cpu);
      def->initializer = malloc(64 * num_vm_cpu);
      sprintf(def->initializer, "{\n");
      iterate_over_children(i, scene, STRUCT_GUEST, guest) {
        if (!has_list(guest->attrs + GUEST_ATTR_CPUMAP, physical_cpu))
          continue;

        sprintf(def->initializer + strlen(def->initializer),
			"  vm_%s_cpus + %d,\n",
			guest->attrs[GUEST_ATTR_ID].value.string,
			list_index(guest->attrs + GUEST_ATTR_CPUMAP, physical_cpu));
      }
      sprintf(def->initializer + strlen(def->initializer),
			"}");
    }
  }

  return 0;
}

// --------------------------------------------------------------------------

int generate_global_data(struct xmlnode *scene) {
  struct definition *def;

  if (generate_item_specification(scene)) {
    return 1;
  }

  if (generate_items_specification_cpu(scene)) {
    return 1;
  }

  if (generate_items_cpuX_memareas(scene)) {
    return 1;
  }

  if (generate_items_cpuX_vm_cpus(scene)) {
    return 1;
  }

  def = add_definition(SECTION_RW, "uint32_t", "__placeholder_rw");
  def->initializer = "0xbeef";

  def = add_definition(SECTION_RW_SHARED, "uint32_t", "__placeholder_rws");
  def->initializer = "0x7ee1beef";

  return 0;
}
