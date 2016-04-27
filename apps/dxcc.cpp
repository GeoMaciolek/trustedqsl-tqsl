/***************************************************************************
                          dxcc.cpp  -  description
                             -------------------
    begin                : Tue Jun 18 2002
    copyright            : (C) 2002 by ARRL
    author               : Jon Bloom
    email                : jbloom@arrl.org
    revision             : $Id$
 ***************************************************************************/

#include <stdlib.h>
#include <string.h>

#include "dxcc.h"
#include "tqsllib.h"
#include "tqsltrace.h"

#include "winstrdefs.h"

bool DXCC::_init = false;

static int num_entities = 0;

static struct _dxcc_entity {
	int number;
	const char *name;
	const char *zonemap;
} *entity_list = 0;

static int
_ent_cmp(const void *a, const void *b) {
	return strcasecmp(((struct _dxcc_entity *)a)->name, ((struct _dxcc_entity *)b)->name);
}

bool
DXCC::init() {
	tqslTrace("DXCC::init", NULL);
//	if (!_init) {
		if (tqsl_getNumDXCCEntity(&num_entities))
			return false;
		entity_list = new struct _dxcc_entity[num_entities];
		for (int i = 0; i < num_entities; i++) {
			tqsl_getDXCCEntity(i, &(entity_list[i].number), &(entity_list[i].name));
			tqsl_getDXCCZoneMap(entity_list[i].number, &(entity_list[i].zonemap));
		}
		qsort(entity_list, num_entities, sizeof(struct _dxcc_entity), &_ent_cmp);
//	}
	_init = true;
	return _init;
}

bool
DXCC::getFirst() {
	tqslTrace("DXCC::getFirst", NULL);
	if (!init())
		return false;
	_index = -1;
	return getNext();
}

bool
DXCC::getNext() {
	int newidx = _index+1;
	if (newidx < 0 || newidx >= num_entities) {
		tqslTrace("DXCC::getNext", "index error, newidx=%d, n=%d", newidx, num_entities);
		return false;
	}
	_index = newidx;
	_number = entity_list[newidx].number;
	_name = entity_list[newidx].name;
	_zonemap = entity_list[newidx].zonemap;
	return true;
}

bool
DXCC::getByEntity(int e) {
	tqslTrace("DXCC::getByEntity", "e=%d", e);
	_number = 0;
	_name = "<NONE>";
	_zonemap = "";
	if (!init())
		return false;
	for (int i = 0; i < num_entities; i++) {
		if (entity_list[i].number == e) {
			_index = i;
			_number = entity_list[i].number;
			_name = entity_list[i].name;
			_zonemap = entity_list[i].zonemap;
			return true;
		}
	}
	return false;
}


