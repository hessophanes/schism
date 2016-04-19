#include <xml.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

char xml_errormsg[256];

struct xmlnode *parse_node(char **data, uint32_t size) {
  struct xmlnode *node, *child_node;
  uint32_t i;
  uint32_t elem_id = ~0;
  char *p = *data;
  char *close_tag;

  while ((*p == ' ') || (*p == '\t') || (*p == '\n') || (*p == '\r'))
    p++;

  if (*p != '<') {
    sprintf(xml_errormsg, "EP001: expected an opening tag");
    goto error;
  }
  p++;

  for (i = 0; i < E2ID_COUNT; i++) {
    uint32_t n = strlen(elem_names[i].name);
    if ( (strncmp(p, elem_names[i].name, n) == 0) &&
	 !(p[n] >= 0x41 && p[n] <= 0x5a) &&
	 !(p[n] >= 0x61 && p[n] <= 0x7a) &&
	 !(p[n] == 0x5f)) {
      elem_id = elem_names[i].id;
      close_tag = malloc(n + 4);
      sprintf(close_tag, "</%s>", elem_names[i].name);
      p += n;
      break;
    }
  }

  if (i == E2ID_COUNT) {
    sprintf(xml_errormsg, "EP002: unrecognized element");
    goto error;
  }

  node = malloc(sizeof(struct xmlnode));
  node->node_type = elem_id;
  memset(node->attrs, 0, sizeof(node->attrs));
  node->num_children = 0;
  node->children = NULL;
  node->parent = NULL;

  /* attribute parse loop */
  while (1) {
    char *attr_end;

    while ((*p == ' ') || (*p == '\t') || (*p == '\n') || (*p == '\r'))
      p++;

    if ((p[0] == '/') && (p[1] == '>')) {
      p += 2;
      goto success;
    }

    if (p[0] == '>') {
      p++;
      break;
    }

    for (i = 0; i < A2ID_COUNT; i++) {
      uint32_t n = strlen(attr_names[i].name);
      if (attr_names[i].struct_id != node->node_type)
        continue;
      if ( (strncmp(p, attr_names[i].name, n) == 0) &&
		(p[n] == '=') && (p[n+1] == '"')) {
        p += n+2;
        break;
      }
    }

    if (i == A2ID_COUNT) {
      sprintf(xml_errormsg, "EP003: unrecognized attribute of element %d", elem_id);
      goto error;
    }

    attr_end = strchr(p, '"');
    if (attr_end == NULL) {
      sprintf(xml_errormsg, "EP004: attribute not quoted properly");
      goto error;
    }
    node->attrs[attr_names[i].id].state = ATTRSTATE_UNPARSED;
    node->attrs[attr_names[i].id].attr_type = attr_names[i].attr_type;
    node->attrs[attr_names[i].id].str_value = p;
    *attr_end = '\0';
    p = attr_end+1;
  }

  /* children parse loop */
  while (1) {

    while ((*p == ' ') || (*p == '\t') || (*p == '\n') || (*p == '\r'))
      p++;

    if (strncmp(p, close_tag, strlen(close_tag)) == 0) {
      p += strlen(close_tag);
      goto success;
    }

    node->num_children++;
    node->children = realloc(node->children, node->num_children * sizeof(struct xmlnode));

    child_node = parse_node(&p, size - (*data-p));
    if (child_node == NULL) {
      // xml_errormsg set by recursive call
      goto error;
    }
    child_node->parent = node;
    memcpy(node->children + node->num_children - 1, child_node, sizeof(struct xmlnode));
    free(child_node);
  }

success:
  *data = p;

  return node;

error:
  *data = p;

  return NULL;
}

int validate_node_recursive(struct xmlnode *node) {
  uint32_t i;
  int ret;

  ret = validate_node_one_generated(node);
  if (ret)
    return ret;

  for (i = 0; i < node->num_children; i++) {
    ret = validate_node_recursive(node->children + i);
    if (ret)
      return ret;
  }

  return 0;
}
