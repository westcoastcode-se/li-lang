#include "vmc_compiler.h"
#include "vmc_lexer_logic.h"
#include "vmc_lexer_math.h"
#include "vmc_compiler_messages.h"
#include "../interpreter/vmi_ops.h"
#include "../interpreter/vmi_process.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>

//
// PRIVATE ///////////////////////////////////////////////////////////////////////////
// 

VM_STRING_CONST(int8, "int8", 4);
VM_STRING_CONST(uint8, "uint8", 5);
VM_STRING_CONST(int16, "int16", 5);
VM_STRING_CONST(uint16, "uint16", 6);
VM_STRING_CONST(int32, "int32", 5);
VM_STRING_CONST(uint32, "uint32", 6);
VM_STRING_CONST(int64, "int64", 5);
VM_STRING_CONST(uint64, "uint64", 6);
VM_STRING_CONST(float32, "float32", 7);
VM_STRING_CONST(float64, "float64", 7);

VM_STRING_CONST(load_a, "load_a", 6);
VM_STRING_CONST(save_r, "save_r", 6);
VM_STRING_CONST(const, "const", 5);
VM_STRING_CONST(alloc_s, "alloc_s", 7);
VM_STRING_CONST(free_s, "free_s", 6);
VM_STRING_CONST(ret, "ret", 3);
VM_STRING_CONST(add, "add", 3);
VM_STRING_CONST(call, "call", 4);
VM_STRING_CONST(conv, "conv", 4);

VM_STRING_CONST(_0, "_0", 2);
VM_STRING_CONST(_1, "_1", 2);
VM_STRING_CONST(_2, "_2", 2);
VM_STRING_CONST(_3, "_3", 2);
VM_STRING_CONST(_4, "_4", 2);
VM_STRING_CONST(_5, "_5", 2);
VM_STRING_CONST(_6, "_6", 2);
VM_STRING_CONST(_7, "_7", 2);
VM_STRING_CONST(_8, "_8", 2);
VM_STRING_CONST(_9, "_9", 2);

const vm_string* _vm_string_const_anon_names[10] = {
	VM_STRING_CONST_GET(_0),
	VM_STRING_CONST_GET(_1),
	VM_STRING_CONST_GET(_2),
	VM_STRING_CONST_GET(_3),
	VM_STRING_CONST_GET(_4),
	VM_STRING_CONST_GET(_5),
	VM_STRING_CONST_GET(_6),
	VM_STRING_CONST_GET(_7),
	VM_STRING_CONST_GET(_8),
	VM_STRING_CONST_GET(_9)
};

const vmc_compiler_config _vmc_compiler_config_default = {
	NULL,
	&vmc_compiler_config_import
};

void _vmc_emit_begin(vmc_compiler* c, vm_int8 argument_total_size, vm_int8 return_total_size)
{
	vmi_instr_begin instr;
	instr.opcode = 0;
	instr.icode = VMI_BEGIN;
	instr.expected_stack_size = argument_total_size + return_total_size;
	vmc_write(c, &instr, sizeof(vmi_instr_begin));
}

void _vmc_emit_opcode(vmc_compiler* c, vm_int32 opcode)
{
	vmi_instr_single_instruction instr;
	instr.header.opcode = opcode;
	vmc_write(c, &instr, sizeof(vmi_instr_single_instruction));
}

void _vmc_emit_icode(vmc_compiler* c, vm_int8 icode)
{
	vmi_instr_single_instruction instr;
	instr.header.opcode = 0;
	instr.header.icode = icode;
	vmc_write(c, &instr, sizeof(vmi_instr_single_instruction));
}

// Initialize the function memory
void _vmc_func_init(vmc_func* func)
{
	VMC_INIT_TYPE_HEADER(func, VMC_TYPE_HEADER_FUNC, sizeof(void*));
	func->package = NULL;
	vm_string_zero(&func->name);
	func->offset = -1;
	func->next = NULL;
	func->modifiers = 0;
	func->complexity = 0;
	func->complexity_components = 0;
	func->args_count = 0;
	func->returns_count = 0;
}

// Allocate new memory for a potential function. Will return NULL if the system is out of memory
vmc_func* _vmc_func_malloc()
{
	vmc_func* func = (vmc_func*)malloc(sizeof(vmc_func));
	if (func == NULL)
		return NULL;
	_vmc_func_init(func);
	return func;
}

// Destroy memory allocated for the supplied function
void _vmc_func_free(vmc_func* func)
{
	free(func);
}

