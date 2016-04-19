#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <elf.h>
#include "xml.h"
#include "scenario.h"

uint64_t offset_pa_to_va;

uint64_t hypervisor_base;
uint64_t config_base;
uint64_t pagetables_base;
uint64_t blob_base;

// ---------------------------------------------------------------------------

static int measure_hypervisor_elf(struct xmlnode *scene, const char *filename) {
  struct phdr phdrs[4];
  struct xmlnode *hypervisor;
  struct xmlnode *memreq;
  struct xmlnode *map;
  uint64_t hypervisor_entry;

  hypervisor_entry = elf_entry_point(filename);

  if (elf_to_phdrs(filename, phdrs, 4)) {
    return 1;
  }

  hypervisor = get_child(scene, STRUCT_HYPERVISOR);

  hypervisor_base = hypervisor->attrs[HYPERVISOR_ATTR_LOAD_BASE].value.number;

  offset_pa_to_va = phdrs[0].base - hypervisor_base;
  config_base = hypervisor_base + (phdrs[3].base - phdrs[0].base) + phdrs[3].size;

  memreq = find_memreq(scene, "core_rx");
  map = find_hypervisor_map(scene, "core_rx");
  if (!memreq || !map) {
    fprintf(stderr, "ERROR: <memreq>/<map> node missing for \"%s\".\n", "core_rx");
    return 1;
  }
  set_dict_wildcard_hex(memreq->attrs + MEMREQ_ATTR_BASE, hypervisor_base);
  memreq->attrs[MEMREQ_ATTR_SIZE].value.number = phdrs[0].size;
  memreq->attrs[MEMREQ_ATTR_SIZE].state = ATTRSTATE_MODIFIED;
  set_dict_wildcard_hex(map->attrs + MAP_ATTR_BASE, phdrs[0].base);

  memreq = find_memreq(scene, "core_r");
  map = find_hypervisor_map(scene, "core_r");
  if (!memreq || !map) {
    fprintf(stderr, "ERROR: <memreq>/<map> node missing for \"%s\".\n", "core_r");
    return 1;
  }
  set_dict_wildcard_hex(memreq->attrs + MEMREQ_ATTR_BASE, hypervisor_base + (phdrs[1].base - phdrs[0].base));
  memreq->attrs[MEMREQ_ATTR_SIZE].value.number = phdrs[1].size;
  memreq->attrs[MEMREQ_ATTR_SIZE].state = ATTRSTATE_MODIFIED;
  set_dict_wildcard_hex(map->attrs + MAP_ATTR_BASE, phdrs[1].base);

  memreq = find_memreq(scene, "core_rws");
  map = find_hypervisor_map(scene, "core_rws");
  if (!memreq || !map) {
    fprintf(stderr, "ERROR: <memreq>/<map> node missing for \"%s\".\n", "core_rws");
    return 1;
  }
  set_dict_wildcard_hex(memreq->attrs + MEMREQ_ATTR_BASE, hypervisor_base + (phdrs[2].base - phdrs[0].base));
  memreq->attrs[MEMREQ_ATTR_SIZE].value.number = phdrs[2].size;
  memreq->attrs[MEMREQ_ATTR_SIZE].state = ATTRSTATE_MODIFIED;
  set_dict_wildcard_hex(map->attrs + MAP_ATTR_BASE, phdrs[2].base);

  memreq = find_memreq(scene, "core_rwt");
  map = find_hypervisor_map(scene, "core_rwt");
  if (!memreq || !map) {
    fprintf(stderr, "ERROR: <memreq>/<map> node missing for \"%s\".\n", "core_rwt");
    return 1;
  }
  set_dict_wildcard_hex(memreq->attrs + MEMREQ_ATTR_BASE, hypervisor_base + (phdrs[3].base - phdrs[0].base));
  memreq->attrs[MEMREQ_ATTR_SIZE].value.number = phdrs[3].size;
  memreq->attrs[MEMREQ_ATTR_SIZE].state = ATTRSTATE_MODIFIED;
  map->attrs[MAP_ATTR_BASE].state = ATTRSTATE_DELETED;

  memreq = find_memreq(scene, "core_rw");
  map = find_hypervisor_map(scene, "core_rw");
  if (!memreq || !map) {
    fprintf(stderr, "ERROR: <memreq>/<map> node missing for \"%s\".\n", "core_rw");
    return 1;
  }
  memreq->attrs[MEMREQ_ATTR_BASE].state = ATTRSTATE_DELETED;
  memreq->attrs[MEMREQ_ATTR_SIZE].value.number = phdrs[3].size;
  memreq->attrs[MEMREQ_ATTR_SIZE].state = ATTRSTATE_MODIFIED;
  set_dict_wildcard_hex(map->attrs + MAP_ATTR_BASE, phdrs[3].base);

  /* convert virtual hypervisor entry to physical using offset calculated above */

  hypervisor->attrs[HYPERVISOR_ATTR_ENTRY].value.number = hypervisor_entry - offset_pa_to_va;
  hypervisor->attrs[HYPERVISOR_ATTR_ENTRY].state = ATTRSTATE_MODIFIED;

  return 0;
}

