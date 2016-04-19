// layouter

struct range {
  unsigned long base, size;
  struct range *prev, *next;
};

uint32_t *get_preferred_alignments(struct xmlnode *arch);
uint32_t *get_preferred_alignment(struct xmlnode *paging_format);

struct range *init_range(unsigned long base, unsigned long size);
struct range *claim_range(struct range *range, unsigned long base, unsigned long size);
struct range *find_range(struct range *range, unsigned long size, int align, unsigned long *base_ret);
struct range *find_padded_window_range(struct range *head, unsigned long size, int align, unsigned long *base_ret, int pad_align, unsigned long *window);

struct range *clone_range(struct range *head);
void error_dump_ranges(struct range *head);

struct xmlnode *find_memreq(struct xmlnode *scene, const char *map_xref);
struct xmlnode *find_device(struct xmlnode *scene, const char *map_xref);
struct xmlnode *find_mmu_map(struct xmlnode *mmu, const char *id);

static inline struct xmlnode *find_hypervisor_map(struct xmlnode *scene, const char *id) {
  return find_mmu_map(get_child(get_child(scene, STRUCT_HYPERVISOR), STRUCT_MMU), id);
}

#define		ALIGN_MASK(a)		((1UL << (a)) - 1)

// ELF

struct phdr {
  unsigned long base, size;		// in virtual memory
  unsigned long offset, filesize;	// into file
  int flags;
};

int elf_to_phdrs(const char *file, struct phdr *phdr, int phdr_expect);
uint64_t elf_entry_point(const char *file);
