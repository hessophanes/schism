#include <xml.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

void _regenerate_attr_hex(struct xmlnode *node, struct xmlattr *attr) {
  (void)node;

  attr->str_value = malloc(20);
  sprintf(attr->str_value, "0x%lx", attr->value.number);
  attr->state = ATTRSTATE_CLEAN;
}

void _regenerate_attr_dec(struct xmlnode *node, struct xmlattr *attr) {
  (void)node;

  attr->str_value = malloc(24);
  sprintf(attr->str_value, "%ld", attr->value.number);
  attr->state = ATTRSTATE_CLEAN;
}

void _regenerate_attr_char(struct xmlnode *node, struct xmlattr *attr) {
  (void)node;

  attr->str_value = malloc(4);
  sprintf(attr->str_value, "%c", attr->value.character);
  attr->state = ATTRSTATE_CLEAN;
}

void _regenerate_attr_string(struct xmlnode *node, struct xmlattr *attr) {
  (void)node;

  attr->str_value = attr->value.string;
  attr->state = ATTRSTATE_CLEAN;
}

void _regenerate_attr_id(struct xmlnode *node, struct xmlattr *attr) {
  _regenerate_attr_string(node, attr);
}

void _regenerate_attr_idref(struct xmlnode *node, struct xmlattr *attr) {
  _regenerate_attr_string(node, attr);
}

void _regenerate_attr_file(struct xmlnode *node, struct xmlattr *attr) {
  _regenerate_attr_string(node, attr);
}

void _regenerate_attr_list(struct xmlnode *node, struct xmlattr *attr) {
  (void)node;

  if ((attr->value.list.num == 1) && (attr->value.list.elem == ATTR_WILDCARD)) {
    attr->str_value = malloc(4);
    sprintf(attr->str_value, "*");
  } else {
    uint32_t i;

    attr->str_value = malloc(attr->value.list.num * 4);
    attr->str_value[0] = '\0';
    for (i = 0; i < attr->value.list.num; i++) {
      sprintf(attr->str_value + strlen(attr->str_value), ",%d", attr->value.list.elem[i]);
    }
    attr->str_value[0] = '[';
    sprintf(attr->str_value + strlen(attr->str_value), "]");
  }
  attr->state = ATTRSTATE_CLEAN;
}

void _regenerate_attr_table_hex(struct xmlnode *node, struct xmlattr *attr) {
  (void)node;

  if ((attr->value.dict.num == 1) && (attr->value.dict.keys == ATTR_WILDCARD)) {
    attr->str_value = malloc(20);
    sprintf(attr->str_value, "0x%lx", attr->value.dict.vals[0].number);
  } else if (attr->value.dict.num == 0) {
    attr->state = ATTRSTATE_DELETED;
  } else {
    uint32_t i;

    attr->str_value = malloc(attr->value.dict.num * 28);
    attr->str_value[0] = '\0';
    for (i = 0; i < attr->value.dict.num; i++) {
      sprintf(attr->str_value + strlen(attr->str_value), "[%d]=0x%lx;",
		attr->value.dict.keys[i], attr->value.dict.vals[i].number);
    }
    attr->str_value[strlen(attr->str_value)-1] = '\0';
  }
  attr->state = ATTRSTATE_CLEAN;
}

void _regenerate_attr_table_dec(struct xmlnode *node, struct xmlattr *attr) {
  (void)node;

  if ((attr->value.dict.num == 1) && (attr->value.dict.keys == ATTR_WILDCARD)) {
    attr->str_value = malloc(20);
    sprintf(attr->str_value, "0x%lx", attr->value.dict.vals[0].number);
  } else if (attr->value.dict.num == 0) {
    attr->state = ATTRSTATE_DELETED;
  } else {
    uint32_t i;

    attr->str_value = malloc(attr->value.dict.num * 28);
    attr->str_value[0] = '\0';
    for (i = 0; i < attr->value.dict.num; i++) {
      sprintf(attr->str_value + strlen(attr->str_value), "[%d]=%ld;",
		attr->value.dict.keys[i], attr->value.dict.vals[i].number);
    }
    attr->str_value[strlen(attr->str_value)-1] = '\0';
  }
  attr->state = ATTRSTATE_CLEAN;
}
