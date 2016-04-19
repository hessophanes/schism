#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <sys/wait.h>
#include "xml.h"
#include "scenario.h"

extern char * const *environ;

// ---------------------------------------------------------------------------

static int write_core(int image_fd, struct xmlnode *scene, const char *input_file) {
  int input_fd;
  struct phdr phdrs[4];
  struct xmlnode *memreq;
  uint64_t memreq_size;

  if (elf_to_phdrs(input_file, phdrs, 4)) {
    fprintf(stderr, "ERROR: could not read program headers from hypervisor ELF.\n");
    return 1;
  }

  input_fd = open(input_file, O_RDONLY);
  if (input_fd < 0) {
    fprintf(stderr, "ERROR: could not open hypervisor ELF.\n");
    return 1;
  }

  if ((phdrs[0].size != phdrs[0].filesize) ||
	(phdrs[1].size != phdrs[1].filesize) ||
	(phdrs[2].size != phdrs[2].filesize)) {
    fprintf(stderr, "ERROR: ELF file size differs from memory size for PHDR[0-2].\n");
    return 1;
  }

  memreq = find_memreq(scene, "core_rx");
  memreq_size = memreq->attrs[MEMREQ_ATTR_SIZE].value.number;
  lseek(input_fd, phdrs[0].offset, SEEK_SET);
  sendfile(image_fd, input_fd, NULL, memreq_size);

  memreq = find_memreq(scene, "core_r");
  memreq_size = memreq->attrs[MEMREQ_ATTR_SIZE].value.number;
  lseek(input_fd, phdrs[1].offset, SEEK_SET);
  sendfile(image_fd, input_fd, NULL, memreq_size);

  memreq = find_memreq(scene, "core_rws");
  memreq_size = memreq->attrs[MEMREQ_ATTR_SIZE].value.number;
  lseek(input_fd, phdrs[2].offset, SEEK_SET);
  sendfile(image_fd, input_fd, NULL, memreq_size);

  memreq = find_memreq(scene, "core_rw");
  memreq_size = memreq->attrs[MEMREQ_ATTR_SIZE].value.number;
  lseek(input_fd, phdrs[3].offset, SEEK_SET);
  sendfile(image_fd, input_fd, NULL, memreq_size);

  close(input_fd);

  return 0;
}

// ---------------------------------------------------------------------------

static int write_config(int image_fd, struct xmlnode *scene, const char *input_file) {
  int input_fd;
  struct phdr phdrs[3];
  struct xmlnode *memreq;
  uint64_t memreq_size;

  if (elf_to_phdrs(input_file, phdrs, 3)) {
    fprintf(stderr, "ERROR: could not read program headers from config data ELF.\n");
    return 1;
  }

  input_fd = open(input_file, O_RDONLY);
  if (input_fd < 0) {
    fprintf(stderr, "ERROR: could not open hypervisor ELF.\n");
    return 1;
  }

  memreq = find_memreq(scene, "config_r");
  memreq_size = memreq->attrs[MEMREQ_ATTR_SIZE].value.number;
  lseek(input_fd, phdrs[0].offset, SEEK_SET);
  sendfile(image_fd, input_fd, NULL, memreq_size);

  memreq = find_memreq(scene, "config_rw");
  memreq_size = memreq->attrs[MEMREQ_ATTR_SIZE].value.number;
  lseek(input_fd, phdrs[1].offset, SEEK_SET);
  sendfile(image_fd, input_fd, NULL, memreq_size);

  memreq = find_memreq(scene, "config_rws");
  memreq_size = memreq->attrs[MEMREQ_ATTR_SIZE].value.number;
  lseek(input_fd, phdrs[2].offset, SEEK_SET);
  sendfile(image_fd, input_fd, NULL, memreq_size);

  close(input_fd);

  return 0;
}

// ---------------------------------------------------------------------------

static int write_pagetables(int image_fd, struct xmlnode *scene, const char *input_file) {
  int input_fd;
  struct stat input_fs;
  struct xmlnode *memreq;
  uint64_t memreq_size;

  input_fd = open(input_file, O_RDONLY);
  if (input_fd < 0) {
    fprintf(stderr, "ERROR: could not open pagetable file.\n");
    return 1;
  }
  (void)fstat(input_fd, &input_fs);

  memreq = find_memreq(scene, "pagetables");
  memreq_size = memreq->attrs[MEMREQ_ATTR_SIZE].value.number;
  sendfile(image_fd, input_fd, NULL, input_fs.st_size);
  lseek(image_fd, memreq_size - input_fs.st_size, SEEK_CUR);

  close(input_fd);

  return 0;
}

// ---------------------------------------------------------------------------

