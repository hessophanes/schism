#include "generate_config.h"

const char *config_include_list = \
	"#include <phidias.h>\n"
	"#include <vm.h>\n"
	"#include <emulate/core.h>\n"
	"#include <emulate/uart.h>\n"
	"#include <emulate/irq.h>\n"
	"#include <emulate/timer.h>\n"
	"#include <emulate/clock.h>\n"
	"#include <emulate/vtlb.h>\n"
	"#include <emulate/memory.h>\n"
	"#include <arch/cpu_state.h>\n"
	"#include <schedule.h>\n"
	"#include <specification.h>\n";

// ---------------------------------------------------------------------------

int emit_config_base_address(struct xmlnode *scene, const char *filename) {
  int fd;
  struct xmlnode *map;
  char base_expression[32];

  fd = open(filename, O_CREAT | O_TRUNC | O_WRONLY, 0644);
  if (fd < 0) {
    fprintf(stderr, "ERROR: could not open output file.\n");
    return 1;
  }

  map = find_hypervisor_map(scene, "config_r");
  sprintf(base_expression, "CONFIG_START = 0x%lx;\n", get_dict_hex(map->attrs + MAP_ATTR_BASE, 0));
  write(fd, base_expression, strlen(base_expression));

  close(fd);

  return 0;
}

// ---------------------------------------------------------------------------

int main(int an, char **ac) {
  struct xmlnode *scene;
  char fn[128];
  const char *pattern_in, *pattern_out, *pattern_base_address_file;

  if (an != 3) {
    fprintf(stderr, "ERROR: need build directory argument and iteration number.\n");
    return 1;
  }

  switch (atoi(ac[2])) {
  case 1:
	pattern_in = "%s/scenario_reparented.xml";
	pattern_out = "%s/scenario_config.c";
	pattern_base_address_file = NULL;
	break;
  case 2:
	pattern_in = "%s/scenario_pagetables.xml";
	pattern_out = "%s/scenario_config_real.c";
	pattern_base_address_file = "%s/config_base.lds";
	break;
  default:
	fprintf(stderr, "ERROR: invalid iteration number.\n");
	return 1;
  }

  sprintf(fn, pattern_in, ac[1]);
  scene = parse_from_file(fn, STRUCT_SCENARIO);
  if (!scene) {
    fprintf(stderr, "ERROR: could not load expanded scenario: %s\n", xml_errormsg);
    return 1;
  }

  if (generate_global_data(scene)) {
    return 1;
  }

  if (generate_vm_data(scene)) {
    return 1;
  }

  sprintf(fn, pattern_out, ac[1]);
  if (emit_definitions(fn, config_include_list)) {
    return 1;
  }

  if (pattern_base_address_file) {
    sprintf(fn, pattern_base_address_file, ac[1]);
    if (emit_config_base_address(scene, fn)) {
      return 1;
    }
  }

  return 0;
}
