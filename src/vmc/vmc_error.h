#ifndef _VMC_ERROR_H_
#define _VMC_ERROR_H_

#include "../vm_message.h"

enum vmc_error_code
{
	VMC_ERR_NO_ERROR = 0,

	VMC_CERR_START,
	VMC_CERR_SYMBOL_ALREADY_EXIST,
	VMC_CERR_INVALID_ARG,
	VMC_CERR_EXPECTED_COMMA,
	VMC_CERR_OUT_OF_MEMORY,

	VMC_LERR_START,
	VMC_LERR_MISSING_LABEL,

	// Lexer errors

	// Lexer warnings
	VMC_WARN_CODE_ESCAPE_RUNE_UNKNOWN,
	VMC_WARN_CODE_UNCLOSED_STRING,
	VMC_WARN_CODE_UNCLOSED_COMMENT,

};
typedef enum vmc_error_code vmc_error_code;

#endif
