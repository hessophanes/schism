#include "generate_config.h"

uint32_t vm_emulate_count(struct xmlnode *guest) {
  uint64_t i;
  uint32_t counter = 0;
  struct xmlnode *vdev;

  iterate_over_children(i, guest, STRUCT_VDEV, vdev) {
    uint32_t emulates = count_children(vdev, STRUCT_EMULATE);
    counter += (emulates ? emulates : 1);
  }

  return counter;
}

uint32_t vm_capability_count(struct xmlnode *guest) {
  struct xmlnode *init = get_child(guest, STRUCT_INIT);

  return count_children(init, STRUCT_CAP);
}

uint32_t vm_copyins_count(struct xmlnode *guest) {
  struct xmlnode *init = get_child(guest, STRUCT_INIT);

  return count_children(init, STRUCT_COPY);
}

uint32_t vm_memarea_count(struct xmlnode *guest) {
  struct xmlnode *mmu = get_child(guest, STRUCT_MMU);

  return count_children(mmu, STRUCT_MAP);
}

struct xmlnode *find_hypervisor_archpage_map(struct xmlnode *scene, struct xmlnode *guest) {
  struct xmlnode *init = get_child(guest, STRUCT_INIT);

  return find_hypervisor_map(scene, init->attrs[INIT_ATTR_ARCH_PAGE].value.string);
}

const char *capability_type_string(const char *cap_type) {
  if (strcmp(cap_type, "ipc") == 0) {
    return "CAPABILITY_IPC";
  }

  return NULL;
}

// --------------------------------------------------------------------------

int generate_items_vmX_cpuY_memareas(struct xmlnode *scene) {
  struct definition *def;
  struct xmlnode *guest;
  struct xmlnode *hypervisor_mmu = get_child(get_child(scene, STRUCT_HYPERVISOR), STRUCT_MMU);
  uint64_t i;
  uint32_t j;
  char *memarea_name;
  char *top_item_name;

  iterate_over_children(i, scene, STRUCT_GUEST, guest) {
    struct xmlnode *mmu = get_child(guest, STRUCT_MMU);
    uint32_t map_count = guest_memarea_count(guest);

    for (j = 0; j < guest->attrs[GUEST_ATTR_CPUMAP].value.list.num; j++) {
      def = add_definition(SECTION_RO, "memarea", NULL);
      def->identifier = malloc(64);
      sprintf(def->identifier, "vm_%s_cpu%d_memareas[%d]",
			guest->attrs[GUEST_ATTR_ID].value.string,
			j,
			map_count);
      def->initializer = generate_body_memareas(scene, mmu,
				map_count, j,
				hypervisor_mmu);

      memarea_name = malloc(32);
      sprintf(memarea_name, "vm_%s_cpu%d_memareas", guest->attrs[GUEST_ATTR_ID].value.string, j);
      top_item_name = malloc(32);
      sprintf(top_item_name, "vm_%s_cpu%d_memtree", guest->attrs[GUEST_ATTR_ID].value.string, j);
      generate_memtree(scene, mmu, map_count, j, memarea_name, top_item_name, NULL);
    }
  }

  return 0;
}

int generate_items_vmX_capabilities(struct xmlnode *scene) {
  struct definition *def;
  struct xmlnode *guest, *init, *cap;
  uint64_t i, j;

  iterate_over_children(i, scene, STRUCT_GUEST, guest) {
    init = get_child(guest, STRUCT_INIT);

    def = add_definition(SECTION_RO, "capability", NULL);
    def->identifier = malloc(64);
    sprintf(def->identifier, "vm_%s_capabilities[%d]",
			guest->attrs[GUEST_ATTR_ID].value.string,
			vm_capability_count(guest));
    def->initializer = malloc(64 * vm_capability_count(guest));
    sprintf(def->initializer, "{\n");

    iterate_over_children(j, init, STRUCT_CAP, cap) {
      struct xmlnode *target_guest = resolve_idref(scene, cap->attrs + CAP_ATTR_TARGET_XREF);

      sprintf(def->initializer + strlen(def->initializer),
				"  { &vm_%s, %s, 0x%lx },\n",
			target_guest->attrs[GUEST_ATTR_ID].value.string,
			capability_type_string(cap->attrs[CAP_ATTR_TYPE].value.string),
			cap->attrs[CAP_ATTR_PARAM].value.number);
    }

    sprintf(def->initializer + strlen(def->initializer), "}");
  }

  return 0;
}

