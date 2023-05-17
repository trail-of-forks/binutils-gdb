/* Accessibility of float format controls from inside the Python API

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
#include "floatformat.h"

/* Structure backing the float format Python interface. */

struct float_format_object
{
  PyObject_HEAD
  struct floatformat format;

  struct floatformat *float_format ()
  {
    return &this->format;
  }
};

/* Initializes the float format type and registers it with the Python interpreter. */

static int CPYCHECKER_NEGATIVE_RESULT_SETS_EXCEPTION
gdbpy_initialize_float_format (void)
{
  if (PyType_Ready (&float_format_object_type) < 0)
    return -1;

  if (gdb_pymodule_addobject (gdb_module, "FloatFormat",
                              (PyObject *) &float_format_object_type) < 0)
    return -1;

  return 0;
}

GDBPY_INITIALIZE_FILE (gdbpy_initialize_float_format);

#define INSTANCE_FIELD_GETTER(getter_name, field_name, field_type, field_conv) \
  static PyObject *                                                            \
  getter_name (PyObject *self, void *closure)                                  \
  {                                                                            \
    float_format_object *ff = (float_format_object*) self;                     \
    field_type value = ff->float_format ()->field_name;                        \
    return field_conv (value);                                                 \
  }

#define INSTANCE_FIELD_SETTER(getter_name, field_name, field_type, field_conv) \
  static int                                                                   \
  getter_name (PyObject *self, PyObject* value, void *closure)                 \
  {                                                                            \
    field_type native_value;                                                   \
    if (!field_conv (value, &native_value))                                    \
      return -1;                                                               \
    float_format_object *ff = (float_format_object*) self;                     \
    ff->float_format ()->field_name = native_value;                            \
    return 0;                                                                  \
  }

/* Converts from the intbit enum to a Python boolean. */

static PyObject *
intbit_to_py (enum floatformat_intbit intbit)
{
  gdb_assert 
    (intbit == floatformat_intbit_yes || 
     intbit == floatformat_intbit_no);

  if (intbit == floatformat_intbit_no)
    Py_RETURN_FALSE;
  else
    Py_RETURN_TRUE;
}

/* Converts from a Python boolean to the intbit enum. */

static bool
py_to_intbit (PyObject *object, enum floatformat_intbit *intbit)
{
  if (!PyObject_IsInstance (object, (PyObject*) &PyBool_Type))
    {
      PyErr_SetString (PyExc_TypeError, "intbit must be True or False");
      return false;
    }

  *intbit = PyObject_IsTrue (object) ? 
    floatformat_intbit_yes : floatformat_intbit_no;
  return true;
}

/* Converts from a Python integer to a unsigned integer. */

static bool
py_to_unsigned_int (PyObject *object, unsigned int *val)
{
  if (!PyObject_IsInstance (object, (PyObject*) &PyLong_Type))
    {
      PyErr_SetString (PyExc_TypeError, "value must be an integer");
      return false;
    }

  long native_val = PyLong_AsLong (object);
  if (native_val > (long) UINT_MAX)
    {
      PyErr_SetString (PyExc_ValueError, "value is too large");
      return false;
    }
  if (native_val < 0)
    {
      PyErr_SetString (PyExc_ValueError, 
                       "value must not be smaller than zero");
      return false;
    }

  *val = (unsigned int) native_val;
  return true;
}

/* Converts from a Python integer to a signed integer. */

static bool
py_to_int(PyObject *object, int *val)
{
  if(!PyObject_IsInstance(object, (PyObject*)&PyLong_Type))
    {
      PyErr_SetString(PyExc_TypeError, "value must be an integer");
      return false;
    }

  long native_val = PyLong_AsLong(object);
  if(native_val > (long)INT_MAX)
    {
      PyErr_SetString(PyExc_ValueError, "value is too large");
      return false;
    }

  *val = (int)native_val;
  return true;
}

