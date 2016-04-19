#include "generate_config.h"

// ---------------------------------------------------------------------------

struct definition *definitions;
unsigned int num_definitions = 0;

const char section_defs[] = \
  "#define	SEC_ROH	__attribute__((section(\".rodata_head\")))\n" \
  "#define	SEC_RO	__attribute__((section(\".rodata\")))\n" \
  "#define	SEC_RW	__attribute__((section(\".data\")))\n" \
  "#define	SEC_RWS	__attribute__((section(\".data_shared\")))\n" \
  "\n";

const char *section_specifiers[6] = {
  NULL, "SEC_ROH ", "SEC_RO ", "SEC_RW ", "SEC_RWS ", ""
};

// ---------------------------------------------------------------------------

#define	_write(fd, str)		write(fd, str, strlen(str))

struct definition *add_definition(enum section section, char *type, char *identifier) {
  num_definitions++;
  definitions = realloc(definitions, num_definitions * sizeof(struct definition));

  definitions[num_definitions-1].section = section;
  definitions[num_definitions-1].type = type;
  definitions[num_definitions-1].identifier = identifier;
  definitions[num_definitions-1].initializer = NULL;

  return definitions + (num_definitions - 1);
}

// ---------------------------------------------------------------------------

int emit_definitions(const char *filename, const char *includes) {
  int fd;
  unsigned int i;
  struct definition *def;

  fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) {
    fprintf(stderr, "ERROR: could not open output file.\n");
    return 1;
  }

  write(fd, includes, strlen(includes));
  write(fd, section_defs, sizeof(section_defs)-1);

  for (i = 0; i < num_definitions; i++) {
    def = definitions + i;
    if (def->section == SECTION_EXTERN)
      _write(fd, "extern ");
    _write(fd, section_specifiers[def->section]);
    _write(fd, def->type);
    _write(fd, " ");
    if ((def->section == SECTION_RO_HEAD) || (def->section == SECTION_RO))
      _write(fd, "const ");
    _write(fd, def->identifier);
    _write(fd, ";\n");
  }

  _write(fd, "\n");

  for (i = 0; i < num_definitions; i++) {
    def = definitions + i;
    if (def->section == SECTION_EXTERN)
      continue;
    _write(fd, def->type);
    _write(fd, " ");
    if ((def->section == SECTION_RO_HEAD) || (def->section == SECTION_RO))
      _write(fd, "const ");
    _write(fd, def->identifier);
    _write(fd, " = ");
    _write(fd, def->initializer);
    _write(fd, ";\n");
  }

  close(fd);

  return 0;
}

// ---------------------------------------------------------------------------
