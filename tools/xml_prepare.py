#!/usr/bin/python

class XMLEnt:
	valid_opts = [ "REQUIRED", "OPTIONAL" ]
	valid_forms = [ "HEX", "DEC", "STRING", "CHAR", "ID", "TABLE", "LIST", "FILE", "IDREF" ]
	forms_with_inner = [ "TABLE", "IDREF" ]

class Struct(XMLEnt):

	def __init__(self, name):
		if not name.translate(None, "_").isalpha():
			raise ValueError("struct name is not a proper identifier")
		self.name = name
		self.enum_id = "STRUCT_" + name.upper()
		self.list_attrs = []
		self.list_1child = []
		self.list_nchild = []
		self.resolved = False
		self.container = None

	def add_attribute(self, attr):
		attr.container = self
		attr.index = len(self.list_attrs)
		attr.enum_id = "%s_ATTR_%s" % (self.name.upper(), attr.name.upper())
		self.list_attrs.append(attr)

	def add_child(self, name, opt):
		if not opt in self.valid_opts:
			raise ValueError("invalid value for req/opt")
		self.list_1child.append((name, (opt == "REQUIRED")))

	def add_children(self, name, quantity):
		q_min, q_max = quantity.split(":")
		q_min = int(q_min)
		if q_max == "n":
			q_max = None
		else:
			q_max = int(q_max)
		self.list_nchild.append((name, q_min, q_max))	

	def resolve_refs(self):
		for i, c in enumerate(self.list_1child):
			try:
				c_struct = structs[c[0]]
				self.list_1child[i] = (c_struct, c[1])
			except KeyError:
				raise ValueError("Unknown child node " + c[0] + " in element " + self.name)
		for i, c in enumerate(self.list_nchild):
			try:
				c_struct = structs[c[0]]
				self.list_nchild[i] = (c_struct, c[1], c[2])
			except KeyError:
				raise ValueError("Unknown child node " + c[0] + " in element " + self.name)
		self.resolved = True
		for a in self.list_attrs:
			if a.outer_form == "IDREF":
				a.parse_idref_typelist()

class Attribute(XMLEnt):

	def __init__(self, name, form, opt):
		self.container = None
		if not opt in self.valid_opts:
			raise ValueError("invalid value for req/opt")
		form_parsed = self.parse_form(form)
		if not form_parsed:
			raise ValueError("invalid form for attribute")
		self.outer_form, self.inner_form = form_parsed
		if (self.outer_form in self.forms_with_inner) and not self.inner_form:
			raise ValueError("missing nested form")
		if self.outer_form != "IDREF" and self.inner_form:
			self.function_suffix = self.outer_form.lower() + "_" + self.inner_form.lower()
		else:
			self.function_suffix = self.outer_form.lower()
		self.name = name
		if self.outer_form == "ID":
			self.dtd_type = "ID"
		elif self.outer_form == "IDREF":
			self.dtd_type = "IDREF"
		else:
			self.dtd_type = "CDATA"
		if opt == "REQUIRED":
			self.is_required = True
			self.dtd_req = "#REQUIRED"
		else:
			self.is_required = False
			self.dtd_req = "#IMPLIED"

	def parse_form(self, form):
		if form in self.valid_forms:
			return ( form, None )
		br_op = form.find("(")
		br_close = form.rfind(")")
		if br_op == -1 or br_close != len(form)-1:
			return None
		outer_form = form[0:br_op]
		inner_form = form[br_op+1:br_close]
		if not outer_form in self.valid_forms:
			return None
		if outer_form != "IDREF" and not inner_form in self.valid_forms:
			return None
		return ( outer_form, inner_form )

	def parse_idref_typelist(self):
		if not self.container:
			return True
		if not self.container.resolved:
			return True
		typelist = self.inner_form.split("|")
		typelist_int = ""
		for t in typelist:
			if not structs[t]:
				raise ValueError("unknown type " + t + " in IDREF() list")
			typelist_int += "| (1 << " + structs[t].enum_id + ")"
		self.inner_form = typelist_int[2:]
		return True