INSTANCE_FIELD_GETTER (ffpy_get_totalsize, totalsize, 
                       unsigned int, PyLong_FromLong)
INSTANCE_FIELD_GETTER (ffpy_get_sign_start, sign_start, 
                       unsigned int, PyLong_FromLong)
INSTANCE_FIELD_GETTER (ffpy_get_exp_start, exp_start, 
                       unsigned int, PyLong_FromLong)
INSTANCE_FIELD_GETTER (ffpy_get_exp_len, exp_len, 
                       unsigned int, PyLong_FromLong)
INSTANCE_FIELD_GETTER (ffpy_get_exp_bias, exp_bias, int, PyLong_FromLong)
INSTANCE_FIELD_GETTER (ffpy_get_exp_nan, exp_nan, 
                       unsigned int, PyLong_FromLong)
INSTANCE_FIELD_GETTER (ffpy_get_man_start, man_start, 
                       unsigned int, PyLong_FromLong)
INSTANCE_FIELD_GETTER (ffpy_get_man_len, man_len, 
                       unsigned int, PyLong_FromLong)
INSTANCE_FIELD_GETTER (ffpy_get_intbit, intbit, 
                       enum floatformat_intbit, intbit_to_py)
INSTANCE_FIELD_GETTER (ffpy_get_name, name, 
                       const char *, PyUnicode_FromString)

INSTANCE_FIELD_SETTER (ffpy_set_totalsize, totalsize, 
                       unsigned int, py_to_unsigned_int)
INSTANCE_FIELD_SETTER (ffpy_set_sign_start, sign_start, 
                       unsigned int, py_to_unsigned_int)
INSTANCE_FIELD_SETTER (ffpy_set_exp_start, exp_start, 
                       unsigned int, py_to_unsigned_int)
INSTANCE_FIELD_SETTER (ffpy_set_exp_len, exp_len, 
                       unsigned int, py_to_unsigned_int)
INSTANCE_FIELD_SETTER (ffpy_set_exp_bias, exp_bias, int, py_to_int)
INSTANCE_FIELD_SETTER (ffpy_set_exp_nan, exp_nan, 
                       unsigned int, py_to_unsigned_int)
INSTANCE_FIELD_SETTER (ffpy_set_man_start, man_start,
                       unsigned int, py_to_unsigned_int)
INSTANCE_FIELD_SETTER (ffpy_set_man_len, man_len, 
                       unsigned int, py_to_unsigned_int)
INSTANCE_FIELD_SETTER (ffpy_set_intbit, intbit, 
                       enum floatformat_intbit, py_to_intbit)

/* Makes sure float formats created from Python always test as valid. */

static int
ffpy_always_valid (const struct floatformat *fmt ATTRIBUTE_UNUSED,
                   const void *from ATTRIBUTE_UNUSED)
{
  return 1;
}

/* Initializes new float format objects. */

static int
ffpy_init (PyObject *self,
           PyObject *args ATTRIBUTE_UNUSED,
           PyObject *kwds ATTRIBUTE_UNUSED)
{
  auto ff = (float_format_object*) self;
  ff->format = floatformat ();
  ff->float_format ()->name = "";
  ff->float_format ()->is_valid = ffpy_always_valid;
  return 0;
}

/* Retrieves a pointer to the underlying float format structure. */

struct floatformat *
float_format_object_as_float_format (PyObject *self)
{
  if (!PyObject_IsInstance (self, (PyObject*) &float_format_object_type))
    return nullptr;
  return ((float_format_object*) self)->float_format ();
}

