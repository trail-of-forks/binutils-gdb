/* Functionality for creating new types accessible from python.

   Copyright (C) 2008-2023 Free Software Foundation, Inc.

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
#include "gdbtypes.h"
#include "floatformat.h"
#include "objfiles.h"
#include "gdbsupport/gdb_obstack.h"


/* An abstraction covering the objects types that can own a type object. */

class type_storage_owner
{
public:
  /* Creates a new type owner from the given python object. If the object is
   * of a type that is not supported, the newly created instance will be
   * marked as invalid and nothing should be done with it. */

  type_storage_owner (PyObject *owner)
  {
    if (gdbpy_is_architecture (owner))
      {
	this->kind = owner_kind::arch;
	this->owner.arch = arch_object_to_gdbarch (owner);
	return;
      }

    this->kind = owner_kind::objfile;
    this->owner.objfile = objfile_object_to_objfile (owner);
    if (this->owner.objfile != nullptr)
	return;

    this->kind = owner_kind::none;
    PyErr_SetString(PyExc_TypeError, "unsupported owner type");
  }

  /* Whether the owner is valid. An owner may not be valid if the type that
   * was used to create it is not known. Operations must only be done on valid
   * instances of this class. */

  bool valid ()
  {
    return this->kind != owner_kind::none;
  }

  /* Returns a type allocator that allocates on this owner. */

  type_allocator allocator ()
  {
    gdb_assert (this->valid ());

    if (this->kind == owner_kind::arch)
      return type_allocator (this->owner.arch);
    else if (this->kind == owner_kind::objfile)
      return type_allocator (this->owner.objfile);

    /* Should never be reached, but it's better to fail in a safe way than try
     * to instance the allocator with arbitraty parameters here. */
    abort ();
  }

  /* Get a reference to the owner's obstack. */

  obstack *get_obstack ()
  {
    gdb_assert (this->valid ());

    if (this->kind == owner_kind::arch)
	return gdbarch_obstack (this->owner.arch);
    else if (this->kind == owner_kind::objfile)
	return &this->owner.objfile->objfile_obstack;

    return nullptr;
  }

  /* Get a reference to the owner's architecture. */

  struct gdbarch *get_arch ()
  {
    gdb_assert (this->valid ());

    if (this->kind == owner_kind::arch)
	return this->owner.arch;
    else if (this->kind == owner_kind::objfile)
	return this->owner.objfile->arch ();

    return nullptr;
  }

  /* Copy a null-terminated string to the owner's obstack. */

  const char *copy_string (const char *py_str)
  {
    gdb_assert (this->valid ());

    unsigned int len = strlen (py_str);
    return obstack_strndup (this->get_obstack (), py_str, len);
  }



private:
  enum class owner_kind { arch, objfile, none };

  owner_kind kind = owner_kind::none;
  union {
    struct gdbarch *arch;
    struct objfile *objfile;
  } owner;
};

/* Creates a new type and returns a new gdb.Type associated with it. */

PyObject *
gdbpy_init_type (PyObject *self, PyObject *args, PyObject *kw)
{
  static const char *keywords[] = { "owner", "type_code", "bit_size", "name",
				    NULL };
  PyObject *owner_object;
  enum type_code code;
  int bit_length;
  const char *py_name;

  if (!gdb_PyArg_ParseTupleAndKeywords (args, kw, "Oiis", keywords, &owner_object,
					&code, &bit_length, &py_name))
    return nullptr;

  type_storage_owner owner (owner_object);
  if (!owner.valid ())
    return nullptr;

  const char *name = owner.copy_string (py_name);
  struct type *type;
  try
    {
      type_allocator allocator = owner.allocator ();
      type = allocator.new_type (code, bit_length, name);
      gdb_assert (type != nullptr);
    }
  catch (gdb_exception_error& ex)
    {
      GDB_PY_HANDLE_EXCEPTION (ex);
    }

  return type_to_type_object (type);
}

/* Creates a new integer type and returns a new gdb.Type associated with it. */

PyObject *
gdbpy_init_integer_type (PyObject *self, PyObject *args, PyObject *kw)
{
  static const char *keywords[] = { "owner", "bit_size", "unsigned", "name",
				    NULL };
  PyObject *owner_object;
  int bit_size;
  int unsigned_p;
  const char *py_name;

  if (!gdb_PyArg_ParseTupleAndKeywords (args, kw, "Oips", keywords,
					&owner_object, &bit_size, &unsigned_p,
					&py_name))
    return nullptr;

  type_storage_owner owner (owner_object);
  if (!owner.valid ())
    return nullptr;

  const char *name = owner.copy_string (py_name);
  struct type *type;
  try
    {
      type_allocator allocator = owner.allocator ();
      type = init_integer_type (allocator, bit_size, unsigned_p, name);
      gdb_assert (type != nullptr);
    }
  catch (gdb_exception_error& ex)
    {
      GDB_PY_HANDLE_EXCEPTION (ex);
    }

  return type_to_type_object(type);
}

