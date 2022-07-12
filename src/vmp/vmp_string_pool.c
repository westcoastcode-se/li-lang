#include "vmp_string_pool.h"
#include "../vm_debug.h"

//
// PRIVATE ///////////////////////////////////////////////////////////////////////////
// 

void _vmp_compiler_string_copy(vm_string* s, const char* src, int len)
{
	char* dest = (char*)vm_malloc(len);
	s->start = dest;
	s->end = dest + len;
	for (int i = 0; i < len; ++i) {
		*dest++ = *src++;
	}
}

//
// PUBLIC ///////////////////////////////////////////////////////////////////////////
// 

void vmp_string_pool_init(vmp_string_pool* p)
{
	p->first = p->last = NULL;
}

void vmp_string_pool_destroy(vmp_string_pool* p)
{
	vmp_string_pool_entry* e = p->first;
	while (e != NULL) {
		vmp_string_pool_entry* const next = e->next;
		vm_free((void*)e->value.start);
		vm_free(e);
		e = next;
	}
	p->first = p->last = NULL;
}

const vm_string* vmp_string_pool_stringsz(vmp_string_pool* p, const char* str, int len)
{
	// Try to find an existing string
	vmp_string_pool_entry* e = p->first;
	while (e != NULL) {
		if (vm_string_cmpsz(&e->value, str, len)) {
			return &e->value;
		}
		e = e->next;
	}

	// If not, then add it to the string pool
	e = (vmp_string_pool_entry*)vm_malloc(sizeof(vmp_string_pool_entry));
	if (e == NULL) {
		return NULL;
	}
	e->next = NULL;
	_vmp_compiler_string_copy(&e->value, str, len);
	if (p->last != NULL) {
		p->last->next = e;
		e->index = p->last->index + 1;
		e->offset = p->last->offset + vm_string_length(&p->last->value);
		p->last = e;
	}
	else {
		e->index = 0;
		e->offset = 0;
		p->first = p->last = e;
	}
	return &e->value;
}

const vm_string* vmp_string_pool_string(vmp_string_pool* p, const vm_string* s)
{
	return vmp_string_pool_stringsz(p, s->start, vm_string_length(s));
}