vmc_package* _vmc_package_malloc(const char* name, int length)
{
	vmc_package* p = (vmc_package*)malloc(sizeof(vmc_package));
	if (p == NULL)
		return NULL;
	VMC_INIT_TYPE_HEADER(p, VMC_TYPE_HEADER_PACKAGE, sizeof(void*));
	vm_string_setsz(&p->name, name, length);
	p->full_name = p->name;
	p->func_first = p->func_last = NULL;
	p->func_count = 0;
	p->type_first = p->type_last = NULL;
	p->type_count = 0;
	p->data_offset = 0;
	p->root_package = NULL;
	p->next = NULL;
	return p;
}

void _vmc_package_free(vmc_package* p)
{
	vmc_func* f;
	vmc_type_definition* t;

	// Cleanup functions
	f = p->func_first;
	while (f != NULL) {
		vmc_func* next = f->next;
		_vmc_func_free(f);
		f = next;
	}

	// Cleanup types
	t = p->type_first;
	while (t != NULL) {
		vmc_type_definition* next = t->next;
		free(t);
		t = next;
	}

	free(p);
}

void _vmc_append_header(vmc_compiler* c)
{
	vmi_process_header header;
	header.header[0] = 'V';
	header.header[1] = 'M';
	header.header[2] = '0';
	header.version = VM_VERSION;
	header.data_offset = 0;
	header.code_offset = sizeof(vmi_process_header);
	header.packages_count = 0;
	header.functions_count = 0;
	header.first_package_offset = 0;
	vmc_write(c, &header, sizeof(header));
}

// Add the supplied function to the supplied package
void _vmc_package_add_func(vmc_package* p, vmc_func* f)
{
	if (p->func_last == NULL) {
		p->func_first = p->func_last = f;
	}
	else {
		p->func_last->next = f;
		p->func_last = f;
	}
	p->func_count++;
}

// Calculate the offset on the stack for arguments, return values and local variables
void _vmc_calculate_offsets(vmc_func* func) {
	vm_int32 offset = func->args_total_size + func->returns_total_size;
	vm_int32 i;
	if (func->args_count > 0) {
		for (i = 0; i < func->args_count; ++i) {
			vmc_type_info* info = &func->args[i].type;
			offset -= info->size;
			info->offset = offset;
		}
	}
	if (func->returns_count > 0) {
		for (i = 0; i < func->returns_count; ++i) {
			vmc_type_info* info = &func->returns[i].type;
			offset -= info->size;
			info->offset = offset;
		}
	}
}

const vm_string* _vmc_prepare_func_get_signature(vmc_compiler* c, vm_string name, 
	vmc_var_definition* args, vm_int32 args_count, 
	vmc_var_definition* returns, vm_int32 returns_count)
{
	char memory[2048];
	char* ptr = vm_str_cpy(memory, name.start, vm_string_length(&name));
	*ptr++ = '(';
	for (int i = 0; i < args_count; ++i) {
		const vm_int32 len = vm_string_length(&args[i].type.definition->name);
		const vmc_type_info* info = &args[i].type;
		if (i > 0)
			*ptr++ = ',';
		if ((info->masks & VMC_TYPE_INFO_MASK_ADDR) == VMC_TYPE_INFO_MASK_ADDR)
			*ptr++ = '*';
		ptr = vm_str_cpy(ptr, info->definition->name.start, len);
	}
	*ptr++ = ')';
	*ptr++ = '(';
	for (int i = 0; i < returns_count; ++i) {
		const vm_int32 len = vm_string_length(&returns[i].type.definition->name);
		const vmc_type_info* info = &returns[i].type;
		if (i > 0)
			*ptr++ = ',';
		if ((info->masks & VMC_TYPE_INFO_MASK_ADDR) == VMC_TYPE_INFO_MASK_ADDR)
			*ptr++ = '*';
		ptr = vm_str_cpy(ptr, info->definition->name.start, len);
	}
	*ptr++ = ')';
	return vmc_string_pool_stringsz(&c->string_pool, memory, (int)(ptr - memory));
}

BOOL _vmc_prepare_func_signature(vmc_compiler* c, vmc_func* func)
{
	func->signature = *_vmc_prepare_func_get_signature(c, func->name, func->args, func->args_count,
		func->returns, func->returns_count);
	return TRUE;
}

