#include "generate_config.h"

int generator_timer_sp804(struct xmlnode *scene, struct xmlnode *guest, struct xmlnode *vdev) {
  struct definition *def;

  (void)scene; (void)guest;

  def = add_definition(SECTION_RW, "emulate_timer_sp804", NULL);
  def->identifier = malloc(64);
    sprintf(def->identifier, "vdev_%s",
			vdev->attrs[VDEV_ATTR_ID].value.string);
  def->initializer = "{}";

  return 0;
}

int generator_timer_mpcore(struct xmlnode *scene, struct xmlnode *guest, struct xmlnode *vdev) {
  struct definition *def;
  uint32_t k;

  (void)scene;

  for (k = 0; k < guest->attrs[GUEST_ATTR_CPUMAP].value.list.num; k++) {
    def = add_definition(SECTION_RW, "emulate_timer_mpcore", NULL);
    def->identifier = malloc(64);
      sprintf(def->identifier, "vdev_%s_cpu%d",
			vdev->attrs[VDEV_ATTR_ID].value.string,
			k);
    def->initializer = "{}";
  }

  return 0;
}
