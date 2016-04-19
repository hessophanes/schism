#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <elf.h>
#include "xml.h"
#include "scenario.h"

int elf_to_phdrs(const char *file, struct phdr *phdr, int phdr_expect) {
  Elf32_Ehdr *ehdr32;
  Elf64_Ehdr *ehdr64;
  Elf32_Phdr *phdr32;
  Elf64_Phdr *phdr64;
  char buf_eh[128];
  char buf_ph[128];
  int f;
  int i;

  f = open(file, O_RDONLY);
  if (f < 0) {
    fprintf(stderr, "ERROR: cannot open ELF file.\n");
    return 1;
  }
  read(f, buf_eh, sizeof(buf_eh));

  if (buf_eh[EI_CLASS] == ELFCLASS32) {
    ehdr32 = (Elf32_Ehdr *)buf_eh;
    lseek(f, ehdr32->e_phoff, SEEK_SET);
    phdr32 = (Elf32_Phdr *)buf_ph;
    if (ehdr32->e_phnum != phdr_expect) {
      fprintf(stderr, "ERROR: expected %d ELF program headers, found %d\n",
			phdr_expect, ehdr32->e_phnum);
      return 1;
    }
    for (i = 0; i < ehdr32->e_phnum; i++) {
      read(f, buf_ph, ehdr32->e_phentsize);
      if (phdr32->p_type != PT_LOAD) continue;
      if (phdr32->p_vaddr & 0xfff) {
        phdr32->p_memsz += phdr32->p_vaddr & 0xfff;
        phdr32->p_filesz += phdr32->p_vaddr & 0xfff;
        phdr32->p_vaddr &= ~0xfff;
        phdr32->p_offset &= ~0xfff;
      }
      if (phdr32->p_memsz & 0xfff) {
        phdr32->p_memsz = (phdr32->p_memsz & ~0xfff) + 0x1000;
        phdr32->p_filesz = (phdr32->p_filesz & ~0xfff) + 0x1000;
      }

      phdr[i].base = phdr32->p_vaddr;
      phdr[i].size = phdr32->p_memsz;
      phdr[i].offset = phdr32->p_offset;
      phdr[i].filesize = phdr32->p_filesz;
      phdr[i].flags = phdr32->p_flags & (PF_R | PF_W | PF_X);
    }
  } else if (buf_eh[EI_CLASS] == ELFCLASS64) {
    ehdr64 = (Elf64_Ehdr *)buf_eh;
    lseek(f, ehdr64->e_phoff, SEEK_SET);
    phdr64 = (Elf64_Phdr *)buf_ph;
    if (ehdr64->e_phnum != phdr_expect) {
      fprintf(stderr, "ERROR: expected %d ELF program headers, found %d\n",
			phdr_expect, ehdr64->e_phnum);
      return 1;
    }
    for (i = 0; i < ehdr64->e_phnum; i++) {
      read(f, buf_ph, ehdr64->e_phentsize);
      if (phdr64->p_type != PT_LOAD) continue;
      if (phdr64->p_vaddr & 0xfff) {
        phdr64->p_memsz += phdr64->p_vaddr & 0xfff;
        phdr64->p_filesz += phdr64->p_vaddr & 0xfff;
        phdr64->p_vaddr &= ~0xfff;
        phdr64->p_offset &= ~0xfff;
      }
      if (phdr64->p_memsz & 0xfff) {
        phdr64->p_memsz = (phdr64->p_memsz & ~0xfff) + 0x1000;
        phdr64->p_filesz = (phdr64->p_filesz & ~0xfff) + 0x1000;
      }

      phdr[i].base = phdr64->p_vaddr;
      phdr[i].size = phdr64->p_memsz;
      phdr[i].offset = phdr64->p_offset;
      phdr[i].filesize = phdr64->p_filesz;
      phdr[i].flags = phdr64->p_flags & (PF_R | PF_W | PF_X);
    }
  } else {
    fprintf(stderr, "ERROR: unknown ELF class.\n");
    return 1;
  }

  close(f);

#ifdef LIBXML41_ELF_DEBUG
  for (i = 0; i < phdr_expect; i++) {
    fprintf(stderr, "ELF INFO: phdr[%d] := B %lx S %lx O %lx FS %lx\n",
	i, phdr[i].base, phdr[i].size, phdr[i].offset, phdr[i].filesize);
  }
#endif

  return 0;
}

uint64_t elf_entry_point(const char *file) {
  int f;
  char buf_eh[128];
  Elf32_Ehdr *ehdr32;
  Elf64_Ehdr *ehdr64;

  f = open(file, O_RDONLY);
  if (f < 0) {
    fprintf(stderr, "ERROR: cannot open ELF file.\n");
    return 1;
  }
  read(f, buf_eh, sizeof(buf_eh));

  if (buf_eh[EI_CLASS] == ELFCLASS32) {
    ehdr32 = (Elf32_Ehdr *)buf_eh;

    return ehdr32->e_entry;
  } else if (buf_eh[EI_CLASS] == ELFCLASS64) {
    ehdr64 = (Elf64_Ehdr *)buf_eh;

    return ehdr64->e_entry;
  }

  return 0;
}