BOOL _vmc_compiler_parse_type_decl_without_name(vmc_compiler* c, vmc_lexer* l, vmc_package* p, vmc_lexer_token* t,
	vmc_var_definition* vars, vm_int32* out_count, vm_int32* out_total_size)
{
	vm_int32 count = 0;
	vm_int32 total_size = 0;
	vmc_package* type_package;
	vmc_type_definition* type_definition;

	// Expected a '(' token
	if (!vmc_lexer_next_type(l, t, VMC_LEXER_TYPE_PARAN_L))
		return vmc_compiler_message_syntax_error(&c->messages, l, t, '(');

	// Parse until we reach that end ')' token
	while (!vmc_lexer_next_type(l, t, VMC_LEXER_TYPE_PARAN_R)) {
		vmc_var_definition* const var = &vars[count];
		type_package = p;

		// Ignore comma
		if (t->type == VMC_LEXER_TYPE_COMMA) {
			vmc_lexer_next(l, t);
		}

		// Reset masks
		var->type.masks = 0;
		var->type.offset = 0;
		var->name = *_vm_string_const_anon_names[count];

		// types can be:
		// KEYWORD
		// *KEYWORD
		// [INT]KEYWORD
		// KEYWPORD<TYPE>

		// We can, quickly figure out if this is a type if the keyword is:
		// '[' or '*' since those are part of the type
		if (t->type == VMC_LEXER_TYPE_SQUARE_L || t->type == VMC_LEXER_TYPE_PTR) {
			// These types of types are not supported yet!
			return vmc_compiler_message_not_implemented(&c->messages, l, t);
		}

		// Expected type
		if (t->type != VMC_LEXER_TYPE_KEYWORD)
			return vmc_compiler_message_expected_type(&c->messages, l, t);

		type_definition = vmc_package_find_type(type_package, &t->string);
		if (type_definition == NULL)
			return vmc_compiler_message_type_not_found(&c->messages, l, t);
		var->type.definition = type_definition;

		// Figure out the size of the variable
		if ((var->type.masks & VMC_TYPE_INFO_MASK_ADDR) == VMC_TYPE_INFO_MASK_ADDR)
			var->type.size = sizeof(void*);
		else
			var->type.size = type_definition->size;

		// Argument now loaded
		count++;
		total_size += var->type.size;
	}

	*out_count = count;
	*out_total_size = total_size;
	return TRUE;
}

BOOL _vmc_parse_keyword_fn_args(vmc_compiler* c, vmc_lexer* l, vmc_package* p, vmc_lexer_token* t, vmc_func* func)
{
	func->args_count = 0;
	func->args_total_size = 0;
	vmc_package* type_package;
	vmc_type_definition* type_definition;

	// Expected a '(' token
	if (!vmc_lexer_next_type(l, t, VMC_LEXER_TYPE_PARAN_L))
		return vmc_compiler_message_syntax_error(&c->messages, l, t, '(');

	// Parse until we reach that end ')' token
	while (!vmc_lexer_next_type(l, t, VMC_LEXER_TYPE_PARAN_R)) {
		vmc_var_definition* const var = &func->args[func->args_count];
		type_package = p;

		// Ignore comma
		if (t->type == VMC_LEXER_TYPE_COMMA) {
			vmc_lexer_next(l, t);
		}

		// Reset masks
		var->type.masks = 0;
		var->type.offset = 0;

		// Read var name
		if (t->type != VMC_LEXER_TYPE_KEYWORD)
			return vmc_compiler_message_expected_identifier(&c->messages, l, t);
		var->name = t->string;
		vmc_lexer_next(l, t);

		// The first part can be a name OR a type
		// names are guaranteed to be a KEYWORD
		// vars can be:
		// KEYWORD
		// *KEYWORD
		// [INT]KEYWORD
		// KEYWPORD<TYPE>

		// We can, quickly figure out if this is a type if the keyword is:
		// '[' or '*' since those are part of the type
		if (t->type == VMC_LEXER_TYPE_SQUARE_L || t->type == VMC_LEXER_TYPE_PTR) {
			// These types of types are not supported yet!
			return vmc_compiler_message_not_implemented(&c->messages, l, t);
		}

		// Expected type
		if (t->type != VMC_LEXER_TYPE_KEYWORD)
			return vmc_compiler_message_expected_type(&c->messages, l, t);

		type_definition = vmc_package_find_type(type_package, &t->string);
		if (type_definition == NULL)
			return vmc_compiler_message_type_not_found(&c->messages, l, t);
		var->type.definition = type_definition;

		// Figure out the size of the variable
		if ((var->type.masks & VMC_TYPE_INFO_MASK_ADDR) == VMC_TYPE_INFO_MASK_ADDR)
			var->type.size = sizeof(void*);
		else
			var->type.size = type_definition->size;

		// Argument now loaded
		func->args_count++;
		func->args_total_size += var->type.size;
	}

	return TRUE;
}

