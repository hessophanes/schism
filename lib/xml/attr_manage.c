#include <stdlib.h>
#include "xml.h"

void alloc_list(struct xmlattr *attr) {
  attr->value.list.elem = malloc(sizeof(uint32_t) * attr->value.list.num);
}

int has_list(struct xmlattr *attr, uint32_t item) {
  uint32_t i;

  if ((attr->value.list.num == 1) && (attr->value.list.elem == ATTR_WILDCARD))
    return 1;

  for (i = 0; i < attr->value.list.num; i++) {
    if (attr->value.list.elem[i] == item)
      return 1;
  }

  return 0;
}

int list_index(struct xmlattr *attr, uint32_t item) {
  uint32_t i;

  if ((attr->value.list.num == 1) && (attr->value.list.elem == ATTR_WILDCARD))
    return -1;

  for (i = 0; i < attr->value.list.num; i++) {
    if (attr->value.list.elem[i] == item)
      return i;
  }

  return -1;
}

uint64_t get_dict_hex(struct xmlattr *attr, uint32_t index) {
  uint32_t i;

  if ((attr->value.dict.num == 1) && (attr->value.dict.keys == ATTR_WILDCARD)) {
    return attr->value.dict.vals[0].number;
  }

  for (i = 0; i < attr->value.dict.num; i++) {
    if (attr->value.dict.keys[i] == index)
      return attr->value.dict.vals[i].number;
  }

  return 0;
}

uint64_t get_dict_dec(struct xmlattr *attr, uint32_t index) {
  return get_dict_hex(attr, index);
}

int has_dict(struct xmlattr *attr, uint32_t index) {
  uint32_t i;

  if ((attr->value.dict.num == 1) && (attr->value.dict.keys == ATTR_WILDCARD)) {
    return 1;
  }

  for (i = 0; i < attr->value.dict.num; i++) {
    if (attr->value.dict.keys[i] == index)
      return 1;
  }

  return 0;
}

void add_dict_hex(struct xmlattr *attr, uint32_t index, uint64_t number) {
  if (has_dict(attr, index)) {
    abort();
    // covers both wildcard and pre-existing entry: both should be fatal
  }

  attr->value.dict.num++;
  attr->value.dict.keys = realloc(attr->value.dict.keys, attr->value.dict.num * sizeof(uint32_t));
  attr->value.dict.vals = realloc(attr->value.dict.vals, attr->value.dict.num * sizeof(union xmlattrval));

  attr->value.dict.keys[attr->value.dict.num-1] = index;
  attr->value.dict.vals[attr->value.dict.num-1].number = number;
  attr->state = ATTRSTATE_MODIFIED;
}

void add_dict_dec(struct xmlattr *attr, uint32_t index, uint64_t number) {
  add_dict_hex(attr, index, number);
}

void set_dict_wildcard_hex(struct xmlattr *attr, uint64_t number) {
  if (attr->value.dict.num) {
    abort();
  }

  attr->value.dict.num = 1;
  attr->value.dict.keys = ATTR_WILDCARD;
  attr->value.dict.vals = malloc(sizeof(union xmlattrval));

  attr->value.dict.vals[0].number = number;
  attr->state = ATTRSTATE_MODIFIED;
}

void set_dict_wildcard_dec(struct xmlattr *attr, uint64_t number) {
  set_dict_wildcard_hex(attr, number);
}