/* Creates a new character type and returns a new gdb.Type associated
 * with it. */

PyObject *
gdbpy_init_character_type (PyObject *self, PyObject *args, PyObject *kw)
{
  static const char *keywords[] = { "owner", "bit_size", "unsigned", "name",
				    NULL };
  PyObject *owner_object;
  int bit_size;
  int unsigned_p;
  const char *py_name;

  if (!gdb_PyArg_ParseTupleAndKeywords (args, kw, "Oips", keywords,
					&owner_object, &bit_size, &unsigned_p,
					&py_name))
    return nullptr;

  type_storage_owner owner (owner_object);
  if (!owner.valid ())
    return nullptr;

  const char *name = owner.copy_string (py_name);
  struct type *type;
  try
    {
      type_allocator allocator = owner.allocator ();
      type = init_character_type (allocator, bit_size, unsigned_p, name);
      gdb_assert (type != nullptr);
    }
  catch (gdb_exception_error& ex)
    {
      GDB_PY_HANDLE_EXCEPTION (ex);
    }

  return type_to_type_object (type);
}

/* Creates a new boolean type and returns a new gdb.Type associated with it. */

PyObject *
gdbpy_init_boolean_type (PyObject *self, PyObject *args, PyObject *kw)
{
  static const char *keywords[] = { "owner", "bit_size", "unsigned", "name",
				    NULL };
  PyObject *owner_object;
  int bit_size;
  int unsigned_p;
  const char *py_name;

  if (!gdb_PyArg_ParseTupleAndKeywords (args, kw, "Oips", keywords,
					&owner_object, &bit_size, &unsigned_p,
					&py_name))
    return nullptr;

  type_storage_owner owner (owner_object);
  if (!owner.valid ())
    return nullptr;

  const char *name = owner.copy_string (py_name);
  struct type *type;
  try
    {
      type_allocator allocator = owner.allocator ();
      type = init_boolean_type (allocator, bit_size, unsigned_p, name);
      gdb_assert (type != nullptr);
    }
  catch (gdb_exception_error& ex)
    {
      GDB_PY_HANDLE_EXCEPTION (ex);
    }

  return type_to_type_object (type);
}

/* Creates a new float type and returns a new gdb.Type associated with it. */

PyObject *
gdbpy_init_float_type (PyObject *self, PyObject *args, PyObject *kw)
{
  static const char *keywords[] = { "owner", "format", "name", NULL };
  PyObject *owner_object, *float_format_object;
  const char *py_name;

  if (!gdb_PyArg_ParseTupleAndKeywords (args, kw, "OOs", keywords, &owner_object,
					&float_format_object, &py_name))
    return nullptr;

  type_storage_owner owner (owner_object);
  if (!owner.valid ())
    return nullptr;

  struct floatformat *local_ff = float_format_object_as_float_format
    (float_format_object);
  if (local_ff == nullptr)
    return nullptr;

  /* Persist a copy of the format in the objfile's obstack. This guarantees
   * that the format won't outlive the type being created from it and that
   * changes made to the object used to create this type will not affect it
   * after creation. */
  auto ff = OBSTACK_CALLOC (owner.get_obstack (), 1, struct floatformat);
  memcpy (ff, local_ff, sizeof (struct floatformat));

  /* We only support creating float types in the architecture's endianness, so
   * make sure init_float_type sees the float format structure we need it to.
   */
  enum bfd_endian endianness = gdbarch_byte_order (owner.get_arch ());
  gdb_assert (endianness < BFD_ENDIAN_UNKNOWN);

  const struct floatformat *per_endian[2] = { nullptr, nullptr };
  per_endian[endianness] = ff;

  const char *name = owner.copy_string (py_name);
  struct type *type;
  try
    {
      type_allocator allocator = owner.allocator ();
      type = init_float_type (allocator, -1, name, per_endian, endianness);
      gdb_assert (type != nullptr);
    }
  catch (gdb_exception_error& ex)
    {
      GDB_PY_HANDLE_EXCEPTION (ex);
    }

  return type_to_type_object (type);
}

/* Creates a new decimal float type and returns a new gdb.Type
 * associated with it. */

PyObject *
gdbpy_init_decfloat_type (PyObject *self, PyObject *args, PyObject *kw)
{
  static const char *keywords[] = { "owner", "bit_size", "name", NULL };
  PyObject *owner_object;
  int bit_length;
  const char *py_name;

  if (!gdb_PyArg_ParseTupleAndKeywords (args, kw, "Ois", keywords, &owner_object,
					&bit_length, &py_name))
    return nullptr;

  type_storage_owner owner (owner_object);
  if (!owner.valid ())
    return nullptr;

  const char *name = owner.copy_string (py_name);
  struct type *type;
  try
    {
      type_allocator allocator = owner.allocator ();
      type = init_decfloat_type (allocator, bit_length, name);
      gdb_assert (type != nullptr);
    }
  catch (gdb_exception_error& ex)
    {
      GDB_PY_HANDLE_EXCEPTION (ex);
    }

  return type_to_type_object (type);
}

