#include <xml.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *idcharset = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_:";

int _parse_attr_hex(struct xmlnode *node, struct xmlattr *attr) {
  char *end = NULL;
  uint64_t val;

  (void)node;
  val = strtoul(attr->str_value, &end, 16);

  if (*end) return 1;

  attr->value.number = val;
  attr->attr_type = ATTRTYPE_HEX;
  attr->state = ATTRSTATE_CLEAN;
  return 0;
}

int _parse_attr_dec(struct xmlnode *node, struct xmlattr *attr) {
  char *end = NULL;
  uint64_t val;

  (void)node;
  val = strtoul(attr->str_value, &end, 10);

  if (*end) return 1;

  attr->value.number = val;
  attr->attr_type = ATTRTYPE_DEC;
  attr->state = ATTRSTATE_CLEAN;
  return 0;
}

int _parse_attr_char(struct xmlnode *node, struct xmlattr *attr) {
  (void)node;

  if (attr->str_value[1] != 0) return 1;
  if ((attr->str_value[0] < 0x20) || (attr->str_value[0] & 0x80)) return 1;

  attr->value.character = attr->str_value[0];
  attr->attr_type = ATTRTYPE_CHAR;
  attr->state = ATTRSTATE_CLEAN;
  return 0;
}

int _parse_attr_string(struct xmlnode *node, struct xmlattr *attr) {
  (void)node;

  attr->value.string = attr->str_value;
  attr->attr_type = ATTRTYPE_STRING;
  attr->state = ATTRSTATE_CLEAN;
  return 0;
}

int _parse_attr_id(struct xmlnode *node, struct xmlattr *attr) {
  (void)node;

  if (strspn(attr->str_value, idcharset) != strlen(attr->str_value))
    return 1;

  attr->value.string = attr->str_value;
  attr->attr_type = ATTRTYPE_ID;
  attr->state = ATTRSTATE_CLEAN;
  return 0;
}

int _parse_attr_idref(struct xmlnode *node, struct xmlattr *attr) {
  (void)node;

  if (strspn(attr->str_value, idcharset) != strlen(attr->str_value))
    return 1;

  attr->value.string = attr->str_value;
  attr->attr_type = ATTRTYPE_IDREF;
  attr->state = ATTRSTATE_CLEAN;
  return 0;
}

int _parse_attr_file(struct xmlnode *node, struct xmlattr *attr) {
  struct stat st;

  (void)node;
  if (stat(attr->str_value, &st) != 0)
    return 1;

  attr->value.string = attr->str_value;
  attr->attr_type = ATTRTYPE_FILE;
  attr->state = ATTRSTATE_CLEAN;
  return 0;
}

int _parse_attr_list(struct xmlnode *node, struct xmlattr *attr) {
  char *pos, *end = NULL;
  uint32_t i;

  (void)node;
  if (strcmp(attr->str_value, "*") == 0) {
    attr->value.list.num = 1;
    attr->value.list.elem = ATTR_WILDCARD;
    attr->attr_type = ATTRTYPE_LIST;
    attr->state = ATTRSTATE_CLEAN;
    return 0;
  }

  if (attr->str_value[0] != '[') return 1;

  attr->value.list.num = 1;
  for (pos = attr->str_value + 1; *pos; pos++)
    if (*pos == ',')
      attr->value.list.num++;

  attr->value.list.elem = malloc(attr->value.list.num * sizeof(uint32_t));

  pos = attr->str_value + 1;

  for (i = 0; i < attr->value.list.num; i++) {
    end = NULL;
    attr->value.list.elem[i] = (uint32_t)strtol(pos, &end, 10);
    if ((i+1 < attr->value.list.num) && (*end != ',')) goto list_error;
    if ((i+1 == attr->value.list.num) && (*end != ']')) goto list_error;
    pos = end+1;
  }

  attr->attr_type = ATTRTYPE_LIST;
  attr->state = ATTRSTATE_CLEAN;
  return 0;

list_error:
  free(attr->value.list.elem);
  return 1;
}

