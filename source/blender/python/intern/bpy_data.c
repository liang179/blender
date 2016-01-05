/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Bastien Montagne
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/python/intern/bpy_data.c
 *  \ingroup pythonintern
 *
 * This file exposed blend file library appending/linking to python, typically
 * this would be done via RNA api but in this case a hand written python api
 * allows us to use pythons context manager (__enter__ and __exit__).
 *
 * Everything here is exposed via bpy.data.libraries.load(...) which returns
 * a context manager.
 */

#include <Python.h>
#include <stddef.h>

#include "BLI_utildefines.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_library.h"
#include "BKE_library_query.h"

#include "DNA_ID.h"

#include "bpy_util.h"
#include "bpy_data.h"

#include "../generic/py_capi_utils.h"
#include "../generic/python_utildefines.h"

#include "RNA_access.h"
#include "RNA_types.h"
#include "bpy_rna.h"

typedef struct IDUserMapData {
	ID *id_curr;
	PyObject *py_id_curr;
	PyObject *user_map;
	bool is_restricted;
} IDUserMapData;

static bool foreach_libblock_id_user_map_callback(void *user_data, ID **id_p, int UNUSED(cb_flag))
{
	IDUserMapData *data = user_data;

	if (*id_p) {
		PyObject *key = pyrna_id_CreatePyObject(*id_p);
		PyObject *set;

		if (data->is_restricted) {
			if ((set = PyDict_GetItem(data->user_map, key))) {
				if (data->py_id_curr == NULL) {
					data->py_id_curr = pyrna_id_CreatePyObject(data->id_curr);
				}
				PySet_Add(set, data->py_id_curr);
			}
		}
		else {
			if ((set = PyDict_GetItem(data->user_map, key)) == NULL) {
				set = PySet_New(NULL);
				PyDict_SetItem(data->user_map, key, set);
				Py_DECREF(set);
			}
			if (data->py_id_curr == NULL) {
				data->py_id_curr = pyrna_id_CreatePyObject(data->id_curr);
			}
			PySet_Add(set, data->py_id_curr);
		}
	}

	return true;
}

PyDoc_STRVAR(bpy_user_map_doc,
".. method:: user_map(*args)\n"
"\n"
"   Returns a dict mapping all ID datablocks in current bpy.data (or the subset matching\n"
"   given IDs as optional parameters) to a set of all datablocks using them.\n"
"\n"
"   WARNING: The keys and items of returned dict use ID themselves, not their names.\n"
"            This means the dict will likely become invalid after undo/redo and other operations\n"
"            affecting Blender's internal database."
"\n"
"   :arg *args: One or more datablocks (or iterables of datablocks, e.g. bpy.data.objects...) to search for.\n"
"   :type *args: ID\n"
);
static PyObject *bpy_user_map(PyObject *UNUSED(self), PyObject *args)
{
#if 0  /* If someone knows how to get a proper 'self' in that case... */
	BPy_StructRNA *pyrna = (BPy_StructRNA *)self;
	Main *bmain = pyrna->ptr.data;
#else
	Main *bmain = G.main;  /* XXX Ugly, but should work! */
#endif

	PyObject *user_map = PyDict_New();
	PyObject *ref_id;

	IDUserMapData data_cb = {NULL};

	ListBase *lb_array[MAX_LIBARRAY];
	int lb_idx;

	data_cb.user_map = user_map;

	if (PyTuple_Size(args)) {
		PyObject *iter = PyObject_GetIter(args);

		data_cb.is_restricted = true;
		while ((ref_id = PyIter_Next(iter))) {
			PyObject *sub_iter = PyObject_GetIter(ref_id);
			if (sub_iter && PyIter_Check(sub_iter)) {
				PyObject *sub_ref_id;

				while ((sub_ref_id = PyIter_Next(sub_iter))) {
					PyObject *set = PySet_New(NULL);
					PyDict_SetItem(user_map, sub_ref_id, set);
					Py_DECREF(set);
					Py_DECREF(sub_ref_id);
				}
				Py_DECREF(sub_iter);
			}
			else {
				PyObject *set = PySet_New(NULL);
				PyDict_SetItem(user_map, ref_id, set);
				Py_DECREF(set);
			}
			Py_DECREF(ref_id);
		}
		Py_DECREF(iter);
	}

	lb_idx = set_listbasepointers(bmain, lb_array);

	while (lb_idx--) {
		for (ID *id = lb_array[lb_idx]->first; id; id = id->next) {
			data_cb.id_curr = id;
			data_cb.py_id_curr = NULL;
			BKE_library_foreach_ID_link(id, foreach_libblock_id_user_map_callback, &data_cb, IDWALK_NOP);
			if (data_cb.py_id_curr) {
				Py_DECREF(data_cb.py_id_curr);
			}
		}
	}

	return user_map;
}

int BPY_data_module(PyObject *mod_par)
{
	static PyMethodDef user_map = {"user_map", (PyCFunction)bpy_user_map, METH_VARARGS, bpy_user_map_doc};

	PyModule_AddObject(mod_par, "_data_user_map", PyCFunction_New(&user_map, NULL));

	return 0;
}
