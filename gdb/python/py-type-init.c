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


/* Copies a null-terminated string into an objfile's obstack. */

static const char *
copy_string (struct objfile *objfile, const char *py_str)
{
  unsigned int len = strlen (py_str);
  return obstack_strndup (&objfile->per_bfd->storage_obstack,
                          py_str, len);
}

/* Creates a new type and returns a new gdb.Type associated with it. */

PyObject *
gdbpy_init_type (PyObject *self, PyObject *args)
{
  PyObject *objfile_object;
  enum type_code code;
  int bit_length;
  const char *py_name;

  if(!PyArg_ParseTuple (args, "Oiis", &objfile_object, &code, 
                        &bit_length, &py_name))
    return nullptr;

  struct objfile* objfile = objfile_object_to_objfile (objfile_object);
  if (objfile == nullptr)
    return nullptr;

  const char *name = copy_string (objfile, py_name);
  struct type *type;
  try
    {
      type_allocator allocator (objfile);
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
gdbpy_init_integer_type (PyObject *self, PyObject *args)
{
  PyObject *objfile_object;
  int bit_size;
  int unsigned_p;
  const char *py_name;

  if (!PyArg_ParseTuple (args, "Oips", &objfile_object, &bit_size, 
                         &unsigned_p, &py_name))
    return nullptr;

  struct objfile *objfile = objfile_object_to_objfile (objfile_object);
  if (objfile == nullptr)
    return nullptr;

  const char *name = copy_string (objfile, py_name);
  struct type *type;
  try
    {
      type_allocator allocator (objfile);
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
gdbpy_init_character_type (PyObject *self, PyObject *args)
{

  PyObject *objfile_object;
  int bit_size;
  int unsigned_p;
  const char *py_name;

  if (!PyArg_ParseTuple (args, "Oips", &objfile_object, &bit_size, 
                         &unsigned_p, &py_name))
    return nullptr;

  struct objfile *objfile = objfile_object_to_objfile (objfile_object);
  if (objfile == nullptr)
    return nullptr;

  const char *name = copy_string (objfile, py_name);
  struct type *type;
  try
    {
      type_allocator allocator (objfile);
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
gdbpy_init_boolean_type (PyObject *self, PyObject *args)
{

  PyObject *objfile_object;
  int bit_size;
  int unsigned_p;
  const char *py_name;

  if (!PyArg_ParseTuple (args, "Oips", &objfile_object, &bit_size, 
                         &unsigned_p, &py_name))
    return nullptr;

  struct objfile *objfile = objfile_object_to_objfile (objfile_object);
  if (objfile == nullptr)
    return nullptr;

  const char *name = copy_string (objfile, py_name);
  struct type *type;
  try
    {
      type_allocator allocator (objfile);
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
gdbpy_init_float_type (PyObject *self, PyObject *args)
{
  PyObject *objfile_object, *float_format_object;
  const char *py_name;

  if (!PyArg_ParseTuple (args, "OOs", &objfile_object, 
                         &float_format_object, &py_name))
    return nullptr;

  struct objfile *objfile = objfile_object_to_objfile (objfile_object);
  if (objfile == nullptr)
    return nullptr;

  struct floatformat *local_ff = float_format_object_as_float_format 
    (float_format_object);
  if (local_ff == nullptr)
    return nullptr;

  /* Persist a copy of the format in the objfile's obstack. This guarantees that
   * the format won't outlive the type being created from it and that changes
   * made to the object used to create this type will not affect it after
   * creation. */
  auto ff = OBSTACK_CALLOC
    (&objfile->objfile_obstack,
     1,
     struct floatformat);
  memcpy (ff, local_ff, sizeof (struct floatformat));

  /* We only support creating float types in the architecture's endianness, so
   * make sure init_float_type sees the float format structure we need it to. */
  enum bfd_endian endianness = gdbarch_byte_order (objfile->arch());
  gdb_assert (endianness < BFD_ENDIAN_UNKNOWN);

  const struct floatformat *per_endian[2] = { nullptr, nullptr };
  per_endian[endianness] = ff;

  const char *name = copy_string (objfile, py_name);
  struct type *type;
  try
    {
      type_allocator allocator (objfile);
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
gdbpy_init_decfloat_type (PyObject *self, PyObject *args)
{
  PyObject *objfile_object;
  int bit_length;
  const char *py_name;

  if (!PyArg_ParseTuple (args, "Ois", &objfile_object, &bit_length, &py_name))
    return nullptr;

  struct objfile *objfile = objfile_object_to_objfile (objfile_object);
  if (objfile == nullptr)
    return nullptr;

  const char *name = copy_string (objfile, py_name);
  struct type *type;
  try
    {
      type_allocator allocator (objfile);
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
gdbpy_can_create_complex_type (PyObject *self, PyObject *args)
{

  PyObject *type_object;

  if (!PyArg_ParseTuple (args, "O", &type_object))
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
gdbpy_init_complex_type (PyObject *self, PyObject *args)
{

  PyObject *type_object;
  const char *py_name;

  if (!PyArg_ParseTuple (args, "Os", &type_object, &py_name))
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
gdbpy_init_pointer_type (PyObject *self, PyObject *args)
{
  PyObject *objfile_object, *type_object;
  int bit_length;
  const char *py_name;

  if (!PyArg_ParseTuple (args, "OOis", &objfile_object, &type_object, 
                         &bit_length, &py_name))
    return nullptr;

  struct objfile *objfile = objfile_object_to_objfile (objfile_object);
  if (objfile == nullptr)
    return nullptr;

  struct type *type = type_object_to_type (type_object);
  if (type == nullptr)
    return nullptr;

  const char *name = copy_string (objfile, py_name);
  struct type *pointer_type = nullptr;
  try
    {
      type_allocator allocator (objfile);
      pointer_type = init_pointer_type (allocator, bit_length, 
                                        name, type);
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
gdbpy_init_fixed_point_type (PyObject *self, PyObject *args)
{

  PyObject *objfile_object;
  int bit_length;
  int unsigned_p;
  const char* py_name;

  if (!PyArg_ParseTuple (args, "Oips", &objfile_object, &bit_length, 
                         &unsigned_p, &py_name))
    return nullptr;

  struct objfile *objfile = objfile_object_to_objfile (objfile_object);
  if (objfile == nullptr)
    return nullptr;

  const char *name = copy_string (objfile, py_name);
  struct type *type;
  try
    {
      type = init_fixed_point_type (objfile, bit_length, unsigned_p, 
                                    name);
      gdb_assert (type != nullptr);
    }
  catch (gdb_exception_error& ex)
    {
      GDB_PY_HANDLE_EXCEPTION (ex);
    }

  return type_to_type_object (type);
}

