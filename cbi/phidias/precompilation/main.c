#include <stdio.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "xml.h"
#include "scenario.h"

static char *makeconf_buffer;
uint32_t makeconf_size;

#define	makeconf_printf(str, ...)	makeconf_size += sprintf(makeconf_buffer + makeconf_size, str, __VA_ARGS__)

// ---------------------------------------------------------------------------

struct arch_to_cross {
  const char *arch;
  const char *cross_prefix;
};

static const struct arch_to_cross crosslist[] = {
  { "arm64", "aarch64-unknown-elf-" },
  { "arm64", "aarch64-unknown-linux-gnu-" },
  { NULL, NULL }
};

static char default_path[] = "/bin:/usr/bin";

static int path_try(const char *prefix) {
  char *path_var = getenv("PATH");
  char *path_cursor, *path_next;
  char fn[256];

  if (path_var == NULL) {
    path_var = default_path;
  }

  for (path_cursor = path_var; *path_cursor; path_cursor = path_next) {
    char *path_sep = strchr(path_cursor, ':');
    if (path_sep) {
      path_next = path_sep + 1;
      *path_sep = '\0';
    }
    sprintf(fn, "%s/%sgcc", path_cursor, prefix);

    if (access(fn, X_OK) == 0) {
      return 0;
    }
  }

  return 1;
}

static const char *find_cross_compiler(const char *arch_string) {
  uint32_t i;

  for (i = 0; crosslist[i].arch; i++) {
    if (strcmp(crosslist[i].arch, arch_string) != 0)
      continue;
    if (path_try(crosslist[i].cross_prefix) == 0)
      return crosslist[i].cross_prefix;
  }

  return NULL;
}

// ---------------------------------------------------------------------------

static char *string_toupper(const char *input_string) {
  char *output_string;
  uint32_t len, i;

  len = strlen(input_string);

  output_string = malloc(len + 1);

  for (i = 0; i <= len; i++) {
    if ((input_string[i] >= 0x61) && (input_string[i] <= 0x7a)) {
      output_string[i] = input_string[i] ^ 0x20;
    } else {
      output_string[i] = input_string[i];
    }
  }

  return output_string;
}

// ---------------------------------------------------------------------------

static int rewrite_makeconf(const char *makeconf_filename) {
  int fd;

  fd = open(makeconf_filename, O_WRONLY | O_TRUNC | O_CREAT, 0644);
  if (fd < 0) {
    return 1;
  }

  if (write(fd, makeconf_buffer, makeconf_size) < makeconf_size) {
    return 1;
  }

  close(fd);

  return 0;
}

// ---------------------------------------------------------------------------

static int compare_makeconf(const char *makeconf_filename) {
  void *mem;
  int fd;
  struct stat stt;
  int equal;

  fd = open(makeconf_filename, O_RDONLY);
  if (fd < 0) {
    return 1;
  }
  if (fstat(fd, &stt)) {
    close(fd);
    return 1;
  }

  if (makeconf_size != stt.st_size) {
    close(fd);
    return 1;
  }

  mem = mmap(NULL, stt.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if (mem == MAP_FAILED) {
    close(fd);
    return 1;
  }

  equal = memcmp(mem, (void *)makeconf_buffer, makeconf_size);

  munmap(mem, stt.st_size);
  close(fd);

  return equal;
}

// ---------------------------------------------------------------------------

static int generate_makeconf(struct xmlnode *scene) {
  struct xmlnode *platform, *hypervisor, *feature;
  const char *cross_prefix;
  uint64_t i;

  makeconf_buffer = malloc(32768);

  platform = get_child(scene, STRUCT_PLATFORM);

  makeconf_printf("ARCH := %s\n",
  		platform->attrs[PLATFORM_ATTR_ARCH].value.string);
  makeconf_printf("PLATFORM := %s\n",
  		platform->attrs[PLATFORM_ATTR_BOARD].value.string);

  cross_prefix = find_cross_compiler(platform->attrs[PLATFORM_ATTR_ARCH].value.string);
  if (cross_prefix == NULL) {
    fprintf(stderr, "ERROR: no cross-compiler found.\n");
    return 1;
  }
  makeconf_printf("TARGET_PREFIX := %s\n",
  		cross_prefix);

  hypervisor = get_child(scene, STRUCT_HYPERVISOR);
  iterate_over_children(i, hypervisor, STRUCT_FEATURE, feature) {
    if (strcmp(feature->attrs[FEATURE_ATTR_NAME].value.string, "debugger") == 0) {
      makeconf_printf("FEATURE_DEBUG := %s\n",
		feature->attrs[FEATURE_ATTR_VALUE].value.string);
    } else if (strcmp(feature->attrs[FEATURE_ATTR_NAME].value.string, "driver:uart") == 0) {
      makeconf_printf("UART_DRIVER_%s := yes\n",
		string_toupper(feature->attrs[FEATURE_ATTR_VALUE].value.string));
    } else if (strcmp(feature->attrs[FEATURE_ATTR_NAME].value.string, "driver:timer") == 0) {
      makeconf_printf("TIMER_DRIVER_%s := yes\n",
		string_toupper(feature->attrs[FEATURE_ATTR_VALUE].value.string));
    } else if (strcmp(feature->attrs[FEATURE_ATTR_NAME].value.string, "driver:clock") == 0) {
      makeconf_printf("CLOCK_DRIVER_%s := yes\n",
		string_toupper(feature->attrs[FEATURE_ATTR_VALUE].value.string));
    } else if (strcmp(feature->attrs[FEATURE_ATTR_NAME].value.string, "driver:irq") == 0) {
      makeconf_printf("IRQ_DRIVER_%s := yes\n",
		string_toupper(feature->attrs[FEATURE_ATTR_VALUE].value.string));
    }
  }

  return 0;
}

// ---------------------------------------------------------------------------

int main(int an, char **ac) {
  struct xmlnode *scene;
  char fn[256];

  if (an != 2) {
    fprintf(stderr, "ERROR: need build directory argument.\n");
    return 1;
  }

  sprintf(fn, "%s/scenario.xml", ac[1]);
  scene = parse_from_file(fn, STRUCT_SCENARIO);
  if (!scene) {
    fprintf(stderr, "ERROR: could not load scenario: %s.\n", xml_errormsg);
    return 1;
  }

  if (generate_makeconf(scene)) {
    fprintf(stderr, "ERROR: could not generate $(O)/Makeconf contents.\n");
    return 1;
  }

  sprintf(fn, "%s/Makeconf", ac[1]);
  if (compare_makeconf(fn)) {
    if (rewrite_makeconf(fn)) {
      fprintf(stderr, "ERROR: could not rewrite $(O)/Makeconf.\n");
      return 1;
    }
    fprintf(stderr, "INFO: Makeconf updated.\n");
  }

  return 0;
}
