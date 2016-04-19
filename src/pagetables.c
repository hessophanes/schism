#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <elf.h>
#include "xml.h"
#include "scenario.h"

// ---------------------------------------------------------------------------

struct pagetable_dir;
struct pagetable_entry;
struct paging_level;

struct pagetable_dir {
  int level;
  int is_placed;
  struct pagetable_entry *entries;
  uint64_t phys_addr;
  uint64_t head_padding;
};

#define		ENTRY_LEAF		1
#define		ENTRY_DIR		2

struct pagetable_entry {
  int entry_type;
  struct pagetable_dir *dir;
  uint64_t page_addr;
};

struct pagetable_dir *pagetable_tree;
struct pagetable_dir **unplaced_dir_list;
uint32_t unplaced_dirs;

enum permission_index {
	PERM_R = 0, PERM_W = 2, PERM_X = 4,
	PERM_G = 6, PERM_D = 8, PERM_U = 10, PERM_S = 12
};

struct paging_level {
  int shift, width, bits_per_entry, alignment_shift;

  int can_dir, can_leaf;
  uint64_t dir_entry, leaf_entry;

  uint64_t permission_flags[7 * 2];
};

uint32_t paging_num_levels;
struct paging_level *paging_levels;
uint64_t pagetable_base;

// ---------------------------------------------------------------------------

static struct pagetable_dir *alloc_directory(int level) {
  struct pagetable_dir *d;

  d = malloc(sizeof(struct pagetable_dir));
  d->level = level;
  d->is_placed = 0;
  d->phys_addr = 0;
  d->entries = malloc((1 << paging_levels[level].width) * sizeof(struct pagetable_entry));
  memset(d->entries, 0, (1 << paging_levels[level].width) * sizeof(struct pagetable_entry));

  unplaced_dir_list = realloc(unplaced_dir_list, (unplaced_dirs + 1) * sizeof(void *));
  unplaced_dir_list[unplaced_dirs] = d;
  unplaced_dirs++;

  return d;
}

static uint64_t *assemble_flag_bits(const char *flags) {
  uint64_t *flag_bits;
  uint32_t i;

  flag_bits = malloc(paging_num_levels * sizeof(uint64_t));

  for (i = 0; i < paging_num_levels; i++) {
    flag_bits[i] = paging_levels[i].leaf_entry;
    flag_bits[i] |= paging_levels[i].permission_flags[PERM_R + (strchr(flags, 'r') ? 0 : 1)];
    flag_bits[i] |= paging_levels[i].permission_flags[PERM_W + (strchr(flags, 'w') ? 0 : 1)];
    flag_bits[i] |= paging_levels[i].permission_flags[PERM_X + (strchr(flags, 'x') ? 0 : 1)];
    flag_bits[i] |= paging_levels[i].permission_flags[PERM_G + (strchr(flags, 'g') ? 0 : 1)];
    flag_bits[i] |= paging_levels[i].permission_flags[PERM_D + (strchr(flags, 'd') ? 0 : 1)];
    flag_bits[i] |= paging_levels[i].permission_flags[PERM_U + (strchr(flags, 'u') ? 0 : 1)];
    flag_bits[i] |= paging_levels[i].permission_flags[PERM_S + (strchr(flags, 's') ? 0 : 1)];
  }

  return flag_bits;
}

static int add_mapping(uint64_t va, uint64_t pa, uint64_t size, const char *flags) {
  struct pagetable_dir *d = pagetable_tree;
  uint64_t *flag_bits;

  flag_bits = assemble_flag_bits(flags);

  while (size) {
    struct pagetable_entry *e;
    int va_index;

    va_index = (va >> paging_levels[d->level].shift) & ALIGN_MASK(paging_levels[d->level].width);
    e = d->entries + va_index;

    if (	(!paging_levels[d->level].can_leaf) ||
		(va & ALIGN_MASK(paging_levels[d->level].shift)) ||
    		(pa & ALIGN_MASK(paging_levels[d->level].shift)) ||
    		(size < ALIGN_MASK(paging_levels[d->level].shift))	) {
      if (e->entry_type == ENTRY_DIR) {
//printf("DSC(%d) %lx+%lx\n", d->level, va, size);
        d = e->dir;
      } else if (e->entry_type == 0) {
//printf("DSC+(%d) %lx+%lx\n", d->level, va, size);
        e->entry_type = ENTRY_DIR;
        d = e->dir = alloc_directory(d->level + 1);
      } else {
        fprintf(stderr, "Clash, entry present where dir should be at 0x%lx\n", va);
        return 1;
      }
    } else {
      if (e->entry_type) {
        fprintf(stderr, "Clash, entry present at 0x%lx\n", va);
        return 1;
      }
      e->entry_type = ENTRY_LEAF;
      e->dir = NULL;
      e->page_addr = pa | flag_bits[d->level];

      va += (1UL << paging_levels[d->level].shift);
      pa += (1UL << paging_levels[d->level].shift);
      size -= (1UL << paging_levels[d->level].shift);
      d = pagetable_tree;
    }
  }

  free(flag_bits);

  return 0;
}