int generate_items_vmX_copyins(struct xmlnode *scene) {
  struct definition *def;
  struct xmlnode *guest, *init, *copy;
  uint64_t i, j;
  uint32_t guest_primary_physcpu;
  struct xmlnode *copy_src = find_hypervisor_map(scene, "blob");
  uint64_t src_baseaddr;

  iterate_over_children(i, scene, STRUCT_GUEST, guest) {
    init = get_child(guest, STRUCT_INIT);
    guest_primary_physcpu = guest->attrs[GUEST_ATTR_CPUMAP].value.list.elem[0];
    src_baseaddr = get_dict_hex(copy_src->attrs + MAP_ATTR_BASE, guest_primary_physcpu);

    def = add_definition(SECTION_RO, "vm_copyin", NULL);
    def->identifier = malloc(64);
    sprintf(def->identifier, "vm_%s_copyins[%d]",
                        guest->attrs[GUEST_ATTR_ID].value.string,
                        vm_copyins_count(guest));
    def->initializer = malloc(96 * vm_copyins_count(guest));
    sprintf(def->initializer, "{\n");

    iterate_over_children(j, init, STRUCT_COPY, copy) {
      struct xmlnode *copy_dst = find_hypervisor_map(scene, copy->attrs[COPY_ATTR_DREF].value.string);
      struct xmlnode *src_file = resolve_idref(scene, copy->attrs + COPY_ATTR_XREF);

      sprintf(def->initializer + strlen(def->initializer),
                                "  { 0x%lx, 0x%lx, 0x%lx },\n",
			get_dict_hex(copy_dst->attrs + MAP_ATTR_BASE, guest_primary_physcpu) +
				copy->attrs[COPY_ATTR_OFFSET].value.number,
                        src_baseaddr + src_file->attrs[FILE_ATTR_OFFSET].value.number,
                        src_file->attrs[FILE_ATTR_SIZE].value.number);
    }

    sprintf(def->initializer + strlen(def->initializer), "}");
  }

  return 0;
}

int generate_items_vmX_cpuY_emulates(struct xmlnode *scene) {
  struct definition *def;
  struct xmlnode *guest, *vdev, *emulate;
  uint64_t i, k, l;
  uint32_t j;
  uint32_t emulate_count;

  iterate_over_children(i, scene, STRUCT_GUEST, guest) {
    emulate_count = vm_emulate_count(guest);
    for (j = 0; j < guest->attrs[GUEST_ATTR_CPUMAP].value.list.num; j++) {
      def = add_definition(SECTION_RO, "emulate", NULL);
      def->identifier = malloc(64);
      sprintf(def->identifier, "vm_%s_cpu%d_emulates[%d]",
			guest->attrs[GUEST_ATTR_ID].value.string,
			j,
			emulate_count);
      def->initializer = malloc(256 * vm_emulate_count(guest));
      sprintf(def->initializer, "{\n");

      iterate_over_children(k, guest, STRUCT_VDEV, vdev) {
        int is_global = is_vdev_global(vdev);

        if (count_children(vdev, STRUCT_EMULATE) == 0) {
          sprintf(def->initializer + strlen(def->initializer),
				"  { 0, 0, %s,",
			emulate_type_string(vdev));
          if (is_global)
            sprintf(def->initializer + strlen(def->initializer),
				" { &vdev_%s }, 0 },\n",
			vdev->attrs[VDEV_ATTR_ID].value.string);
          else
            sprintf(def->initializer + strlen(def->initializer),
				" { &vdev_%s_cpu%d }, 0 },\n",
			vdev->attrs[VDEV_ATTR_ID].value.string,
			j);
          continue;
        }

        iterate_over_children(l, vdev, STRUCT_EMULATE, emulate) {
          sprintf(def->initializer + strlen(def->initializer),
				"  { 0x%lx, 0x%lx, %s,",
			emulate->attrs[EMULATE_ATTR_BASE].value.number,
			emulate->attrs[EMULATE_ATTR_SIZE].value.number,
			emulate_type_string(vdev));
          if (is_global)
            sprintf(def->initializer + strlen(def->initializer),
				" { &vdev_%s }, %ld },\n",
			vdev->attrs[VDEV_ATTR_ID].value.string,
			l);
          else
            sprintf(def->initializer + strlen(def->initializer),
				" { &vdev_%s_cpu%d }, %ld },\n",
			vdev->attrs[VDEV_ATTR_ID].value.string,
			j,
			l);
        }
      }
    }

    sprintf(def->initializer + strlen(def->initializer), "}");

    for (j = 0; j < guest->attrs[GUEST_ATTR_CPUMAP].value.list.num; j++) {
      char *ref_name = malloc(64);
      char *tree_top_name = malloc(64);
      sprintf(ref_name, "vm_%s_cpu%d_emulates",
			guest->attrs[GUEST_ATTR_ID].value.string,
			j);
      sprintf(tree_top_name, "vm_%s_cpu%d_emulatetree",
			guest->attrs[GUEST_ATTR_ID].value.string,
			j);

      generate_emulatetree(scene, guest, emulate_count, j, ref_name, tree_top_name);
    }
  }

  return 0;
}

// --------------------------------------------------------------------------

