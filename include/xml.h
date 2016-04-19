#include <stdint.h>
#include <_gen_xml_structs.h>

#define	ATTRSTATE_NP		0
#define	ATTRSTATE_UNPARSED	1
#define	ATTRSTATE_CLEAN		2
#define	ATTRSTATE_MODIFIED	3
#define	ATTRSTATE_DELETED	4

#define	ATTRTYPE_HEX		1
#define	ATTRTYPE_DEC		2
#define	ATTRTYPE_CHAR		3
#define	ATTRTYPE_STRING		4
#define	ATTRTYPE_ID		5
#define	ATTRTYPE_IDREF		6
#define	ATTRTYPE_LIST		7
#define	ATTRTYPE_TABLE		8
#define	ATTRTYPE_FILE		9

#define	ATTR_WILDCARD		((void *)0x1)

extern char xml_errormsg[256];

union xmlattrval;

struct xmllist {
  uint32_t num;
  uint32_t *elem;
};

struct xmldict {
  uint32_t num;
  uint32_t *keys;
  union xmlattrval *vals;
};

union xmlattrval {
  uint64_t number;
  uint8_t character;
  char *string;
  struct xmllist list;
  struct xmldict dict;
};

struct xmlattr {
  char *str_value;
  uint32_t attr_type;
  uint32_t state;
  union xmlattrval value;
};

struct xmlnode {
  uint32_t node_type;
  struct xmlattr attrs[MAX_ATTR_COUNT];
  uint32_t num_children;
  struct xmlnode *children;
  struct xmlnode *parent;
};

struct xmlnode *parse_from_memory(char *data, uint32_t length, uint32_t root_node_type);
struct xmlnode *parse_from_file(const char *fn, uint32_t root_node_type);
void write_to_file(const char *fn, struct xmlnode *tree);

extern struct xmlnode *parse_node(char **, unsigned int);
extern void write_node(int, struct xmlnode *);

extern int validate_node_recursive(struct xmlnode *);

extern int validate_node_one_generated(struct xmlnode *);
extern void write_node_one_generated(int, struct xmlnode *, unsigned int);

struct xmlnode *resolve_idref(struct xmlnode *tree, struct xmlattr *attr);

struct xmlnode *get_nth_child(struct xmlnode *node, uint32_t nth, uint32_t child_type);
struct xmlnode *get_child(struct xmlnode *node, uint32_t child_type);
void add_child(struct xmlnode *node, struct xmlnode *child);
uint32_t count_children(struct xmlnode *node, uint32_t child_type);

void alloc_list(struct xmlattr *);
int has_list(struct xmlattr *attr, uint32_t item);
int list_index(struct xmlattr *attr, uint32_t item);

uint64_t get_dict_hex(struct xmlattr *attr, uint32_t index);
uint64_t get_dict_dec(struct xmlattr *attr, uint32_t index);
int has_dict(struct xmlattr *attr, uint32_t index);

void add_dict_hex(struct xmlattr *attr, uint32_t index, uint64_t number);
void add_dict_dec(struct xmlattr *attr, uint32_t index, uint64_t number);
void set_dict_wildcard_hex(struct xmlattr *attr, uint64_t number);
void set_dict_wildcard_dec(struct xmlattr *attr, uint64_t number);

static inline int attr_exists(struct xmlattr *attr) {
  return (attr->state == ATTRSTATE_CLEAN) || (attr->state == ATTRSTATE_MODIFIED);
}

#define		iterate_over_children(i, parent, child_type, child)	\
  for (i = 0; (child = get_nth_child(parent, i, child_type)); i++)