static int measure_config_data(struct xmlnode *scene, const char *filename) {
  struct phdr phdrs[3];
  struct xmlnode *memreq;
  struct xmlnode *map;

  if (elf_to_phdrs(filename, phdrs, 3)) {
    return 1;
  }

  pagetables_base = config_base + (phdrs[2].base - phdrs[0].base) + phdrs[2].size;

  memreq = find_memreq(scene, "config_r");
  map = find_hypervisor_map(scene, "config_r");
  if (!memreq || !map) {
    fprintf(stderr, "ERROR: <memreq>/<map> node missing for \"%s\".\n", "config_r");
    return 1;
  }
  set_dict_wildcard_hex(memreq->attrs + MEMREQ_ATTR_BASE, config_base);
  memreq->attrs[MEMREQ_ATTR_SIZE].value.number = phdrs[0].size;
  memreq->attrs[MEMREQ_ATTR_SIZE].state = ATTRSTATE_MODIFIED;
  set_dict_wildcard_hex(map->attrs + MAP_ATTR_BASE, offset_pa_to_va + config_base);

  memreq = find_memreq(scene, "config_rw");
  map = find_hypervisor_map(scene, "config_rw");
  if (!memreq || !map) {
    fprintf(stderr, "ERROR: <memreq>/<map> node missing for \"%s\".\n", "config_rw");
    return 1;
  }
  memreq->attrs[MEMREQ_ATTR_BASE].state = ATTRSTATE_DELETED;
  memreq->attrs[MEMREQ_ATTR_SIZE].value.number = phdrs[1].size;
  memreq->attrs[MEMREQ_ATTR_SIZE].state = ATTRSTATE_MODIFIED;
  set_dict_wildcard_hex(map->attrs + MAP_ATTR_BASE, offset_pa_to_va + config_base + (phdrs[1].base - phdrs[0].base));

  memreq = find_memreq(scene, "config_rws");
  map = find_hypervisor_map(scene, "config_rws");
  if (!memreq || !map) {
    fprintf(stderr, "ERROR: <memreq>/<map> node missing for \"%s\".\n", "config_rws");
    return 1;
  }
  set_dict_wildcard_hex(memreq->attrs + MEMREQ_ATTR_BASE, config_base + (phdrs[2].base - phdrs[0].base));
  memreq->attrs[MEMREQ_ATTR_SIZE].value.number = phdrs[2].size;
  memreq->attrs[MEMREQ_ATTR_SIZE].state = ATTRSTATE_MODIFIED;
  set_dict_wildcard_hex(map->attrs + MAP_ATTR_BASE, offset_pa_to_va + config_base + (phdrs[2].base - phdrs[0].base));

  memreq = find_memreq(scene, "config_rwt");
  map = find_hypervisor_map(scene, "config_rwt");
  if (!memreq || !map) {
    fprintf(stderr, "ERROR: <memreq>/<map> node missing for \"%s\".\n", "config_rwt");
    return 1;
  }
  set_dict_wildcard_hex(memreq->attrs + MEMREQ_ATTR_BASE, config_base + (phdrs[1].base - phdrs[0].base));
  memreq->attrs[MEMREQ_ATTR_SIZE].value.number = phdrs[1].size;
  memreq->attrs[MEMREQ_ATTR_SIZE].state = ATTRSTATE_MODIFIED;
  map->attrs[MAP_ATTR_BASE].state = ATTRSTATE_DELETED;

  return 0;
}

static uint64_t get_default_estimate(struct xmlnode *scene, struct xmlattr *format_attr) {
  struct xmlnode *paging_format;

  paging_format = resolve_idref(scene, format_attr);

  if (!paging_format || (paging_format->node_type != STRUCT_PAGING_FORMAT)) {
    fprintf(stderr, "ERROR: paging format not found, cannot estimate size.\n");
    return 0;
  }

  if (paging_format->attrs[PAGING_FORMAT_ATTR_SIZE_ESTIMATE].state != ATTRSTATE_CLEAN) {
    fprintf(stderr, "ERROR: no default estimate value, using 0.\n");
    return 0;
  }

  return paging_format->attrs[PAGING_FORMAT_ATTR_SIZE_ESTIMATE].value.number;
}