static int write_blob(int image_fd, struct xmlnode *scene) {
  struct xmlnode *files, *file;
  uint64_t i;
  int input_fd;
  uint64_t input_len;

  files = get_child(scene, STRUCT_FILES);

  iterate_over_children(i, files, STRUCT_FILE, file) {
    input_fd = open(file->attrs[FILE_ATTR_HREF].value.string, O_RDONLY);
    if (input_fd < 0) {
      fprintf(stderr, "ERROR: could not open data file \"%s\".\n",
		file->attrs[FILE_ATTR_HREF].value.string);
      return 1;
    }
    input_len = file->attrs[FILE_ATTR_SIZE].value.number;
    sendfile(image_fd, input_fd, NULL, input_len);
    close(input_fd);
  }

  return 0;
}

// ---------------------------------------------------------------------------

static int build_uimage(struct xmlnode *scene, const char *builddir) {
  char loadaddr[20];
  char entryaddr[20];
  char imagename[20];
  char arch[10];
  char srcfile[128];
  char dstfile[128];
  char *argv[17] = { "mkimage", "-a", loadaddr, "-e", entryaddr, "-n", imagename,
			"-A", arch, "-C", "none", "-T", "kernel",
			"-d", srcfile, dstfile, NULL };
  struct xmlnode *hypervisor = get_child(scene, STRUCT_HYPERVISOR);
  int F, W;

  sprintf(loadaddr, "0x%lx", hypervisor->attrs[HYPERVISOR_ATTR_LOAD_BASE].value.number);
  sprintf(entryaddr, "0x%lx", hypervisor->attrs[HYPERVISOR_ATTR_ENTRY].value.number);
  sprintf(imagename, "%s", scene->attrs[SCENARIO_ATTR_CBI].value.string);
  sprintf(arch, "%s", get_child(get_child(scene, STRUCT_PLATFORM), STRUCT_ARCH)->attrs[ARCH_ATTR_ID].value.string);
  sprintf(srcfile, "%s/image", builddir);
  sprintf(dstfile, "%s/uimage", builddir);

  F = fork();
  if (F > 0) {
    (void)wait(&W);
  } else if (F == 0) {
    close(1);
    execve("/usr/bin/mkimage", argv, environ);
    exit(1);
  } else {
    fprintf(stderr, "ERROR: fork() failed.\n");
    return 1;
  }

  return W;
}

static int build_efi_pe(struct xmlnode *scene, const char *builddir) {
  (void)scene; (void)builddir;

  return 1;
}

// ---------------------------------------------------------------------------

int main(int an, char **ac) {
  struct xmlnode *scene;
  char fn[128];
  int fd;

  if (an != 2) {
    fprintf(stderr, "ERROR: need build directory argument.\n");
    return 1;
  }

  sprintf(fn, "%s/scenario_pagetables.xml", ac[1]);
  scene = parse_from_file(fn, STRUCT_SCENARIO);
  if (!scene) {
    fprintf(stderr, "ERROR: could not load scenario: %s.\n", xml_errormsg);
    return 1;
  }

  // ---------------------------------------------------------------------------

  sprintf(fn, "%s/image", ac[1]);
  fd = open(fn, O_CREAT | O_TRUNC | O_WRONLY, 0644);
  if (fd < 0) {
    fprintf(stderr, "ERROR: could not create image file.\n");
    return 1;
  }

  sprintf(fn, "%s/%s.elf", ac[1], scene->attrs[SCENARIO_ATTR_CBI].value.string);
  if (write_core(fd, scene, fn)) {
    fprintf(stderr, "ERROR: could not write core sections.\n");
    return 1;
  }

  sprintf(fn, "%s/scenario_config_real.xo", ac[1]);
  if (write_config(fd, scene, fn)) {
    fprintf(stderr, "ERROR: could not write config data.\n");
    return 1;
  }

  sprintf(fn, "%s/pagetables", ac[1]);
  if (write_pagetables(fd, scene, fn)) {
    fprintf(stderr, "ERROR: could not write pagetables.\n");
    return 1;
  }

  if (write_blob(fd, scene)) {
    fprintf(stderr, "ERROR: could not write payload blob.\n");
    return 1;
  }

  close(fd);

  if (strcmp(scene->attrs[SCENARIO_ATTR_IMAGE].value.string, "uboot") == 0) {
    return build_uimage(scene, ac[1]);
  } else if (strcmp(scene->attrs[SCENARIO_ATTR_IMAGE].value.string, "efi-pe") == 0) {
    return build_efi_pe(scene, ac[1]);
  } else if (strcmp(scene->attrs[SCENARIO_ATTR_IMAGE].value.string, "raw") == 0) {
    return 0;
  } else {
    fprintf(stderr, "ERROR: unknown image type.\n");
    return 1;
  }
}
