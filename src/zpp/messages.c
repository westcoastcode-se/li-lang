#include "messages.h"
#include "compiler.h"

BOOL zpp_message_panic(const struct zpp_compiler_state* s, const char* str, zpp_message_code code)
{
	vm_message* const m = &s->compiler->panic;
	m->prefix = ZPP_MESSAGE_PREFIX;
	m->code = code;
	m->next = NULL;
	m->line = m->line_offset = m->offset = 0;
	sprintf(m->message, str);
	return FALSE;
}

BOOL zpp_message_out_of_memory(const struct zpp_compiler_state* s)
{
	return zpp_message_panic(s, "out of memory", ZPP_MESSAGE_OUT_OF_MEMORY);
}

BOOL zpp_message_not_implemented(const struct zpp_compiler_state* s)
{
	const zpp_token* const t = s->token;
	vm_messages* const m = &s->compiler->messages;

	int line, line_offset, _;
	zpp_token_get_metadata(t, &line, &line_offset, &_);
	return vm_messages_add(m,
		ZPP_MESSAGE_PREFIX,
		ZPP_MESSAGE_NOT_IMPLEMENTED,
		ZPP_MESSAGE_NOT_IMPLEMENTED_STR,
		vm_string_length(&t->string), t->string.start, line, line_offset);
}

BOOL zpp_message_expected_identifier(const struct zpp_compiler_state* s)
{
	const zpp_token* const t = s->token;
	vm_messages* const m = &s->compiler->messages;

	int line, line_offset, _;
	zpp_token_get_metadata(t, &line, &line_offset, &_);
	return vm_messages_add(m,
		ZPP_MESSAGE_PREFIX,
		ZPP_MESSAGE_EXPECTED_IDENTIFIER,
		ZPP_MESSAGE_EXPECTED_IDENTIFIER_STR,
		vm_string_length(&t->string), t->string.start, line, line_offset);
}

BOOL zpp_message_syntax_error(const struct zpp_compiler_state* s, vm_byte c)
{
	const zpp_token* const t = s->token;
	vm_messages* const m = &s->compiler->messages;

	int line, line_offset, _;
	zpp_token_get_metadata(t, &line, &line_offset, &_);
	return vm_messages_add(m,
		ZPP_MESSAGE_PREFIX,
		ZPP_MESSAGE_SYNTAX_ERROR,
		ZPP_MESSAGE_SYNTAX_ERROR_STR,
		c, vm_string_length(&t->string), t->string.start, line, line_offset);
}