BOOL _vmc_parse_keyword_fn_rets(vmc_compiler* c, vmc_lexer* l, vmc_package* p, vmc_lexer_token* t, vmc_func* func)
{
	return _vmc_compiler_parse_type_decl_without_name(c, l, p, t,
		func->returns, &func->returns_count, &func->returns_total_size);
}

BOOL _vmc_parse_keyword_fn_body(vmc_compiler* c, vmc_lexer* l, vmc_package* p, vmc_lexer_token* t, vmc_func* func)
{
	// External functions are not allowed to have a body
	if (vmc_func_is_extern(func))
		return TRUE;

	// Expected a {
	if (!vmc_lexer_next_type(l, t, VMC_LEXER_TYPE_BRACKET_L))
		return vmc_compiler_message_syntax_error(&c->messages, l, t, '{');
	vmc_lexer_next(l, t);

	// Body is now definde and we know where the body bytecode starts
	func->offset = vm_bytestream_get_size(&c->bytecode);

	// The function is actually beginning here
	// TODO: Add support for reserving memory for local variables
	if (func->args_total_size > INT8_MAX)
		return vmc_compiler_message_not_implemented(&c->messages, l, t);
	if (func->returns_total_size > INT8_MAX)
		return vmc_compiler_message_not_implemented(&c->messages, l, t);
	_vmc_emit_begin(c, func->args_total_size, func->returns_total_size);

	while (1) {
		// Unexpected end of function body
		if (t->type == VMC_LEXER_TYPE_EOF) {
			return vmc_compiler_message_unexpected_eof(&c->messages, l, t);
		}

		// Break if we've reached the end of the function
		if (t->type == VMC_LEXER_TYPE_BRACKET_R) {
			break;
		}

		// Ignore comments
		if (t->type == VMC_LEXER_TYPE_COMMENT) {
			vmc_lexer_next(l, t);
			continue;
		}

		// Verify that a keyword is being processed
		if (!vmc_lexer_type_is_keyword(t->type)) {
			return vmc_compiler_message_expected_keyword(&c->messages, l, t);
		}

		if (vm_string_cmp(&t->string, VM_STRING_CONST_GET(load_a))) {
			// load_a <i32>

			vmi_instr_load_a instr;
			vm_int32 index;

			vmc_lexer_next(l, t);
			if (t->type != VMC_LEXER_TYPE_INT) {
				return vmc_compiler_message_expected_index(&c->messages, l, t);
			}

			index = (vm_int32)strtoi64(t->string.start, vm_string_length(&t->string));
			if (func->args_count == 0 || func->args_count <= index) {
				return vmc_compiler_message_invalid_index(&c->messages, l, index, 0, func->args_count - 1);
			}
			instr.opcode = 0;
			instr.icode = VMI_LOAD_A;
			instr.size = func->args[index].type.size;
			instr.offset = func->args[index].type.offset;
			vmc_write(c, &instr, sizeof(vmi_instr_load_a));
		}
		else if (vm_string_cmp(&t->string, VM_STRING_CONST_GET(const))) {
			// const <type> <value>

			vmc_lexer_next(l, t);
			if (t->type != VMC_LEXER_TYPE_KEYWORD) {
				return vmc_compiler_message_expected_type(&c->messages, l, t);
			}
			if (vm_string_cmp(&t->string, VM_STRING_CONST_GET(int16))) {
				vmi_instr_const_int32 instr;
				vmc_lexer_next(l, t);
				if (t->type != VMC_LEXER_TYPE_INT)
					return vmc_compiler_message_expected_int(&c->messages, l, t);
				instr.opcode = 0;
				instr.icode = VMI_CONST;
				instr.props1 = VMI_INSTR_CONST_PROP1_INT16;
				instr.i16 = (vm_int16)strtoi64(t->string.start, vm_string_length(&t->string));
				vmc_write(c, &instr, sizeof(vmi_instr_const_int32));

			} else if (vm_string_cmp(&t->string, VM_STRING_CONST_GET(int32))) {
				vmi_instr_const_int32 instr;
				vmc_lexer_next(l, t);
				if (t->type != VMC_LEXER_TYPE_INT)
					return vmc_compiler_message_expected_int(&c->messages, l, t);
				instr.opcode = 0;
				instr.icode = VMI_CONST;
				instr.props1 = VMI_INSTR_CONST_PROP1_INT32;
				instr.value = (vm_int32)strtoi64(t->string.start, vm_string_length(&t->string));
				vmc_write(c, &instr, sizeof(vmi_instr_const_int32));
			}
			else
				return vmc_compiler_message_not_implemented(&c->messages, l, t);
		}
		else if (vm_string_cmp(&t->string, VM_STRING_CONST_GET(add))) {
			// add <type>

			vmi_opcode opcode = VMI_ADD;

			vmc_lexer_next(l, t);
			if (t->type != VMC_LEXER_TYPE_KEYWORD)
				return vmc_compiler_message_expected_type(&c->messages, l, t);
			if (vm_string_cmp(&t->string, VM_STRING_CONST_GET(int32)))
				opcode |= VMI_PROPS1_OPCODE(VMI_INSTR_ADD_PROP1_INT32);
			else if (vm_string_cmp(&t->string, VM_STRING_CONST_GET(int16)))
				opcode |= VMI_PROPS1_OPCODE(VMI_INSTR_ADD_PROP1_INT16);
			_vmc_emit_opcode(c, opcode);
		}
		else if (vm_string_cmp(&t->string, VM_STRING_CONST_GET(save_r))) {
			// save_r <i32>

			vmi_instr_save_r instr;
			vm_int32 index;

			vmc_lexer_next(l, t);
			if (t->type != VMC_LEXER_TYPE_INT) {
				return vmc_compiler_message_expected_index(&c->messages, l, t);
			}

			index = (vm_int32)strtoi64(t->string.start, vm_string_length(&t->string));
			if (func->returns_count == 0 || func->returns_count <= index) {
				return vmc_compiler_message_invalid_index(&c->messages, l, index, 0, func->returns_count - 1);
			}
			instr.opcode = 0;
			instr.icode = VMI_SAVE_R;
			instr.size = func->returns[index].type.size;
			instr.offset = func->returns[index].type.offset;
			vmc_write(c, &instr, sizeof(vmi_instr_save_r));
		}
		else if (vm_string_cmp(&t->string, VM_STRING_CONST_GET(alloc_s))) {
			// alloc_s <i32>

			vmi_instr_alloc_s instr;
			vm_int32 num_bytes;

			vmc_lexer_next(l, t);
			if (t->type != VMC_LEXER_TYPE_INT) {
				return vmc_compiler_message_expected_int(&c->messages, l, t);
			}

			num_bytes = (vm_int32)strtoi64(t->string.start, vm_string_length(&t->string));
			if (num_bytes < 0) {
				return vmc_compiler_message_expected_int(&c->messages, l, t);
			}
			else if (num_bytes > UINT16_MAX) {
				return vmc_compiler_message_not_implemented(&c->messages, l, t);
			}
			instr.opcode = 0;
			instr.icode = VMI_ALLOC_S;
			instr.size = num_bytes;
			vmc_write(c, &instr, sizeof(vmi_instr_alloc_s));
		}
		else if (vm_string_cmp(&t->string, VM_STRING_CONST_GET(free_s))) {
			// free_s <i32>

			vmi_instr_free_s instr;
			vm_int32 num_bytes;

			vmc_lexer_next(l, t);
			if (t->type != VMC_LEXER_TYPE_INT) {
				return vmc_compiler_message_expected_int(&c->messages, l, t);
			}

			num_bytes = (vm_int32)strtoi64(t->string.start, vm_string_length(&t->string));
			if (num_bytes < 0) {
				return vmc_compiler_message_expected_int(&c->messages, l, t);
			}
			else if (num_bytes > UINT16_MAX) {
				return vmc_compiler_message_not_implemented(&c->messages, l, t);
			}
			instr.opcode = 0;
			instr.icode = VMI_FREE_S;
			instr.size = num_bytes;
			vmc_write(c, &instr, sizeof(vmi_instr_free_s));
		}
		else if (vm_string_cmp(&t->string, VM_STRING_CONST_GET(ret))) {
			// ret

			vmi_instr_ret instr;
			instr.opcode = 0;
			instr.icode = VMI_RET;
			instr.pop_stack_size = func->args_total_size;
			vmc_write(c, &instr, sizeof(vmi_instr_ret));
		}
		else if (vm_string_cmp(&t->string, VM_STRING_CONST_GET(call))) {
			// call <definition>
			vm_string name;
			vmc_var_definition args[9];
			vm_int32 args_count;
			vmc_var_definition returns[9];
			vm_int32 returns_count;
			vm_int32 _;
			vmc_func* func;
			vmi_instr_call instr;
			
			// Name of the function
			if (!vmc_lexer_next_type(l, t, VMC_LEXER_TYPE_KEYWORD))
				return vmc_compiler_message_expected_keyword(&c->messages, l, t);
			name = t->string;

			// Fetch arguments
			if (!_vmc_compiler_parse_type_decl_without_name(c, l, p, t, args, &args_count, &_))
				return FALSE;

			// Fetch returns
			if (!_vmc_compiler_parse_type_decl_without_name(c, l, p, t, returns, &returns_count, &_))
				return FALSE;

			func = vmc_func_find(p, _vmc_prepare_func_get_signature(c, name, args, args_count, returns, returns_count));
			if (func == NULL) {
				return vmc_compiler_message_not_implemented(&c->messages, l, t);
			}
			instr.header.opcode = 0;
			instr.header.icode = VMI_CALL;
			instr.addr = OFFSET(func->offset);
			vmc_write(c, &instr, sizeof(vmi_instr_call));
		}
		else if (vm_string_cmp(&t->string, VM_STRING_CONST_GET(conv))) {
			// conv <from_type> <to_type>
			vmi_instr_conv instr;
			instr.header.opcode = 0;
			instr.header.icode = VMI_CONV;

			// From type
			if (!vmc_lexer_next_type(l, t, VMC_LEXER_TYPE_KEYWORD))
				return vmc_compiler_message_expected_keyword(&c->messages, l, t);
			if (vm_string_cmp(&t->string, VM_STRING_CONST_GET(int16)))
				instr.props1 = VMI_INSTR_CONV_PROP1_INT16;
			else
				return vmc_compiler_message_not_implemented(&c->messages, l, t);

			// To type
			if (!vmc_lexer_next_type(l, t, VMC_LEXER_TYPE_KEYWORD))
				return vmc_compiler_message_expected_keyword(&c->messages, l, t);
			if (vm_string_cmp(&t->string, VM_STRING_CONST_GET(int32)))
				instr.props2 = VMI_INSTR_CONV_PROP2_INT32;
			else
				return vmc_compiler_message_not_implemented(&c->messages, l, t);

			vmc_write(c, &instr, sizeof(vmi_instr_conv));
		}
		else {
			return vmc_compiler_message_not_implemented(&c->messages, l, t);
		}

		vmc_lexer_next(l, t);
	}
	func->modifiers |= VMC_FUNC_MODIFIER_HAS_BODY;
	vmc_lexer_next(l, t);
	return TRUE;
}

