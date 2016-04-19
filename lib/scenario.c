#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include "xml.h"
#include "scenario.h"

struct xmlnode *parse_from_memory(char *data, uint32_t length, uint32_t root_node_type) {
  struct xmlnode *xml;
  char *dataptr = data;
  int validate_error;

  if (strncmp(dataptr, "<?xml", 5) == 0) {
    dataptr = strchr(dataptr+5, '<');
  }

  if (strncmp(dataptr, "<!DOCTYPE", 9) == 0) {
    dataptr = strchr(dataptr+9, '<');
  }

  *xml_errormsg = 0;
  xml = parse_node(&dataptr, length - (dataptr - data));
  if (*xml_errormsg) {
    sprintf(xml_errormsg + strlen(xml_errormsg), " [%.16s...]", dataptr);
  }

  if (xml == NULL) {
    return NULL;
  }

  if (xml->node_type != root_node_type) {
    sprintf(xml_errormsg, "Root node has wrong type (expected %d, found %d)",
		root_node_type, xml->node_type);
    return NULL;
  }

  validate_error = validate_node_recursive(xml);
  if (validate_error) {
    return NULL;
  }

  return xml;
}

struct xmlnode *parse_from_file(const char *fn, uint32_t root_node_type) {
  struct stat fs;
  int fd;
  char *fdata;

  fd = open(fn, O_RDONLY);
  if (fd < 0)
    return NULL;

  if (fstat(fd, &fs))
    return NULL;

  fdata = malloc(fs.st_size);
  if (!fdata)
    return NULL;

  read(fd, fdata, fs.st_size);
  close(fd);

  return parse_from_memory(fdata, fs.st_size, root_node_type);
}

void write_to_file(const char *fn, struct xmlnode *tree) {
  int fd;

  fd = open(fn, O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd < 0) return;

  write_node_one_generated(fd, tree, 0);

  close(fd);
}

// ---------------------------------------------------------------------------

struct xmlnode *get_nth_child(struct xmlnode *node, uint32_t nth, uint32_t child_type) {
  uint32_t i, count;

  for (i = 0, count = 0; i < node->num_children; i++) {
    if (node->children[i].node_type == child_type) {
      if (count == nth)
        return node->children + i;
      count++;
    }
  }

  return NULL;
}

struct xmlnode *get_child(struct xmlnode *node, uint32_t child_type) {
  return get_nth_child(node, 0, child_type);
}

void add_child(struct xmlnode *node, struct xmlnode *child) {
  node->num_children++;

  node->children = realloc(node->children, node->num_children * sizeof(struct xmlnode));

  memcpy(node->children + (node->num_children - 1), child, sizeof(*child));
  node->children[node->num_children - 1].parent = node;
}

uint32_t count_children(struct xmlnode *node, uint32_t child_type) {
  uint32_t i, count = 0;

  for (i = 0; i < node->num_children; i++)
    if (node->children[i].node_type == child_type) ++count;

  return count;
}