/**
 * cpu_no: the dictionary index the base address of this pagetable will be recorded as
 *         (in the "base" attribute of the <mmu> node)
 */
static int place_directories(struct xmlnode *mmu, int cpu_no, int fd) {
  uint32_t i, j;
  int align;
  uint64_t allocation_base = pagetable_base;
  struct pagetable_dir **placed_dir_list = malloc(unplaced_dirs * sizeof(void *));
  uint32_t placed_dirs = 0;

  while (1) {
    align = 0;
    for (i = 0; i < unplaced_dirs; i++) {
      if (unplaced_dir_list[i]->is_placed) continue;
      if (paging_levels[unplaced_dir_list[i]->level].alignment_shift < align) continue;
      align = paging_levels[unplaced_dir_list[i]->level].alignment_shift;
    }
    if (align == 0) break;

    for (i = 0; i < unplaced_dirs; i++) {
      struct pagetable_dir *d = unplaced_dir_list[i];

      if (d->is_placed) continue;
      if (paging_levels[d->level].alignment_shift != align) continue;

      d->phys_addr = allocation_base;
      if (d->phys_addr & ALIGN_MASK(align)) {
        d->phys_addr = (d->phys_addr & ~ALIGN_MASK(align)) + (1UL << align);
      }
      d->head_padding = d->phys_addr - allocation_base;
      allocation_base = d->phys_addr + \
			(paging_levels[d->level].bits_per_entry \
			<< paging_levels[d->level].width);
      d->is_placed = 1;
      placed_dir_list[placed_dirs] = d;
      placed_dirs++;

      if (d->level == 0) {
        add_dict_hex(mmu->attrs + MMU_ATTR_BASE, cpu_no, d->phys_addr);
      } else {
        d->phys_addr |= paging_levels[d->level - 1].dir_entry;
      }
    }
  }

  for (i = 0; i < placed_dirs; i++) {
    struct pagetable_dir *d = placed_dir_list[i];

    (void)lseek(fd, d->head_padding, SEEK_CUR);
    for (j = 0; j < (1U << paging_levels[d->level].width); j++) {
      uint64_t val;

      switch (d->entries[j].entry_type) {
      case 0:		val = 0; break;
      case ENTRY_LEAF:	val = d->entries[j].page_addr; break;
      case ENTRY_DIR:	val = d->entries[j].dir->phys_addr; break;
      }

      switch (paging_levels[d->level].bits_per_entry) {
      case 4:		write(fd, (void *)&val, 4); break;
      case 8:		write(fd, (void *)&val, 8); break;
      }
    }
  }

  pagetable_base = allocation_base;

  return 0;
}

// ---------------------------------------------------------------------------