// Parse the function signature:
// [name:keyword]([arg1:keyword] [type1:keyword],...)([type1:keyword])
vmc_func* _vmc_parse_func_signature(vmc_compiler* c, vmc_lexer* l, vmc_package* p, vmc_lexer_token* t)
{
	vmc_func* func;

	// 1. Function name
	if (!vmc_lexer_next_type(l, t, VMC_LEXER_TYPE_KEYWORD)) {
		vmc_compiler_message_expected_identifier(&c->messages, l, t);
		return NULL;
	}

	func = _vmc_func_malloc();
	if (func == NULL) {
		vmc_compiler_message_panic(&c->panic_error_message, "out of memory");
		return NULL;
	}
	func->name = t->string;
	if (vmc_lexer_test_uppercase(*func->name.start))
		func->modifiers |= VMC_FUNC_MODIFIER_PUBLIC;

	// Parse args
	if (!_vmc_parse_keyword_fn_args(c, l, p, t, func))
		return NULL;

	// Parse return types
	if (!_vmc_parse_keyword_fn_rets(c, l, p, t, func))
		return NULL;

	// Build the signature string
	if (!_vmc_prepare_func_signature(c, func))
		return NULL;
	return func;
}

// Append a new function to the current package. The syntax is:
// [extern] fn name (arg1 [modifier][package.]type1, arg2 [modifier][package.]type2, ...) ([modifier][package.]ret1, [modifier][package.]ret2, ...)
//
// If the function is external then a body is not required. You are allowed to have a body if you want to override the 
BOOL _vmc_parse_keyword_fn(vmc_compiler* c, vmc_lexer* l, vmc_package* p, vmc_lexer_token* t, vm_bits32 modifiers)
{
	vmc_func* func;

	func = _vmc_parse_func_signature(c, l, p, t);
	if (func == NULL) {
		return FALSE;
	}
	_vmc_package_add_func(p, func);
	func->id = c->functions_count++;
	func->modifiers |= modifiers;

	// External functions do not have a body
	if (vmc_func_is_extern(func))
		return TRUE;

	// Figure out the offset on the stack
	_vmc_calculate_offsets(func);

	// Parse the function body
	if (!_vmc_parse_keyword_fn_body(c, l, p, t, func))
		return FALSE;
	return TRUE;
}

