/* Python class allowing users to build and install objfiles.

   Copyright (C) 2013-2023 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include "defs.h"
#include "python-internal.h"
#include "quick-symbol.h"
#include "objfiles.h"
#include "minsyms.h"
#include "buildsym.h"
#include "observable.h"
#include <string>
#include <unordered_map>
#include <type_traits>
#include <optional>

/* This module relies on symbols being trivially copyable. */
static_assert (std::is_trivially_copyable_v<struct symbol>);

/* Interface to be implemented for symbol types supported by this interface. */
class symbol_def
{
public:
  virtual void register_msymbol (const std::string& name, 
                                 struct objfile* objfile,
                                 minimal_symbol_reader& reader) const = 0;
  virtual void register_symbol (const std::string& name, 
                                struct objfile* objfile,
                                buildsym_compunit& builder) const = 0;
};

/* Shorthand for a unique_ptr to a symbol. */
typedef std::unique_ptr<symbol_def> symbol_def_up;

struct objfile_builder_object
{
  PyObject_HEAD

  /* Indicates whether the objfile has already been built and added to the
   * current context. We enforce that objfiles can't be installed twice. */
  bool installed;

  /* The symbols that will be added to new newly built objfile. */
  std::unordered_map<std::string, symbol_def_up> symbols;

  /* The name given to this objfile. */
  std::string name;

  /* Adds a symbol definition with the given name. */
  bool add_symbol_def (std::string name, symbol_def_up&& symbol_def)
  {
    return std::get<1> (symbols.insert ({name, std::move (symbol_def)}));
  }
};

/* Convenience function that performs a checked coversion from a PyObject to
 * a objfile_builder_object structure pointer. */
inline static struct objfile_builder_object *
validate_objfile_builder_object (PyObject *self);

/* Constructs a new objfile from an objfile_builder. */
static struct objfile *
build_new_objfile (const objfile_builder_object& builder)
{
  gdb_assert (!builder.installed);

  auto of = objfile::make (nullptr, builder.name.c_str (), OBJF_READNOW, nullptr);

  /* Setup object file sections. */
  of->sections_start = OBSTACK_CALLOC (&of->objfile_obstack,
                                       4,
                                       struct obj_section);
  of->sections_end = of->sections_start + 4;

  const auto init_section = [&](struct obj_section* sec)
    {
      sec->objfile = of;
      sec->ovly_mapped = false;
      
      /* We're not being backed by BFD. So we have no real section data to speak 
      * of, but, because specifying sections requires BFD structures, we have to
      * play a little game of predend. */
      auto bfd = obstack_new<bfd_section>(&of->objfile_obstack);
      bfd->vma = 0;
      bfd->size = 0;
      sec->the_bfd_section = bfd;
    };
  init_section (&of->sections_start[0]);
  init_section (&of->sections_start[1]);
  init_section (&of->sections_start[2]);
  init_section (&of->sections_start[4]);

  of->sect_index_text = 0;
  of->sect_index_data = 1;
  of->sect_index_rodata = 2;
  of->sect_index_bss = 3;

  /* Construct the minimal symbols. */
  minimal_symbol_reader msym (of);
  for (const auto& [name, symbol] : builder.symbols)
      symbol->register_msymbol (name, of, msym);
  msym.install ();

  /* Construct the full symbols. */
  buildsym_compunit fsym (of, builder.name.c_str (), "", language_c, 0);
  for (const auto& [name, symbol] : builder.symbols)
    symbol->register_symbol (name, of, fsym);
  fsym.end_compunit_symtab (0);

  /* Notify the rest of GDB this objfile has been created. */
  gdb::observers::new_objfile.notify (of);
}

/* Implementation of the quick symbol functions used by the objfiles created 
 * using this interface. Turns out we have our work cut out for us here, as we
 * can get something that works by effectively just using no-ops, and the rest
 * of the code will fall back to using just the minimal and full symbol data. It
 * is important to note, though, that this only works because we're marking our 
 * objfile with `OBJF_READNOW`. */
class runtime_objfile : public quick_symbol_functions
{
  virtual bool has_symbols (struct objfile*) override
  {
    return false;
  }

  virtual void dump (struct objfile *objfile) override
  {
  }

  virtual void expand_matching_symbols
    (struct objfile *,
     const lookup_name_info &lookup_name,
     domain_enum domain,
     int global,
     symbol_compare_ftype *ordered_compare) override
  {
  }

  virtual bool expand_symtabs_matching
    (struct objfile *objfile,
     gdb::function_view<expand_symtabs_file_matcher_ftype> file_matcher,
     const lookup_name_info *lookup_name,
     gdb::function_view<expand_symtabs_symbol_matcher_ftype> symbol_matcher,
     gdb::function_view<expand_symtabs_exp_notify_ftype> expansion_notify,
     block_search_flags search_flags,
     domain_enum domain,
     enum search_domain kind) override
  {
    return true;
  }
};


/* Create a new symbol alocated in the given objfile. */