int _parse_attr_table_hex(struct xmlnode *node, struct xmlattr *attr) {
  uint64_t val;
  char *pos, *end = NULL;
  uint32_t i;

  (void)node;
  val = strtoul(attr->str_value, &end, 16);

  if (*end == 0) {
    attr->value.dict.num = 1;
    attr->value.dict.keys = ATTR_WILDCARD;
    attr->value.dict.vals = malloc(sizeof(union xmlattrval));
    attr->value.dict.vals[0].number = val;
    attr->attr_type = ATTRTYPE_TABLE;
    attr->state = ATTRSTATE_CLEAN;
    return 0;
  }

  attr->value.dict.num = 1;
  for (pos = attr->str_value + 1; *pos; pos++)
    if (*pos == ';')
      attr->value.dict.num++;

  attr->value.dict.keys = malloc(attr->value.dict.num * sizeof(uint32_t));
  attr->value.dict.vals = malloc(attr->value.dict.num * sizeof(union xmlattrval));

  pos = attr->str_value;
  for (i = 0; i < attr->value.dict.num; i++) {
    if (*pos != '[') goto dict_error;
    attr->value.dict.keys[i] = (uint32_t)strtol(pos+1, &end, 10);
    if (*end != ']') goto dict_error;
    pos = end+1;
    if (*pos != '=') goto dict_error;
    attr->value.dict.vals[i].number = (uint64_t)strtoul(pos+1, &end, 16);
    if ((i+1 < attr->value.dict.num) && (*end != ';')) goto dict_error;
    if ((i+1 == attr->value.dict.num) && (*end != 0)) goto dict_error;
    pos = end+1;
  }

  attr->attr_type = ATTRTYPE_TABLE;
  attr->state = ATTRSTATE_CLEAN;
  return 0;

dict_error:
  free(attr->value.dict.keys);
  free(attr->value.dict.vals);
  return 1;
}

int _parse_attr_table_dec(struct xmlnode *node, struct xmlattr *attr) {
  uint64_t val;
  char *pos, *end = NULL;
  uint32_t i;

  (void)node;
  val = strtoul(attr->str_value, &end, 10);

  if (*end == 0) {
    attr->value.dict.num = 1;
    attr->value.dict.keys = ATTR_WILDCARD;
    attr->value.dict.vals = malloc(sizeof(union xmlattrval));
    attr->value.dict.vals[0].number = val;
    attr->attr_type = ATTRTYPE_TABLE;
    attr->state = ATTRSTATE_CLEAN;
    return 0;
  }

  attr->value.dict.num = 1;
  for (pos = attr->str_value + 1; *pos; pos++)
    if (*pos == ';')
      attr->value.dict.num++;

  attr->value.dict.keys = malloc(attr->value.dict.num * sizeof(uint32_t));
  attr->value.dict.vals = malloc(attr->value.dict.num * sizeof(union xmlattrval));

  pos = attr->str_value;
  for (i = 0; i < attr->value.dict.num; i++) {
    if (*pos != '[') goto dict_error;
    attr->value.dict.keys[i] = (uint32_t)strtol(pos+1, &end, 10);
    if (*end != ']') goto dict_error;
    pos = end+1;
    if (*pos != '=') goto dict_error;
    attr->value.dict.vals[i].number = (uint64_t)strtoul(pos+1, &end, 10);
    if ((i+1 < attr->value.dict.num) && (*end != ';')) goto dict_error;
    if ((i+1 == attr->value.dict.num) && (*end != 0)) goto dict_error;
    pos = end+1;
  }

  attr->attr_type = ATTRTYPE_TABLE;
  attr->state = ATTRSTATE_CLEAN;
  return 0;

dict_error:
  free(attr->value.dict.keys);
  free(attr->value.dict.vals);
  return 1;
}

struct xmlnode *resolve_idref(struct xmlnode *tree, struct xmlattr *attr) {
  struct xmlnode *ref;
  uint32_t i;

  for (i = 0; i < MAX_ATTR_COUNT; i++) {
    if ((tree->attrs[i].state == ATTRSTATE_CLEAN) &&
	(tree->attrs[i].attr_type == ATTRTYPE_ID) &&
	(strcmp(tree->attrs[i].value.string, attr->value.string) == 0)) {
      return tree;
    }
  }

  for (i = 0; i < tree->num_children; i++) {
    ref = resolve_idref(tree->children + i, attr);
    if (ref)
      return ref;
  }

  return NULL;
}
