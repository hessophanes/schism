#include "generate_config.h"

static int build_memory_array(struct xmlnode *vdev, const char *param, const char *def_param, uint64_t *defval) {
  struct xmlnode *value;
  uint64_t i;

  *defval = 0UL;

  iterate_over_children(i, vdev, STRUCT_VALUE, value) {
    if (strcmp(value->attrs[VALUE_ATTR_TYPE].value.string, def_param) == 0) {
      *defval = value->attrs[VALUE_ATTR_VALUE].value.number;
      break;
    }
  }

  iterate_over_children(i, vdev, STRUCT_VALUE, value) {
    if (strcmp(value->attrs[VALUE_ATTR_TYPE].value.string, param) == 0) {
      if (value->attrs[VALUE_ATTR_VALUE].value.number != *defval) {
        return 1;
      }
    }
  }

  return 0;
}

// --------------------------------------------------------------------------

int generator_memory32(struct xmlnode *scene, struct xmlnode *guest, struct xmlnode *vdev) {
  struct definition *def;
  char *mem_flags;
  int hasarray_value, hasarray_mem_rmask, hasarray_mem_wmask, hasarray_hw_rmask, hasarray_hw_wmask;
  uint64_t def_value, def_mem_rmask, def_mem_wmask, def_hw_rmask, def_hw_wmask;
  uint64_t hw_address = 0UL;
  uint64_t i;
  struct xmlnode *param, *value;

  (void)guest;

  mem_flags = malloc(256);
  mem_flags[0] = '\0';

  hasarray_value = build_memory_array(vdev, "mem_value", "default_mem_value", &def_value);
  if (!hasarray_value) sprintf(mem_flags + strlen(mem_flags), "|EMULATE_MEMORY_FLAG_SINGLE_VALUE");
  hasarray_mem_rmask = build_memory_array(vdev, "mask_mem_r", "default_mask_mem_r", &def_mem_rmask);
  if (!hasarray_mem_rmask) sprintf(mem_flags + strlen(mem_flags), "|EMULATE_MEMORY_FLAG_SINGLE_MEM_RMASK");
  hasarray_mem_wmask = build_memory_array(vdev, "mask_mem_w", "default_mask_mem_w", &def_mem_wmask);
  if (!hasarray_mem_wmask) sprintf(mem_flags + strlen(mem_flags), "|EMULATE_MEMORY_FLAG_SINGLE_MEM_WMASK");
  hasarray_hw_rmask = build_memory_array(vdev, "mask_hw_r", "default_mask_hw_r", &def_hw_rmask);
  if (!hasarray_hw_rmask) sprintf(mem_flags + strlen(mem_flags), "|EMULATE_MEMORY_FLAG_SINGLE_HW_RMASK");
  hasarray_hw_wmask = build_memory_array(vdev, "mask_hw_w", "default_mask_hw_w", &def_hw_wmask);
  if (!hasarray_hw_wmask) sprintf(mem_flags + strlen(mem_flags), "|EMULATE_MEMORY_FLAG_SINGLE_HW_WMASK");

  def = add_definition(SECTION_RO, "emulate_memory", NULL);
  def->identifier = malloc(64);
    sprintf(def->identifier, "vdev_%s",
			vdev->attrs[VDEV_ATTR_ID].value.string);
  def->initializer = malloc(512);
  sprintf(def->initializer, "{\n"
				"  %s,\n"
				"  0x%lx, 0x%lx, 0x%lx, 0x%lx, 0x%lx,\n",
			mem_flags+1,
			def_value, def_mem_rmask, def_mem_wmask, def_hw_rmask, def_hw_wmask);

  if (hasarray_value)
    sprintf(def->initializer + strlen(def->initializer), "  vdev_%s_values,\n",
			vdev->attrs[VDEV_ATTR_ID].value.string);
  else
    sprintf(def->initializer + strlen(def->initializer), "  NULL,\n");

  iterate_over_children(i, vdev, STRUCT_PARAM, param) {
    if (strcmp(param->attrs[PARAM_ATTR_TYPE].value.string, "hardware") == 0) {
      struct xmlnode *hw_map = find_hypervisor_map(scene, param->attrs[PARAM_ATTR_XREF].value.string);
      // struct xmlnode *hw_memreq = find_memreq(scene, hw_map->attrs[MAP_ATTR_XREF].value.string);
      // struct xmlnode *hw_device = find_device(scene, hw_map->attrs[MAP_ATTR_XREF].value.string);

      hw_address += get_dict_hex(hw_map->attrs + MAP_ATTR_BASE, -1);
    }
  }
  iterate_over_children(i, vdev, STRUCT_VALUE, value) {
    if (strcmp(value->attrs[VALUE_ATTR_TYPE].value.string, "hardware_offset") == 0) {
      hw_address += value->attrs[VALUE_ATTR_VALUE].value.number;
      break;
    }
  }
  sprintf(def->initializer + strlen(def->initializer), "  (volatile uint32_t *)0x%lx,\n",
			hw_address);

  if (hasarray_mem_rmask)
    sprintf(def->initializer + strlen(def->initializer), "  vdev_%s_mem_rmask,\n",
			vdev->attrs[VDEV_ATTR_ID].value.string);
  else
    sprintf(def->initializer + strlen(def->initializer), "  NULL,\n");

  if (hasarray_mem_wmask)
    sprintf(def->initializer + strlen(def->initializer), "  vdev_%s_mem_wmask,\n",
			vdev->attrs[VDEV_ATTR_ID].value.string);
  else
    sprintf(def->initializer + strlen(def->initializer), "  NULL,\n");

  if (hasarray_hw_rmask)
    sprintf(def->initializer + strlen(def->initializer), "  vdev_%s_hw_rmask,\n",
			vdev->attrs[VDEV_ATTR_ID].value.string);
  else
    sprintf(def->initializer + strlen(def->initializer), "  NULL,\n");

  if (hasarray_hw_wmask)
    sprintf(def->initializer + strlen(def->initializer), "  vdev_%s_hw_wmask\n}",
			vdev->attrs[VDEV_ATTR_ID].value.string);
  else
    sprintf(def->initializer + strlen(def->initializer), "  NULL\n}");

  return 0;
}
