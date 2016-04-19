#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include "xml.h"
#include "scenario.h"

extern char * const *environ;

int main(int an, char **ac) {
  struct xmlnode *scene; 
  char fn[128];
  const char *scenario_file;

  if (an != 3) {
    fprintf(stderr, "ERROR: need build directory argument and iteration number.\n");
    return 1;
  }

  switch (atoi(ac[2])) {
  case 1:
        scenario_file = "%s/scenario_reparented.xml";
        break;
  case 2:
        scenario_file = "%s/scenario_pagetables.xml";
        break;
  default:
        fprintf(stderr, "ERROR: invalid iteration number.\n");
        return 1;
  }

  sprintf(fn, scenario_file, ac[1]);
  scene = parse_from_file(fn, STRUCT_SCENARIO);
  if (!scene) {
    fprintf(stderr, "ERROR: could not load expanded scenario: %s\n", xml_errormsg);
    return 1;
  }

  sprintf(fn, "./generate_config_%s", scene->attrs[SCENARIO_ATTR_CBI].value.string);

  (void)execve(fn, ac, environ);
  fprintf(stderr, "ERROR: could not execute CBI-specific utility.\n");

  return 1;
}
