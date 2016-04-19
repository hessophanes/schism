#include "generate_config.h"

int generator_irq_gic(struct xmlnode *scene, struct xmlnode *guest, struct xmlnode *vdev) {
  struct definition *def;
  uint32_t j;

  (void)scene; (void)guest;

  for (j = 0; j < guest->attrs[GUEST_ATTR_CPUMAP].value.list.num; j++) {
    def = add_definition(SECTION_RW, "emulate_irq_gic", NULL);
    def->identifier = malloc(64);
    sprintf(def->identifier, "vdev_%s_cpu%d",
			vdev->attrs[VDEV_ATTR_ID].value.string,
			j);
    def->initializer = malloc(256);
    sprintf(def->initializer, "{ 0, 0, 0, 0, {}, {}, {}, {}, &vdev_%s_dist }",
			vdev->attrs[VDEV_ATTR_ID].value.string);
  }

  def = add_definition(SECTION_RW_SHARED, "emulate_irq_gic_distributor", NULL);
  def->identifier = malloc(64);
  sprintf(def->identifier, "vdev_%s_dist",
			vdev->attrs[VDEV_ATTR_ID].value.string);
  def->initializer = "{}";

  return 0;
}
