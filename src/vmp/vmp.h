#ifndef _VMC_PIPELINE_H_
#define _VMC_PIPELINE_H_

#include "../vm_config.h"
#include "../vm_message.h"
#include "../vm_bytestream.h"
#include "vmp_types.h"
#include "vmp_instr.h"
#include "vmp_list_packages.h"
#include "vmp_string_pool.h"
#include "../vm_debug.h"

struct vmp_pipeline
{
	// All packages
	vmp_list_packages packages;

	// The total size, in bytes, that the header will take in the bytecode
	vm_int32 total_header_size;

	// The total size in bytes that the bytecode for all functions will take
	vm_int32 total_body_size;

	// A string pool that can be used to create strings
	vmp_string_pool string_pool;

	// Global virtual machine package
	vmp_package* vm_package;

	// Messages raised during the build process
	vm_messages messages;

	// If a panic error has occurred, such as if the computer is out of memory
	vm_message panic_error_message;
};
typedef struct vmp_pipeline vmp_pipeline;

struct vmp_builder
{
	// The pipeline
	const vmp_pipeline* pipeline;

	// Optimization level
	int opt_level;

	// Stream where the actual data will be put
	vm_bytestream bytestream;

	// Messages raised during the build process
	vm_messages messages;

	// If a panic error has occurred, such as if the computer is out of memory
	vm_message panic_error_message;
};
typedef struct vmp_builder vmp_builder;

// Create a new virtual machine pipeline
extern vmp_pipeline* vmp_pipeline_new();

// Destroy the supplied pipeline
extern void vmp_pipeline_destroy(vmp_pipeline* p);

// Add the supplied package
extern BOOL vmp_pipeline_add_package(vmp_pipeline* p, vmp_package* pkg);

// Search for a package in the pipeline
extern vmp_package* vmp_pipeline_find_package(vmp_pipeline* p, const vm_string* name);

// Search for a package in the pipeline
extern vmp_package* vmp_pipeline_find_packagesz(vmp_pipeline* p, const char* name, vm_int32 len);

// Resolve all information, sizes and offsets for types, functions etc.
extern BOOL vmp_pipeline_resolve(vmp_pipeline* p);

// Get a string
extern const vm_string* vmp_pipeline_get_string(vmp_pipeline* p, const char* str, vm_int32 len);

// Create a new virtual machine pipeline builder
extern vmp_builder* vmp_builder_new(vmp_pipeline* p);

// Set the optimization level for the builder
extern void vmp_builder_set_opt_level(vmp_builder* b, vm_int32 opt_level);

// Compile the builder content into bytecode that the virtual machine understands
extern BOOL vmp_builder_compile_package(vmp_builder* b, const struct vmp_package* p);

// Compile the builder content into bytecode that the virtual machine understands
extern BOOL vmp_builder_compile(vmp_builder* b);

// Destroy the supplied builder and all it's internal resources
extern void vmp_builder_destroy(vmp_builder* b);

// Write bytecode to the builder
extern BOOL vmp_builder_write(vmp_builder* builder, const void* ptr, vm_int32 size);

// Reserve memory to the builder
extern BOOL vmp_builder_reserve(vmp_builder* builder, vm_int32 size);

// Fetch the bytecode generated by the builder. You are responsible for freeing the bytecode when you are done
extern vm_byte* vmp_builder_get_bytecode(vmp_builder* b);

// Check to see if the builder has compiled successfully
inline static BOOL vmp_builder_success(vmp_builder* b)
{
	return vm_messages_has_messages(&b->messages) == FALSE &&
		b->panic_error_message.code == 0;
}

#endif