xml_def_file = open('include/xml_definition.txt', "r")
xml_def_lines = xml_def_file.readlines()

structs = {}
current_struct = None

for line in xml_def_lines:
	words = line.strip().split()
	if len(words) < 1:
		if current_struct:
			structs[current_struct.name] = current_struct
			current_struct = None
		continue
	if words[0].translate(None, "_").isalpha():
		# print("S+ " + line)
		current_struct = Struct(line.strip())
	elif words[0] == "*" and words[1] == "attribute":
		# print("A+ " + line)
		current_struct.add_attribute(Attribute(words[2], words[3], words[4]))
	elif words[0] == "*" and words[1] == "child":
		# print("C+ " + line)
		current_struct.add_child(words[2], words[3])
	elif words[0] == "*" and words[1] == "children":
		# print("CC+ " + line)
		current_struct.add_children(words[2], words[3])

if current_struct:
	structs[current_struct.name] = current_struct

for s in structs.values():
	s.resolve_refs()

# -----------------------------------------------------------------------------

xml_hdr_structs = open('include/_gen_xml_structs.h', "w")
xml_structs = open('lib/xml/_gen_structs.c', "w")

xml_structs.write("#include <xml.h>\n")
xml_structs.write("#include <stdint.h>\n\n")

xml_hdr_structs.write("enum node_type_t {\n")
xml_hdr_structs.write("  NODE_DELETED = 0,\n")
for s in structs.values():
	xml_hdr_structs.write("  " + s.enum_id + ",\n")
xml_hdr_structs.write("};\n\n")

max_attr = 0
for s in structs.values():
	for a in s.list_attrs:
		xml_hdr_structs.write("#define %s %d\n" % (a.enum_id, a.index))
	max_attr = max(len(s.list_attrs), max_attr)

xml_hdr_structs.write("#define MAX_ATTR_COUNT	%d\n\n" % (max_attr + 1))

xml_hdr_structs.write("struct e2id { char *name; uint32_t id; };\n")

xml_structs.write("struct e2id elem_names[] = {\n")
for s in structs.values():
	xml_structs.write("  { \"%s\", %s },\n" % (s.name, s.enum_id))
xml_structs.write("};\n")

xml_hdr_structs.write("struct e2id elem_names[%d];\n" % len(structs))
xml_hdr_structs.write("#define E2ID_COUNT	%d\n\n" % len(structs))

xml_hdr_structs.write("struct a2id { char *name; uint32_t struct_id; uint32_t id; uint32_t attr_type; };\n")

xml_structs.write("struct a2id attr_names[] = {\n")
count = 0
for s in structs.values():
	for a in s.list_attrs:
		xml_structs.write("  { \"%s\", %s, %s, %s },\n" % (a.name, s.enum_id, a.enum_id, "0"))
	count = count + len(s.list_attrs)
xml_structs.write("};\n")

xml_hdr_structs.write("struct a2id attr_names[%d];\n" % count)
xml_hdr_structs.write("#define A2ID_COUNT	%d\n\n" % count)

xml_structs.close()

# -----------------------------------------------------------------------------

required_attribute_parsers = []

xml_code = open('lib/xml/_gen_structs_validate.c', "w")

xml_code.write("#include <xml.h>\n")
xml_code.write("#include <stdint.h>\n")
xml_code.write("#include <stdio.h>\n")
xml_code.write("#include <unistd.h>\n\n")