static int parse_paging_format(struct xmlnode *paging_format) {
  uint64_t i, j;
  struct xmlnode *level, *flag;

  paging_num_levels = count_children(paging_format, STRUCT_LEVEL);
  paging_levels = malloc(paging_num_levels * sizeof(struct paging_level));
  memset(paging_levels, 0, paging_num_levels * sizeof(struct paging_level));

  iterate_over_children(i, paging_format, STRUCT_LEVEL, level) {
    paging_levels[i].shift = level->attrs[LEVEL_ATTR_SHIFT].value.number;
    paging_levels[i].width = level->attrs[LEVEL_ATTR_WIDTH].value.number;
    paging_levels[i].bits_per_entry = level->attrs[LEVEL_ATTR_BPE].value.number;
    paging_levels[i].alignment_shift = level->attrs[LEVEL_ATTR_ALIGN].value.number;

    if (attr_exists(level->attrs + LEVEL_ATTR_DIR_BASE)) {
      paging_levels[i].can_dir = 1;
      paging_levels[i].dir_entry = level->attrs[LEVEL_ATTR_DIR_BASE].value.number;
    }
    if (attr_exists(level->attrs + LEVEL_ATTR_LEAF_BASE)) {
      paging_levels[i].can_leaf = 1;
      paging_levels[i].leaf_entry = level->attrs[LEVEL_ATTR_LEAF_BASE].value.number;
    }

    iterate_over_children(j, level, STRUCT_FLAG, flag) {
      char flag_str = flag->attrs[FLAG_ATTR_NAME].value.character;
      enum permission_index index;
      switch (flag_str) {
      case 'r': index = PERM_R; break;
      case 'w': index = PERM_W; break;
      case 'x': index = PERM_X; break;
      case 'g': index = PERM_G; break;
      case 'd': index = PERM_D; break;
      case 'u': index = PERM_U; break;
      case 's': index = PERM_S; break;
      default:
        fprintf(stderr, "ERROR: unknown paging flag.\n");
        return 1;
      }

      paging_levels[i].permission_flags[index] = flag->attrs[FLAG_ATTR_VALUE_SET].value.number;
      paging_levels[i].permission_flags[index+1] = flag->attrs[FLAG_ATTR_VALUE_CLEAR].value.number;
    }

    if ((i > 0) && (paging_levels[i].shift > paging_levels[i-1].shift)) {
      fprintf(stderr, "ERROR: paging format levels not in descending order.\n");
      return 1;
    }
  }

  return 0;
}

/**
 * Build up the pagetable for a given <mmu> node.
 * Case 1: <hypervisor><mmu>
 *   host_cpu: -1 (INIT pagetable) or [0..ncpu-1] (core_space)
 *   mmu_cpu: == host_cpu
 * Case 2: <guest><mmu>
 *   mmu_cpu: [0..guest_ncpu-1] (virt_space)
 *   host_cpu: physical CPU this virt_space is used on
 */
static int create_pagetable(struct xmlnode *scene, struct xmlnode *mmu, int mmu_cpu, int host_cpu, int fd) {
  uint64_t i;
  struct xmlnode *paging_format;
  struct xmlnode *map;

  if (strcmp(mmu->attrs[MMU_ATTR_FORMAT].value.string, "none") == 0) {
    return 0;
  }

  paging_format = resolve_idref(scene, mmu->attrs + MMU_ATTR_FORMAT);

  if (paging_format == NULL) {
    fprintf(stderr, "ERROR: unknown paging format.\n");
    return 1;
  }
  if (parse_paging_format(paging_format)) {
    return 1;
  }

  unplaced_dirs = 0;
  unplaced_dir_list = NULL;

  pagetable_tree = alloc_directory(0);

  iterate_over_children(i, mmu, STRUCT_MAP, map) {
    struct xmlnode *memdev;
    uint64_t va, pa, size;

    if (attr_exists(map->attrs + MAP_ATTR_CPUMAP) &&
	!has_list(map->attrs + MAP_ATTR_CPUMAP, (uint32_t)mmu_cpu))
      continue;

    if ((host_cpu < 0) && !attr_exists(map->attrs + MAP_ATTR_IS_INIT))
      continue;

    memdev = resolve_idref(scene, map->attrs + MAP_ATTR_XREF);
    if (memdev && (memdev->node_type == STRUCT_MEMREQ)) {
      size = memdev->attrs[MEMREQ_ATTR_SIZE].value.number;
      pa = get_dict_hex(memdev->attrs + MEMREQ_ATTR_BASE, (uint32_t)host_cpu);
    } else if (memdev && (memdev->node_type == STRUCT_DEVICE)) {
      size = memdev->attrs[DEVICE_ATTR_SIZE].value.number;
      pa = memdev->attrs[DEVICE_ATTR_BASE].value.number;
    } else {
      fprintf(stderr, "ERROR: no <memreq> or <device> for <map> \"%s\".\n", map->attrs[MAP_ATTR_XREF].value.string);
      return 1;
    }

    if (attr_exists(map->attrs + MAP_ATTR_OFFSET)) {
      pa += map->attrs[MAP_ATTR_OFFSET].value.number;
      // DO NOT INCREMENT va
      size -= map->attrs[MAP_ATTR_OFFSET].value.number;
    }
    if (attr_exists(map->attrs + MAP_ATTR_SUBSIZE)) {
      size = map->attrs[MAP_ATTR_SUBSIZE].value.number;
    }

    if ((memdev->node_type == STRUCT_MEMREQ) &&
	attr_exists(memdev->attrs + MEMREQ_ATTR_CPUMAP) &&
	!has_list(memdev->attrs + MEMREQ_ATTR_CPUMAP, (uint32_t)host_cpu))
      continue;

    if (host_cpu < 0) {
      if (add_mapping(pa, pa, size, map->attrs[MAP_ATTR_FLAGS].value.string)) {
        fprintf(stderr, "ERROR: map insertion (INIT:ID) failed for \"%s\".\n", map->attrs[MAP_ATTR_XREF].value.string);
        return 1;
      }
    }

    va = get_dict_hex(map->attrs + MAP_ATTR_BASE, (uint32_t)mmu_cpu);
    if (add_mapping(va, pa, size, map->attrs[MAP_ATTR_FLAGS].value.string)) {
      fprintf(stderr, "ERROR: map insertion failed for \"%s\".\n", map->attrs[MAP_ATTR_XREF].value.string);
      return 1;
    }
  }

  if (place_directories(mmu, mmu_cpu, fd)) return 1;

  return 0;
}

