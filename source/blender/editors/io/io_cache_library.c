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
 * The Original Code is Copyright (C) 2015 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/io/io_cache_library.c
 *  \ingroup editor/io
 */

#include "BLF_translation.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "DNA_cache_library_types.h"
#include "DNA_object_types.h"

#include "BKE_cache_library.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "ED_screen.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "WM_api.h"
#include "WM_types.h"

#include "io_cache_library.h"

/********************** new cache library operator *********************/

static int new_cachelib_exec(bContext *C, wmOperator *UNUSED(op))
{
	CacheLibrary *cachelib = CTX_data_pointer_get_type(C, "cachelib", &RNA_CacheLibrary).data;
	Main *bmain = CTX_data_main(C);
	PointerRNA ptr, idptr;
	PropertyRNA *prop;
	
	/* add or copy material */
	if (cachelib) {
		cachelib = BKE_cache_library_copy(cachelib);
	}
	else {
		cachelib = BKE_cache_library_add(bmain, DATA_("CacheLibrary"));
	}
	
	/* hook into UI */
	UI_context_active_but_prop_get_templateID(C, &ptr, &prop);
	
	if (prop) {
		/* when creating new ID blocks, use is already 1, but RNA
		* pointer se also increases user, so this compensates it */
		cachelib->id.us--;
		
		RNA_id_pointer_create(&cachelib->id, &idptr);
		RNA_property_pointer_set(&ptr, prop, idptr);
		RNA_property_update(C, &ptr, prop);
	}
	
	WM_event_add_notifier(C, NC_OBJECT, cachelib);
	
	return OPERATOR_FINISHED;
}

void CACHELIBRARY_OT_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "New Cache Library";
	ot->idname = "CACHELIBRARY_OT_new";
	ot->description = "Add a new cache library";
	
	/* api callbacks */
	ot->exec = new_cachelib_exec;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
}

/********************** enable cache item operator *********************/

static int cache_item_enable_poll(bContext *C)
{
	CacheLibrary *cachelib = CTX_data_pointer_get_type(C, "cachelib", &RNA_CacheLibrary).data;
	Object *obcache = CTX_data_pointer_get_type(C, "cache_object", &RNA_Object).data;
	
	if (!cachelib || !obcache)
		return false;
	
	return true;
}

static int cache_item_enable_exec(bContext *C, wmOperator *op)
{
	CacheLibrary *cachelib = CTX_data_pointer_get_type(C, "cachelib", &RNA_CacheLibrary).data;
	Object *obcache = CTX_data_pointer_get_type(C, "cache_object", &RNA_Object).data;
	int type = RNA_enum_get(op->ptr, "type");
	int index = RNA_int_get(op->ptr, "index");
	
	CacheItem *item = BKE_cache_library_add_item(cachelib, obcache, type, index);
	item->flag |= CACHE_ITEM_ENABLED;
	
	WM_event_add_notifier(C, NC_OBJECT, cachelib);
	
	return OPERATOR_FINISHED;
}

void CACHELIBRARY_OT_item_enable(wmOperatorType *ot)
{
	PropertyRNA *prop;
	
	/* identifiers */
	ot->name = "Enable Cache Item";
	ot->idname = "CACHELIBRARY_OT_item_enable";
	ot->description = "Enable a cache item";
	
	/* api callbacks */
	ot->poll = cache_item_enable_poll;
	ot->exec = cache_item_enable_exec;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;
	
	prop = RNA_def_enum(ot->srna, "type", cache_library_item_type_items, CACHE_TYPE_OBJECT, "Type", "Type of cache item to add");
	RNA_def_property_flag(prop, PROP_REQUIRED);
	RNA_def_int(ot->srna, "index", -1, -1, INT_MAX, "Index", "Index of data in the object", -1, INT_MAX);
}