static struct symbol *
new_symbol
  (struct objfile *objfile,
   const char *name,
   enum language language,
   enum domain_enum domain,
   enum address_class aclass,
   short section_index)
{
  auto symbol = new (&objfile->objfile_obstack) struct symbol ();
  OBJSTAT (objfile, n_syms++);

  symbol->set_language (language, &objfile->objfile_obstack);
  symbol->compute_and_set_names (gdb::string_view (name), true, 
                                 objfile->per_bfd);

  symbol->set_is_objfile_owned (true);
  symbol->set_section_index (section_index);
  symbol->set_domain (domain);
  symbol->set_aclass_index (aclass);

  return symbol;
}

/* Parses a language from a string (coming from Python) into a language 
 * variant. */

static enum language
parse_language (const char *language)
{
  if (strcmp (language, "c") == 0)
    return language_c;
  else if (strcmp (language, "objc") == 0)
    return language_objc;
  else if (strcmp (language, "cplus") == 0)
    return language_cplus;
  else if (strcmp (language, "d") == 0)
    return language_d;
  else if (strcmp (language, "go") == 0)
    return language_go;
  else if (strcmp (language, "fortran") == 0)
    return language_fortran;
  else if (strcmp (language, "m2") == 0)
    return language_m2;
  else if (strcmp (language, "asm") == 0)
    return language_asm;
  else if (strcmp (language, "pascal") == 0)
    return language_pascal;
  else if (strcmp (language, "opencl") == 0)
    return language_opencl;
  else if (strcmp (language, "rust") == 0)
    return language_rust;
  else if (strcmp (language, "ada") == 0)
    return language_ada;
  else
    return language_unknown;
}

/* Registers symbols added with add_label_symbol. */
class typedef_symbol_def : public symbol_def
{
public:
  struct type* type;
  enum language language;

  virtual void register_msymbol (const std::string& name,
                                 struct objfile *objfile,
                                 minimal_symbol_reader& reader) const override
  {
  }

  virtual void register_symbol (const std::string& name,
                                struct objfile *objfile,
                                buildsym_compunit& builder) const override
  {
    auto symbol = new_symbol (objfile, name.c_str (), language, LABEL_DOMAIN,
                              LOC_TYPEDEF, objfile->sect_index_text);

    symbol->set_type (type);

    add_symbol_to_list (symbol, builder.get_file_symbols ());
  }
};

/* Adds a type (LOC_TYPEDEF) symbol to a given objfile. */
static PyObject *
objbdpy_add_type_symbol (PyObject *self, PyObject *args, PyObject *kw)
{
  static const char *format = "sO|s";
  static const char *keywords[] =
    {
      "name", "type", "language",NULL
    };

  PyObject *type_object;
  const char *name;
  const char *language_name = nullptr;

  if (!gdb_PyArg_ParseTupleAndKeywords (args, kw, format, keywords, &name,
                                        &type_object, &language_name))
    return nullptr;

  auto builder = validate_objfile_builder_object (self);
  if (builder == nullptr)
    return nullptr;

  struct type *type = type_object_to_type (type_object);
  if (type == nullptr)
    return nullptr;

  if (language_name == nullptr)
    language_name = "auto";
  enum language language = parse_language (language_name);
  if (language == language_unknown)
    {
      PyErr_SetString (PyExc_ValueError, "invalid language name");
      return nullptr;
    }

  auto def = std::make_unique<typedef_symbol_def> ();
  def->type = type;
  def->language = language;

  builder->add_symbol_def (name, std::move (def));

  Py_RETURN_NONE;
}


/* Registers symbols added with add_label_symbol. */
class label_symbol_def : public symbol_def
{
public:
  CORE_ADDR address;
  enum language language;

  virtual void register_msymbol (const std::string& name,
                                 struct objfile *objfile,
                                 minimal_symbol_reader& reader) const override
  {
    reader.record (name.c_str (), 
                   unrelocated_addr (address), 
                   minimal_symbol_type::mst_text);
  }

  virtual void register_symbol (const std::string& name,
                                struct objfile *objfile,
                                buildsym_compunit& builder) const override
  {
    auto symbol = new_symbol (objfile, name.c_str (), language, LABEL_DOMAIN,
                              LOC_LABEL, objfile->sect_index_text);

    symbol->set_value_address (address);

    add_symbol_to_list (symbol, builder.get_file_symbols ());
  }
};

/* Adds a label (LOC_LABEL) symbol to a given objfile. */
static PyObject *
objbdpy_add_label_symbol (PyObject *self, PyObject *args, PyObject *kw)
{
  static const char *format = "sk|s";
  static const char *keywords[] =
    {
      "name", "address", "language",NULL
    };

  const char *name;
  CORE_ADDR address;
  const char *language_name = nullptr;

  if (!gdb_PyArg_ParseTupleAndKeywords (args, kw, format, keywords, &name,
                                        &address, &language_name))
    return nullptr;

  auto builder = validate_objfile_builder_object (self);
  if (builder == nullptr)
    return nullptr;

  if (language_name == nullptr)
    language_name = "auto";
  enum language language = parse_language (language_name);
  if (language == language_unknown)
    {
      PyErr_SetString (PyExc_ValueError, "invalid language name");
      return nullptr;
    }

  auto def = std::make_unique<label_symbol_def> ();
  def->address = address;
  def->language = language;

  builder->add_symbol_def (name, std::move (def));

  Py_RETURN_NONE;
}