for s in structs.values():
	has_children = (len(s.list_1child) + len(s.list_nchild) > 0)
	xml_code.write("int _validate_node_%s(struct xmlnode *node) {\n" % s.name)

	if len(s.list_attrs) > 0:
		xml_code.write("  struct xmlattr *attr;\n")
		xml_code.write("  uint32_t ret = 0;\n")

	if has_children:
		xml_code.write("  struct xmlnode *child;\n  uint32_t i;\n")
		for c in s.list_1child:
			xml_code.write("  uint32_t child_count_%s = 0;\n" % c[0].name)
		for c in s.list_nchild:
			xml_code.write("  uint32_t child_count_%s = 0;\n" % c[0].name)

	for a in s.list_attrs:
		xml_code.write("  attr = node->attrs + %s;\n" % a.enum_id)
		if a.is_required:
			xml_code.write("  if (attr == NULL) { sprintf(xml_errormsg, \"required attribute %s.%s missing\"); return 1; }\n" % (s.name, a.name))
		xml_code.write("  if (attr && (attr->state == ATTRSTATE_UNPARSED))\n")
		xml_code.write("    ret |= _parse_attr_%s(node, attr);\n" % a.function_suffix)
		required_attribute_parsers.append("%s" % a.function_suffix)
		xml_code.write("  if (ret) { sprintf(xml_errormsg, \"parsing attribute %s.%s failed\"); return ret; }\n" % (s.name, a.name))

	if has_children:
		# make sure that node only contains child nodes which are permitted by DTD
		xml_code.write("  for (i = 0; i < node->num_children; i++) {\n")
		xml_code.write("    child = node->children + i;\n")
		for c in s.list_1child:
			xml_code.write("    if (child->node_type == %s) { child_count_%s++; continue; }\n" % (c[0].enum_id, c[0].name))
		for c in s.list_nchild:
			xml_code.write("    if (child->node_type == %s) { child_count_%s++; continue; }\n" % (c[0].enum_id, c[0].name))
		xml_code.write("    sprintf(xml_errormsg, \"%s: unexpected child element %%d\", child->node_type);\n" % s.name)
		xml_code.write("    return 2;\n")
		xml_code.write("  }\n")

		# make sure that the quantity of children matches the expectation
		for c in s.list_1child:
			if c[1]:
				xml_code.write("  if (child_count_%s != 1) { sprintf(xml_errormsg, \"%s: expected one %s child element, found %%d\", child_count_%s); return 3; }\n" % (c[0].name, s.name, c[0].name, c[0].name))
			else:
				xml_code.write("  if (child_count_%s > 1) { sprintf(xml_errormsg, \"%s: expected at most one %s child element, found %%d\", child_count_%s); return 3; }\n" % (c[0].name, s.name, c[0].name, c[0].name))
		for c in s.list_nchild:
			if c[1] > 0:
				xml_code.write("  if (child_count_%s < %d) { sprintf(xml_errormsg, \"%s: expected at least %d %s child elements, found %%d\", child_count_%s); return 4; }\n" % (c[0].name, c[1], s.name, c[1], c[0].name, c[0].name))
			if c[2]:
				xml_code.write("  if (child_count_%s > %d) { sprintf(xml_errormsg, \"%s: expected at most %d %s child elements, found %%d\", child_count_%s); return 4; }\n" % (c[0].name, c[2], s.name, c[2], c[0].name, c[0].name))
	else:
		xml_code.write("  if (node->num_children > 0) { sprintf(xml_errormsg, \"%s: no children expected\"); return 2; }\n" % s.name)

	xml_code.write("  return 0;\n}\n\n")

xml_code.write("int validate_node_one_generated(struct xmlnode *node) {\n")
xml_code.write("  switch (node->node_type) {\n")
for s in structs.values():
	xml_code.write("  case %s: return _validate_node_%s(node);\n" % ( s.enum_id, s.name))
xml_code.write("  default: return 1;\n  }\n}\n")

xml_code.close()

# -----------------------------------------------------------------------------

xml_code = open('lib/xml/_gen_structs_write.c', "w")

xml_code.write("#include <xml.h>\n")
xml_code.write("#include <stdint.h>\n")
xml_code.write("#include <stdio.h>\n")
xml_code.write("#include <unistd.h>\n\n")
xml_code.write("#include <string.h>\n")

