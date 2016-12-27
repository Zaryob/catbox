/*
** Copyright (c) 2007, TUBITAK/UEKAE
**
** This program is free software; you can redistribute it and/or modify it
** under the terms of the GNU General Public License as published by the
** Free Software Foundation; either version 2 of the License, or (at your
** option) any later version. Please read the COPYING file.
*/

#include "catbox.h"
#include <structmember.h>

typedef struct {
	PyObject_HEAD
	int code;
	PyObject *violations;
} RetVal;

static PyMemberDef members[] = {
	{ "code", T_INT, offsetof(RetVal, code), 0, NULL },
	{ "violations", T_OBJECT, offsetof(RetVal, violations), 0, NULL },
	{ NULL, 0, 0, 0, NULL }
};

static PyTypeObject RetVal_type = {
	PyObject_HEAD_INIT(NULL)
	0,			/* ob_size */
	"catbox.RetVal",	/* tp_name */
	sizeof(RetVal),		/* tp_basicsize */
	0,			/* tp_itemsize */
	0,			/* tp_dealloc */
	0,			/* tp_print */
	0,			/* tp_getattr */
	0,			/* tp_setattr  */
	0,			/* tp_compare */
	0,			/* tp_repr */
	0,			/* tp_as_number */
	0,			/* tp_as_sequence */
	0,			/* tp_as_mapping */
	0,			/* tp_hash */
	0,			/* tp_call */
	0,			/* tp_str */
	0,			/* tp_getattro */
	0,			/* tp_setattro */
	0,			/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,	/* tp_flags */
	"Sandbox log",		/* tp_doc */
	0,			/* tp_traverse */
	0,			/* tp_clear */
	0,			/* tp_richcompare */
	0,			/* tp_weaklistoffset */
	0,			/* tp_iter */
	0,			/* tp_iternext */
	0,			/* tp_methods */
	members,		/* tp_members */
	0,			/* tp_getset */
	0,			/* tp_base */
	0,			/* tp_dict */
	0,			/* tp_descr_get */
	0,			/* tp_descr_set */
	0,			/* tp_dictoffset */
	0,			/* tp_init */
	0,			/* tp_alloc */
	0			/* tp_new */
};

static int is_initialized = 0;

int
catbox_retval_init(struct trace_context *ctx)
{
	RetVal *ret;

	if (!is_initialized) {
		RetVal_type.tp_new = PyType_GenericNew;
		if (PyType_Ready(&RetVal_type) < 0)
			return -1;
		Py_INCREF(&RetVal_type);
		is_initialized = 1;
	}
	ret = PyObject_New(RetVal, &RetVal_type);
	ret->code = 0;
	ret->violations = PyList_New(0);
	ctx->retval = (PyObject *) ret;
	return 0;
}

void
catbox_retval_set_exit_code(struct trace_context *ctx, int retcode)
{
	RetVal *ret = (RetVal *) ctx->retval;

	ret->code = retcode;
}

void
catbox_retval_add_violation(struct trace_context *ctx, const char *operation, const char *path, const char *canonical)
{
	RetVal *ret = (RetVal *) ctx->retval;
	PyObject *item;

	item = PyTuple_New(3);
	PyTuple_SetItem(item, 0, PyBytes_FromString(operation));
	PyTuple_SetItem(item, 1, PyBytes_FromString(path));
	PyTuple_SetItem(item, 2, PyBytes_FromString(canonical));
	PyList_Append(ret->violations, item);

	if (ctx->logger) {
		PyObject_Call(ctx->logger, item, NULL);
	}
}