static int estimate_pagetables(struct xmlnode *scene) {
  unsigned int i;
  struct xmlnode *guest;
  struct xmlnode *hypervisor, *mmu;
  struct xmlnode *memreq;
  uint64_t size_estimate = 0;
  uint64_t estimate_item = 0;

  hypervisor = get_child(scene, STRUCT_HYPERVISOR);
  mmu = get_child(hypervisor, STRUCT_MMU);

  if (mmu->attrs[MMU_ATTR_SIZE_ESTIMATE].state == ATTRSTATE_CLEAN) {
    estimate_item = mmu->attrs[MMU_ATTR_SIZE_ESTIMATE].value.number;
  } else {
    estimate_item = get_default_estimate(scene, mmu->attrs + MMU_ATTR_FORMAT);
  }
  size_estimate += (hypervisor->attrs[HYPERVISOR_ATTR_NCPUS].value.number + 1) * estimate_item;

  iterate_over_children(i, scene, STRUCT_GUEST, guest) {
    mmu = get_child(guest, STRUCT_MMU);
    if (strcmp(mmu->attrs[MMU_ATTR_FORMAT].value.string, "none") == 0) continue;
    if (mmu->attrs[MMU_ATTR_SIZE_ESTIMATE].state == ATTRSTATE_CLEAN) {
      estimate_item = mmu->attrs[MMU_ATTR_SIZE_ESTIMATE].value.number;
    } else {
      estimate_item = get_default_estimate(scene, mmu->attrs + MMU_ATTR_FORMAT);
    }
    size_estimate += guest->attrs[GUEST_ATTR_NCPUS].value.number * estimate_item;
  }

  memreq = find_memreq(scene, "pagetables");
  if (!memreq) {
    fprintf(stderr, "ERROR: <memreq> node missing for \"%s\".\n", "pagetables");
    return 1;
  }
  set_dict_wildcard_hex(memreq->attrs + MEMREQ_ATTR_BASE, pagetables_base);
  memreq->attrs[MEMREQ_ATTR_SIZE].value.number = size_estimate;
  memreq->attrs[MEMREQ_ATTR_SIZE].state = ATTRSTATE_MODIFIED;

  fprintf(stderr, "INFO: estimating total size of pagetables: 0x%lx\n", size_estimate);

  blob_base = pagetables_base + size_estimate;

  return 0;
}

static int measure_file_blob(struct xmlnode *scene) {
  uint64_t i;
  struct stat ffs;
  unsigned int fsz;
  struct xmlnode *files, *file;
  struct xmlnode *memreq;

  files = get_child(scene, STRUCT_FILES);

  fsz = 0;
  iterate_over_children(i, files, STRUCT_FILE, file) {
    if (stat(file->attrs[FILE_ATTR_HREF].value.string, &ffs) != 0) {
      fprintf(stderr, "ERROR: cannot access file \"%s\".\n", file->attrs[FILE_ATTR_HREF].value.string);
      return 1;
    }
    file->attrs[FILE_ATTR_OFFSET].value.number = fsz;
    file->attrs[FILE_ATTR_OFFSET].state = ATTRSTATE_MODIFIED;
    file->attrs[FILE_ATTR_SIZE].value.number = ffs.st_size;
    file->attrs[FILE_ATTR_SIZE].state = ATTRSTATE_MODIFIED;

    fsz += ffs.st_size;
  }
  if (fsz & 0xfff)
    fsz = (fsz & ~0xfff) + 0x1000;

  memreq = find_memreq(scene, "blob");
  if (!memreq) {
    fprintf(stderr, "ERROR: <memreq> node missing for \"%s\".\n", "blob");
    return 1;
  }
  set_dict_wildcard_hex(memreq->attrs + MEMREQ_ATTR_BASE, blob_base);
  memreq->attrs[MEMREQ_ATTR_SIZE].value.number = fsz;
  memreq->attrs[MEMREQ_ATTR_SIZE].state = ATTRSTATE_MODIFIED;

  fprintf(stderr, "INFO: file blob has size 0x%x.\n", fsz);

  return 0;
}

// ---------------------------------------------------------------------------

int main(int an, char **ac) {
  struct xmlnode *scene; 
  char fn[128];

  if (an != 2) {
    fprintf(stderr, "ERROR: need build directory argument.\n");
    return 1;
  }

  sprintf(fn, "%s/scenario_reparented.xml", ac[1]);
  scene = parse_from_file(fn, STRUCT_SCENARIO);
  if (!scene) {
    fprintf(stderr, "ERROR: could not load expanded scenario: %s.\n", xml_errormsg);
    return 1;
  }

  // ---------------------------------------------------------------------------

  // measure hypervisor ELF sizes
  sprintf(fn, "%s/%s.elf", ac[1], scene->attrs[SCENARIO_ATTR_CBI].value.string);
  if (measure_hypervisor_elf(scene, fn)) return 1;

  // measure config data
  sprintf(fn, "%s/scenario_config.xo", ac[1]);
  if (measure_config_data(scene, fn)) return 1;

  // estimate pagetable size
  if (estimate_pagetables(scene)) return 1;

  // measure file blob size
  if (measure_file_blob(scene)) return 1;

  // ---------------------------------------------------------------------------

  sprintf(fn, "%s/scenario_measured.xml", ac[1]);
  write_to_file(fn, scene);

  return 0;
}