BOOL _vmc_parse_keyword_extern(vmc_compiler* c, vmc_lexer* l, vmc_package* p, vmc_lexer_token* t)
{
	vmc_lexer_next(l, t);
	switch (t->type)
	{
	case VMC_LEXER_TYPE_KEYWORD_FN:
		return _vmc_parse_keyword_fn(c, l, p, t, VMC_FUNC_MODIFIER_EXTERN);
	default:
		return vmc_compiler_message_unknown_token(&c->messages, l, t);
	}
}

BOOL _vmc_parse_keyword(vmc_compiler* c, vmc_lexer* l, vmc_package* p, vmc_lexer_token* t)
{
	switch (t->type)
	{
	case VMC_LEXER_TYPE_KEYWORD_EXTERN:
		return _vmc_parse_keyword_extern(c, l, p, t);
	case VMC_LEXER_TYPE_KEYWORD_FN:
		return _vmc_parse_keyword_fn(c, l, p, t, 0);
	case VMC_LEXER_TYPE_KEYWORD_CONST:
	default:
		break;
	}

	vmc_compiler_message_unknown_token(&c->messages, l, t);
	_vmc_emit_icode(c, VMI_EOE);
	return FALSE;
}

// Parse the current lexers content and put the compiled byte-code into the compiler and put the metadata into the supplied package
void _vmc_parse(vmc_compiler* c, vmc_lexer* l, vmc_package* p)
{
	vmc_lexer_token token;

	// Ignore all newlines at the start of the content
	vmc_lexer_next(l, &token);
	while (token.type == VMC_LEXER_TYPE_NEWLINE) vmc_lexer_next(l, &token);

	// Compile each instruction
	while (1)
	{
		// Is the supplied token a keyword?
		if (vmc_lexer_type_is_keyword(token.type)) {
			if (!_vmc_parse_keyword(c, l, p, &token)) {
				break;
			}
			continue;
		}

		// 
		switch (token.type)
		{
		case VMC_LEXER_TYPE_NEWLINE:
		case VMC_LEXER_TYPE_COMMENT:
			break;
		case VMC_LEXER_TYPE_EOF:
			_vmc_emit_icode(c, VMI_EOE);
			return;
		default:
			vmc_compiler_message_unknown_token(&c->messages, l, &token);
			_vmc_emit_icode(c, VMI_EOE);
			return;
		}
		vmc_lexer_next(l, &token);
	}
}