/* Returns whether a given type can be used to create a complex type. */

PyObject *
gdbpy_can_create_complex_type (PyObject *self, PyObject *args, PyObject *kw)
{
  static const char *keywords[] = { "type", NULL };
  PyObject *type_object;

  if (!gdb_PyArg_ParseTupleAndKeywords (args, kw, "O", keywords,
					&type_object))
    return nullptr;

  struct type *type = type_object_to_type (type_object);
  if (type == nullptr)
    return nullptr;

  bool can_create_complex = false;
  try
    {
      can_create_complex = can_create_complex_type (type);
    }
  catch (gdb_exception_error& ex)
    {
      GDB_PY_HANDLE_EXCEPTION (ex);
    }

  if (can_create_complex)
    Py_RETURN_TRUE;
  else
    Py_RETURN_FALSE;
}

/* Creates a new complex type and returns a new gdb.Type associated with it. */

PyObject *
gdbpy_init_complex_type (PyObject *self, PyObject *args, PyObject *kw)
{
  static const char *keywords[] = { "type", "name", NULL };
  PyObject *type_object;
  const char *py_name;

  if (!gdb_PyArg_ParseTupleAndKeywords (args, kw, "Os", keywords, &type_object,
					&py_name))
    return nullptr;

  struct type *type = type_object_to_type (type_object);
  if (type == nullptr)
    return nullptr;

  obstack *obstack;
  if (type->is_objfile_owned ())
    obstack = &type->objfile_owner ()->objfile_obstack;
  else
    obstack = gdbarch_obstack (type->arch_owner ());

  unsigned int len = strlen (py_name);
  const char *name = obstack_strndup (obstack,
				      py_name,
				      len);
  struct type *complex_type;
  try
    {
      complex_type = init_complex_type (name, type);
      gdb_assert (complex_type != nullptr);
    }
  catch (gdb_exception_error& ex)
    {
      GDB_PY_HANDLE_EXCEPTION (ex);
    }

  return type_to_type_object (complex_type);
}

/* Creates a new pointer type and returns a new gdb.Type associated with it. */

PyObject *
gdbpy_init_pointer_type (PyObject *self, PyObject *args, PyObject *kw)
{
  static const char *keywords[] = { "owner", "target", "bit_size", "name",
				    NULL };
  PyObject *owner_object, *type_object;
  int bit_length;
  const char *py_name;

  if (!gdb_PyArg_ParseTupleAndKeywords (args, kw, "OOis", keywords,
					&owner_object, &type_object,
					&bit_length, &py_name))
    return nullptr;

  struct type *type = type_object_to_type (type_object);
  if (type == nullptr)
    return nullptr;

  type_storage_owner owner (owner_object);
  if (!owner.valid ())
    return nullptr;

  const char *name = owner.copy_string (py_name);
  struct type *pointer_type = nullptr;
  try
    {
      type_allocator allocator = owner.allocator ();
      pointer_type = init_pointer_type (allocator, bit_length, name, type);
      gdb_assert (type != nullptr);
    }
  catch (gdb_exception_error& ex)
    {
      GDB_PY_HANDLE_EXCEPTION (ex);
    }

  return type_to_type_object (pointer_type);
}

/* Creates a new fixed point type and returns a new gdb.Type associated
 * with it. */

PyObject *
gdbpy_init_fixed_point_type (PyObject *self, PyObject *args, PyObject *kw)
{
  static const char *keywords[] = { "owner", "bit_size", "unsigned", "name",
				    NULL };
  PyObject *objfile_object;
  int bit_length;
  int unsigned_p;
  const char* py_name;

  if (!gdb_PyArg_ParseTupleAndKeywords (args, kw, "Oips", keywords,
					&objfile_object, &bit_length,
					&unsigned_p, &py_name))
    return nullptr;

  struct objfile *objfile = objfile_object_to_objfile (objfile_object);
  if (objfile == nullptr)
    return nullptr;

  unsigned int len = strlen (py_name);
  const char *name = obstack_strndup (&objfile->objfile_obstack, py_name, len);
  struct type *type;
  try
    {
      type = init_fixed_point_type (objfile, bit_length, unsigned_p, name);
      gdb_assert (type != nullptr);
    }
  catch (gdb_exception_error& ex)
    {
      GDB_PY_HANDLE_EXCEPTION (ex);
    }

  return type_to_type_object (type);
}