static gdb_PyGetSetDef float_format_object_getset[] =
{
  { "totalsize", ffpy_get_totalsize, ffpy_set_totalsize,
    "The total size of the floating point number, in bits.", nullptr },
  { "sign_start", ffpy_get_sign_start, ffpy_set_sign_start,
    "The bit offset of the sign bit.", nullptr },
  { "exp_start", ffpy_get_exp_start, ffpy_set_exp_start,
    "The bit offset of the start of the exponent.", nullptr },
  { "exp_len", ffpy_get_exp_len, ffpy_set_exp_len,
    "The size of the exponent, in bits.", nullptr },
  { "exp_bias", ffpy_get_exp_bias, ffpy_set_exp_bias,
    "Bias added to a \"true\" exponent to form the biased exponent.", nullptr },
  { "exp_nan", ffpy_get_exp_nan, ffpy_set_exp_nan,
    "Exponent value which indicates NaN.", nullptr },
  { "man_start", ffpy_get_man_start, ffpy_set_man_start,
    "The bit offset of the start of the mantissa.", nullptr },
  { "man_len", ffpy_get_man_len, ffpy_set_man_len,
    "The size of the mantissa, in bits.", nullptr },
  { "intbit", ffpy_get_intbit, ffpy_set_intbit,
    "Is the integer bit explicit or implicit?", nullptr },
  { "name", ffpy_get_name, nullptr,
    "Internal name for debugging.", nullptr },
  { nullptr }
};

static PyMethodDef float_format_object_methods[] =
{
  { NULL }
};

static PyNumberMethods float_format_object_as_number = {
  nullptr,             /* nb_add */
  nullptr,             /* nb_subtract */
  nullptr,             /* nb_multiply */
  nullptr,             /* nb_remainder */
  nullptr,             /* nb_divmod */
  nullptr,             /* nb_power */
  nullptr,             /* nb_negative */
  nullptr,             /* nb_positive */
  nullptr,             /* nb_absolute */
  nullptr,             /* nb_nonzero */
  nullptr,             /* nb_invert */
  nullptr,             /* nb_lshift */
  nullptr,             /* nb_rshift */
  nullptr,             /* nb_and */
  nullptr,             /* nb_xor */
  nullptr,             /* nb_or */
  nullptr,             /* nb_int */
  nullptr,             /* reserved */
  nullptr,             /* nb_float */
};

PyTypeObject float_format_object_type =
{
  PyVarObject_HEAD_INIT (NULL, 0)
  "gdb.FloatFormat",              /*tp_name*/
  sizeof (float_format_object),   /*tp_basicsize*/
  0,                              /*tp_itemsize*/
  nullptr,                        /*tp_dealloc*/
  0,                              /*tp_print*/
  nullptr,                        /*tp_getattr*/
  nullptr,                        /*tp_setattr*/
  nullptr,                        /*tp_compare*/
  nullptr,                        /*tp_repr*/
  &float_format_object_as_number, /*tp_as_number*/
  nullptr,                        /*tp_as_sequence*/
  nullptr,                        /*tp_as_mapping*/
  nullptr,                        /*tp_hash */
  nullptr,                        /*tp_call*/
  nullptr,                        /*tp_str*/
  nullptr,                        /*tp_getattro*/
  nullptr,                        /*tp_setattro*/
  nullptr,                        /*tp_as_buffer*/
  Py_TPFLAGS_DEFAULT,             /*tp_flags*/
  "GDB float format object",      /* tp_doc */
  nullptr,                        /* tp_traverse */
  nullptr,                        /* tp_clear */
  nullptr,                        /* tp_richcompare */
  0,                              /* tp_weaklistoffset */
  nullptr,                        /* tp_iter */
  nullptr,                        /* tp_iternext */
  float_format_object_methods,    /* tp_methods */
  nullptr,                        /* tp_members */
  float_format_object_getset,     /* tp_getset */
  nullptr,                        /* tp_base */
  nullptr,                        /* tp_dict */
  nullptr,                        /* tp_descr_get */
  nullptr,                        /* tp_descr_set */
  0,                              /* tp_dictoffset */
  ffpy_init,                      /* tp_init */
  nullptr,                        /* tp_alloc */
  PyType_GenericNew,              /* tp_new */
};