void _vmc_compiler_register_builtins(vmc_compiler* c)
{
	// Register the "vm" package and all type definitions
	c->package_first = c->package_last = _vmc_package_malloc("vm", 2);
	c->package_first->id = c->packages_count++;
	vmc_type_definition_new(c->package_first, VM_STRING_CONST_GET(int32), sizeof(vm_int32));
	vmc_type_definition_new(c->package_first, VM_STRING_CONST_GET(int16), sizeof(vm_int16));
}

vmi_process_header* _vmc_compiler_get_header(vmc_compiler* c)
{
	return ((vmi_process_header*)vm_bytestream_get(&c->bytecode, 0));
}

// Append bytecode with package, types and function information
void _vmc_append_package_info(vmc_compiler* c)
{
	vmc_package* p = c->package_first;

	// Set how many packages compiled into the bytecode
	_vmc_compiler_get_header(c)->packages_count = c->packages_count;
	// Set how many functions compiled into the bytecode
	_vmc_compiler_get_header(c)->functions_count = c->functions_count;
	// Set the actual offset of where the package information is found
	_vmc_compiler_get_header(c)->first_package_offset = vm_bytestream_get_size(&c->bytecode);

	// Memory structure for package information:
	// <Package Header>
	// char[]	| name bytes
	
	while (p != NULL) {
		vmi_package_bytecode_header package_header = {
			vm_string_length(&p->name),
			p->func_count,
			p->type_count,
			0,
			0
		};
		vmc_write(c, &package_header, sizeof(vmi_package_bytecode_header));
		vmc_write(c, (void*)p->name.start, vm_string_length(&p->name)); // name bytes
		p = p->next;
	}

	// Memory structure for function information:
	// int32	| name length
	// char[]	| name bytes
	// 
	// Memory structure for type information:
	// int32	| name length
	// char[]	| name bytes

	p = c->package_first;
	while (p != NULL) {
		vmc_func* f = p->func_first;
		while (f != NULL) {
			vmi_package_func_bytecode_header func_header = {
				vm_string_length(&f->name),
				f->offset
			};
			vmc_write(c, &func_header, sizeof(vmi_package_func_bytecode_header));
			vmc_write(c, (void*)f->name.start, vm_string_length(&f->name)); // name bytes
			f = f->next;
		}
		p = p->next;
	}
}