for s in structs.values():
	xml_code.write("void _write_node_%s(int fd, struct xmlnode *node, unsigned int indent_level) {\n" % s.name)
	xml_code.write("  uint32_t i;\n")
	xml_code.write("  for (i = 0; i < indent_level; i++) { write(fd, \"  \", 2); }\n")
	xml_code.write("  write(fd, \"<%s\", %d);\n" % (s.name, 1 + len(s.name)))
	for a in s.list_attrs:
		xml_code.write("  if (node->attrs[%s].state == ATTRSTATE_MODIFIED)\n    _regenerate_attr_%s(node, node->attrs + %s);\n" % (a.enum_id, a.function_suffix, a.enum_id))
		xml_code.write("  if (node->attrs[%s].state == ATTRSTATE_CLEAN) {\n" % (a.enum_id))
		xml_code.write("    write(fd, \" " + a.name + "=\\\"\", %d);\n" % (1 + len(a.name) + 2))
		xml_code.write("    write(fd, node->attrs[%s].str_value, strlen(node->attrs[%s].str_value));\n" % (a.enum_id, a.enum_id))
		xml_code.write("    write(fd, \"\\\"\", 1);\n  }\n")
	xml_code.write("  if (node->num_children == 0) {\n    write(fd, \"/>\\n\", 3);\n    return;\n  }\n")
	xml_code.write("  write(fd, \">\\n\", 2);\n")
	xml_code.write("  for (i = 0; i < node->num_children; i++) {\n")
	xml_code.write("    write_node_one_generated(fd, node->children + i, indent_level + 1);\n")
	xml_code.write("  }\n")
	xml_code.write("  for (i = 0; i < indent_level; i++) { write(fd, \"  \", 2); }\n")
	xml_code.write("  write(fd, \"</%s>\\n\", %d);\n" % (s.name, 2 + len(s.name) + 2))
	xml_code.write("}\n\n")

xml_code.write("void write_node_one_generated(int fd, struct xmlnode *node, unsigned int indent_level) {\n")
xml_code.write("  switch (node->node_type) {\n")
for s in structs.values():
	xml_code.write("  case %s: _write_node_%s(fd, node, indent_level); return;\n" % ( s.enum_id, s.name))
xml_code.write("  }\n}\n")

xml_code.close()

# -----------------------------------------------------------------------------

xml_hdr_structs.write("struct xmlnode;\nstruct xmlattr;\n")

for s in structs.values():
	xml_hdr_structs.write("int _validate_node_%s(struct xmlnode *);\n" % s.name)
	xml_hdr_structs.write("void _write_node_%s(int, struct xmlnode *, unsigned int);\n" % s.name)

for i, rap in enumerate(required_attribute_parsers):
	if rap in required_attribute_parsers[0:i]:
		continue
	xml_hdr_structs.write("extern int _parse_attr_%s(struct xmlnode *, struct xmlattr *);\n" % rap)
	xml_hdr_structs.write("extern void _regenerate_attr_%s(struct xmlnode *, struct xmlattr *);\n" % rap)
## xml_hdr_structs.write("extern int _parse_attr_idref(struct xmlnode *, struct xmlattr *, uint32_t);\n")
## xml_hdr_structs.write("extern void _regenerate_attr_idref(struct xmlnode *, struct xmlattr *);\n")

xml_hdr_structs.close()

# -----------------------------------------------------------------------------

dtd_spec = open('include/dtd', "w")

for s in structs.values():
	num_children = len(s.list_1child) + len(s.list_nchild)
	num_attrs = len(s.list_attrs)

	dtd_spec.write("<!ELEMENT\t%s\t\t" % s.name)
	if num_children > 0:
		dtd_spec.write("(")
		i = 0
		for c in s.list_1child:
			i = i+1
			dtd_spec.write("%s%c" % (c[0].name, '|' if (i < num_children) else ')'))
		for c in s.list_nchild:
			i = i+1
			dtd_spec.write("%s%c" % (c[0].name, '|' if (i < num_children) else ')'))
		dtd_spec.write("*")
	else:
		dtd_spec.write("EMPTY")
	dtd_spec.write(">\n")

	if len(s.list_attrs) > 0:
		dtd_spec.write("<!ATTLIST\t%s\n" % s.name)
		i = 0
		for a in s.list_attrs:
			i = i + 1
			dtd_spec.write("\t\t%s\t\t%s\t%s%s" % (a.name, a.dtd_type, a.dtd_req,
				"\n" if (i < num_attrs) else ">\n"))
	dtd_spec.write("\n")

dtd_spec.close()

# -----------------------------------------------------------------------------