int generate_items_vmX_cpus(struct xmlnode *scene) {
  struct definition *def;
  struct xmlnode *guest, *mmu, *archpage_map;
  uint64_t i;
  uint32_t j;

  iterate_over_children(i, scene, STRUCT_GUEST, guest) {
    mmu = get_child(guest, STRUCT_MMU);
    archpage_map = find_hypervisor_archpage_map(scene, guest);

    if (archpage_map == NULL)
      return 1;

    def = add_definition(SECTION_RW, "vm_cpu", NULL);
    def->identifier = malloc(32);
    sprintf(def->identifier, "vm_%s_cpus[%d]",
			guest->attrs[GUEST_ATTR_ID].value.string,
			guest->attrs[GUEST_ATTR_CPUMAP].value.list.num);
    def->initializer = malloc(256 * guest->attrs[GUEST_ATTR_CPUMAP].value.list.num);
    sprintf(def->initializer, "{\n");
    for (j = 0; j < guest->attrs[GUEST_ATTR_CPUMAP].value.list.num; j++) {
      sprintf(def->initializer + strlen(def->initializer),
				"  { &vm_%s, (vm_cpu_state *)0x%lx,\n"
				"    0x%lx, 0,\n"
				"    %d, vm_%s_cpu%d_memareas, &vm_%s_cpu%d_memtree,\n"
				"    %d, vm_%s_cpu%d_emulates, &vm_%s_cpu%d_emulatetree,\n"
				"    %s,\n"
				"    %s,\n"
				"    %s,\n"
				"    1, vm_%s_cpu%d_scheds },\n",
			guest->attrs[GUEST_ATTR_ID].value.string,
			get_dict_hex(archpage_map->attrs + MAP_ATTR_BASE,
				guest->attrs[GUEST_ATTR_CPUMAP].value.list.elem[j]),
			get_dict_hex(mmu->attrs + MMU_ATTR_BASE, j),
			vm_memarea_count(guest),
			guest->attrs[GUEST_ATTR_ID].value.string,
			j,
			guest->attrs[GUEST_ATTR_ID].value.string,
			j,
			vm_emulate_count(guest),
			guest->attrs[GUEST_ATTR_ID].value.string,
			j,
			guest->attrs[GUEST_ATTR_ID].value.string,
			j,
			vm_master_vdev_name_vtlb(guest, j),
			vm_master_vdev_name_irq(guest, j),
			vm_master_vdev_name_uart(guest, j),
			guest->attrs[GUEST_ATTR_ID].value.string,
			j);
    }
    sprintf(def->initializer + strlen(def->initializer), "}");

    for (j = 0; j < guest->attrs[GUEST_ATTR_CPUMAP].value.list.num; j++) {
      def = add_definition(SECTION_RW, "scheduler_entity", NULL);
      def->identifier = malloc(32);
      sprintf(def->identifier, "vm_%s_cpu%d_scheds[%d]",
			guest->attrs[GUEST_ATTR_ID].value.string,
			j,
			1);
      def->initializer = malloc(128);
      sprintf(def->initializer, "{\n"
				"  { vm_%s_cpus + %d, SCHEDULER_CLASS_FAIR_SHARE,"
				" 0, 0, 100, SCHEDULER_STATE_READY, NULL }"
				"\n}",
			guest->attrs[GUEST_ATTR_ID].value.string,
			j);
    }
  }

  return 0;
}

// --------------------------------------------------------------------------

int generate_items_vm(struct xmlnode *scene) {
  struct definition *def;
  struct xmlnode *guest;
  struct xmlnode *entry, *entry_map;
  uint64_t i;

  iterate_over_children(i, scene, STRUCT_GUEST, guest) {
    entry = get_child(guest, STRUCT_ENTRY);
    entry_map = find_mmu_map(get_child(guest, STRUCT_MMU),
			entry->attrs[ENTRY_ATTR_BP_XREF].value.string);

    def = add_definition(SECTION_RO, "vm", NULL);
    def->identifier = malloc(32);
    sprintf(def->identifier, "vm_%s",
			guest->attrs[GUEST_ATTR_ID].value.string);
    def->initializer = malloc(256);
    sprintf(def->initializer, "{\n"
				"  %d, vm_%s_cpus,\n"
				"  %d, vm_%s_capabilities,\n"
				"  %d, vm_%s_copyins,\n"
				"  0x%lx\n"
				"}",
			guest->attrs[GUEST_ATTR_CPUMAP].value.list.num,
			guest->attrs[GUEST_ATTR_ID].value.string,
			vm_capability_count(guest),
			guest->attrs[GUEST_ATTR_ID].value.string,
			vm_copyins_count(guest),
			guest->attrs[GUEST_ATTR_ID].value.string,
			get_dict_hex(entry_map->attrs + MAP_ATTR_BASE, 0) +
			entry->attrs[ENTRY_ATTR_BP_OFFSET].value.number);
  }

  return 0;
}

// --------------------------------------------------------------------------

int generate_vm_data(struct xmlnode *scene) {
  if (generate_items_vm(scene)) {
    return 1;
  }

  if (generate_items_vmX_cpus(scene)) {
    return 1;
  }

  if (generate_items_vmX_cpuY_memareas(scene)) {
    return 1;
  }

  if (generate_items_vmX_capabilities(scene)) {
    return 1;
  }

  if (generate_items_vmX_copyins(scene)) {
    return 1;
  }

  if (generate_items_vmX_cpuY_emulates(scene)) {
    return 1;
  }

  if (generate_items_vmX_vdevs(scene)) {
    return 1;
  }

  return 0;
}