// ---------------------------------------------------------------------------

int main(int an, char **ac) {
  struct xmlnode *scene, *hypervisor, *mmu, *guest;
  struct xmlnode *memreq;
  char fn[128];
  uint64_t i;
  uint32_t j;
  int pagetable_fd;
  uint64_t pagetable_estimated_size, pagetable_actual_size;
  char *parse_error = NULL;

  if (an != 2) {
    fprintf(stderr, "ERROR: need build directory argument.\n");
    return 1;
  }

  sprintf(fn, "%s/scenario_v_laidout.xml", ac[1]);
  scene = parse_from_file(fn, STRUCT_SCENARIO);
  if (!scene) {
    fprintf(stderr, "ERROR: could not load expanded scenario: %s.\n", xml_errormsg);
    return 1;
  }

  // ---------------------------------------------------------------------------

  memreq = find_memreq(scene, "pagetables");
  if (memreq == NULL) {
    fprintf(stderr, "ERROR: no pagetables <memreq>.\n");
    return 1;
  }
  pagetable_base = get_dict_hex(memreq->attrs + MEMREQ_ATTR_BASE, 0);
  pagetable_estimated_size = memreq->attrs[MEMREQ_ATTR_SIZE].value.number;
  if (parse_error) {
    fprintf(stderr, "ERROR: cannot parse pagetable dimensions.\n");
    return 1;
  }

  // ---------------------------------------------------------------------------

  sprintf(fn, "%s/pagetables", ac[1]);
  pagetable_fd = open(fn, O_WRONLY | O_CREAT | O_TRUNC, 0644);

  hypervisor = get_child(scene, STRUCT_HYPERVISOR);
  mmu = get_child(hypervisor, STRUCT_MMU);
  if (create_pagetable(scene, mmu, -1, -1, pagetable_fd)) return 1;

  for (j = 0; j < (uint32_t)hypervisor->attrs[HYPERVISOR_ATTR_NCPUS].value.number; j++) {
    if (create_pagetable(scene, mmu, j, j, pagetable_fd)) return 1;
  }

  iterate_over_children(i, scene, STRUCT_GUEST, guest) {
    mmu = get_child(guest, STRUCT_MMU);
    for (j = 0; j < (uint32_t)guest->attrs[GUEST_ATTR_NCPUS].value.number; j++) {
      uint32_t host_cpu = guest->attrs[GUEST_ATTR_CPUMAP].value.list.elem[j];
      sprintf(fn, "ptguest_%s_%d", guest->attrs[GUEST_ATTR_ID].value.string, j);
      if (create_pagetable(scene, mmu, j, host_cpu, pagetable_fd)) return 1;
    }
  }

  pagetable_actual_size = lseek(pagetable_fd, 0, SEEK_CUR);

  if (pagetable_actual_size > pagetable_estimated_size) {
    fprintf(stderr, "ERROR: total size of pagetables exceeds estimate (0x%lx > 0x%lx).\n",
		pagetable_actual_size, pagetable_estimated_size);
    return 1;
  } else {
    fprintf(stderr, "INFO: total size of pagetables within estimate (0x%lx <= 0x%lx).\n",
		pagetable_actual_size, pagetable_estimated_size);
  }

  lseek(pagetable_fd, pagetable_estimated_size, SEEK_SET);
  close(pagetable_fd);

  // ---------------------------------------------------------------------------

  sprintf(fn, "%s/scenario_pagetables.xml", ac[1]);
  write_to_file(fn, scene);

  return 0;
}