// Link
void _vmc_link(vmc_compiler* c)
{
}

// Cleanup all packages
void _vmc_compiler_destroy_packages(vmc_compiler* c)
{
	vmc_package* p = c->package_first;
	while (p != NULL) {
		vmc_package* const next = p->next;
		_vmc_package_free(p);
		p = next;
	}
	c->package_first = c->package_last = NULL;
}

//
// PUBLIC ///////////////////////////////////////////////////////////////////////////
// 
const vmc_compiler_config* vmc_compiler_config_default = &_vmc_compiler_config_default;

vmc_compiler* vmc_compiler_new(const vmc_compiler_config* config)
{
	vmc_compiler* c = (vmc_compiler*)malloc(sizeof(vmc_compiler));
	if (c == NULL)
		return c;
	if (config == NULL)
		config = vmc_compiler_config_default;
	c->config = *config;
	vm_bytestream_init(&c->bytecode);
	vm_messages_init(&c->messages);
	c->panic_error_message.code = VMC_COMPILER_MSG_NONE;
	c->package_first = c->package_last = NULL;
	c->packages_count = 0;
	c->functions_count = 0;
	vmc_string_pool_init(&c->string_pool);

	_vmc_compiler_register_builtins(c);
	vmc_package_new(c, "main", 4);
	return c;
}

void vmc_compiler_destroy(vmc_compiler* c)
{
	vmc_string_pool_destroy(&c->string_pool);
	vm_messages_destroy(&c->messages);
	_vmc_compiler_destroy_packages(c);
	vm_bytestream_release(&c->bytecode);
	free(c);
}

BOOL vmc_compiler_compile(vmc_compiler* c, const vm_byte* src)
{
	vmc_lexer l;
	vmc_lexer_init(&l, src);
	_vmc_append_header(c);
	// Main package is the second package
	_vmc_parse(c, &l, c->package_first->next);
	vm_messages_move(&l.messages, &c->messages);
	vmc_lexer_release(&l);
	if (vmc_compiler_success(c)) {
		_vmc_append_package_info(c);
		_vmc_link(c);
		// We might have gotten link errors, so let's verify success again
		return vmc_compiler_success(c);
	}
	else
		return FALSE;
}

vm_int32 vmc_compiler_config_import(struct vmc_compiler* c, const vm_string* path)
{
	return 0;
}

vmc_package* vmc_package_new(vmc_compiler* c, const char* name, int length)
{
	vmc_package* p = _vmc_package_malloc(name, length);
	if (p == NULL)
		return NULL;
	// Add the package to the linked list. Assume that vm/root package is in the beginning
	p->root_package = c->package_first;
	c->package_last->next = p;
	c->package_last = p;
	p->id = c->packages_count++;
	return p;
}

vmc_type_definition* vmc_package_find_type(vmc_package* p, const vm_string* name)
{
	vmc_type_definition* type = p->type_first;
	while (type != NULL) {
		if (vm_string_cmp(&type->name, name))
			return type;
		type = type->next;
	}
	if (p->root_package) {
		return vmc_package_find_type(p->root_package, name);
	}
	return NULL;
}

vmc_func* vmc_func_find(vmc_package* p, const vm_string* signature)
{
	vmc_func* func = p->func_first;
	while (func != NULL) {
		if (vm_string_cmp(&func->signature, signature)) {
			return func;
		}
		func = func->next;
	}
	return NULL;
}

vmc_type_definition* vmc_type_definition_new(vmc_package* p, const vm_string* name, vm_int32 size)
{
	vmc_type_definition* type = (vmc_type_definition*)malloc(sizeof(vmc_type_definition));
	if (type == NULL)
		return type;
	type->name = *name;
	type->package = p;
	type->size = size;
	type->next = NULL;
	if (p->type_last == NULL) {
		p->type_first = p->type_last = type;
	}
	else {
		p->type_last->next = type;
		p->type_last = type;
	}
	p->type_count++;
	return type;
}
