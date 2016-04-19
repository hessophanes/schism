#include "generate_config.h"

int generator_clock_mpcore(struct xmlnode *scene, struct xmlnode *guest, struct xmlnode *vdev) {
  struct definition *def;

  (void)scene; (void)guest;

  def = add_definition(SECTION_RW, "emulate_clock_mpcore", NULL);
  def->identifier = malloc(64);
    sprintf(def->identifier, "vdev_%s",
			vdev->attrs[VDEV_ATTR_ID].value.string);
  def->initializer = "{}";

  return 0;
}