/* Registers symbols added with add_static_symbol. */
class static_symbol_def : public symbol_def
{
public:
  CORE_ADDR address;
  enum language language;

  virtual void register_msymbol (const std::string& name,
                                 struct objfile *objfile,
                                 minimal_symbol_reader& reader) const override
  {
    reader.record (name.c_str (), 
                   unrelocated_addr (address), 
                   minimal_symbol_type::mst_bss);
  }

  virtual void register_symbol (const std::string& name,
                                struct objfile *objfile,
                                buildsym_compunit& builder) const override
  {
    auto symbol = new_symbol (objfile, name.c_str (), language, VAR_DOMAIN,
                              LOC_STATIC, objfile->sect_index_bss);

    symbol->set_value_address (address);

    add_symbol_to_list (symbol, builder.get_file_symbols ());
  }
};

/* Adds a static (LOC_STATIC) symbol to a given objfile. */
static PyObject *
objbdpy_add_static_symbol (PyObject *self, PyObject *args, PyObject *kw)
{
  static const char *format = "sk|s";
  static const char *keywords[] =
    {
      "name", "address", "language", NULL
    };

  const char *name;
  CORE_ADDR address;
  const char *language_name = nullptr;

  if (!gdb_PyArg_ParseTupleAndKeywords (args, kw, format, keywords, &name,
                                        &address, &language_name))
    return nullptr;

  auto builder = validate_objfile_builder_object (self);
  if (builder == nullptr)
    return nullptr;

  if (language_name == nullptr)
    language_name = "auto";
  enum language language = parse_language (language_name);
  if (language == language_unknown)
    {
      PyErr_SetString (PyExc_ValueError, "invalid language name");
      return nullptr;
    }

  auto def = std::make_unique<static_symbol_def> ();
  def->address = address;
  def->language = language;

  builder->add_symbol_def (name, std::move (def));

  Py_RETURN_NONE;
}

static PyMethodDef objfile_builder_object_methods[] =
{
  { "add_type_symbol", (PyCFunction) objbdpy_add_type_symbol,
    METH_VARARGS | METH_KEYWORDS,
    "add_separate_debug_file (file_name).\n\
Add FILE_NAME to the list of files containing debug info for the objfile." },
  { "add_label_symbol", (PyCFunction) objbdpy_add_label_symbol,
    METH_VARARGS | METH_KEYWORDS,
    "add_separate_debug_file (file_name).\n\
Add FILE_NAME to the list of files containing debug info for the objfile." },
  { "add_static_symbol", (PyCFunction) objbdpy_add_static_symbol,
    METH_VARARGS | METH_KEYWORDS,
    "add_separate_debug_file (file_name).\n\
Add FILE_NAME to the list of files containing debug info for the objfile." },
  { NULL }
};

PyTypeObject objfile_builder_object_type = {
  PyVarObject_HEAD_INIT (NULL, 0)
  "gdb.ObjfileBuilder",               /* tp_name */
  sizeof (objfile_builder_object),    /* tp_basicsize */
  0,                                  /* tp_itemsize */
  0,                                  /* tp_dealloc */
  0,                                  /* tp_print */
  0,                                  /* tp_getattr */
  0,                                  /* tp_setattr */
  0,                                  /* tp_compare */
  0,                                  /* tp_repr */
  0,                                  /* tp_as_number */
  0,                                  /* tp_as_sequence */
  0,                                  /* tp_as_mapping */
  0,                                  /* tp_hash  */
  0,                                  /* tp_call */
  0,                                  /* tp_str */
  0,                                  /* tp_getattro */
  0,                                  /* tp_setattro */
  0,                                  /* tp_as_buffer */
  Py_TPFLAGS_DEFAULT,                 /* tp_flags */
  "GDB object file builder",          /* tp_doc */
  0,                                  /* tp_traverse */
  0,                                  /* tp_clear */
  0,                                  /* tp_richcompare */
  0,                                  /* tp_weaklistoffset */
  0,                                  /* tp_iter */
  0,                                  /* tp_iternext */
  objfile_builder_object_methods,     /* tp_methods */
  0,                                  /* tp_members */
  0,                                  /* tp_getset */
  0,                                  /* tp_base */
  0,                                  /* tp_dict */
  0,                                  /* tp_descr_get */
  0,                                  /* tp_descr_set */
  0,                                  /* tp_dictoffset */
  0,                                  /* tp_init */
  0,                                  /* tp_alloc */
};

inline static struct objfile_builder_object *
validate_objfile_builder_object (PyObject *self)
{
  if (!PyObject_TypeCheck (self, &objfile_builder_object_type))
    return nullptr;
  return (struct objfile_builder_object*) self;
}

