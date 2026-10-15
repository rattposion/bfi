// ============================================================================
//  il2cpp_api.h
//  Declaracoes tipadas (extern "C") da API IL2CPP.
//  Definicoes em sdk.cpp, preenchidas por IL2CPP_API::Resolve().
//
//  Params de entrada usam "const T*" -> aceitam T* e const T* sem erro.
// ============================================================================
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "il2cpp.h"

extern "C" {

	// --- dominio / assemblies / imagens ---
	extern Il2CppDomain* (*il2cpp_domain_get)();
	extern const Il2CppAssembly** (*il2cpp_domain_get_assemblies)(const Il2CppDomain*, size_t*);
	extern Il2CppImage* (*il2cpp_assembly_get_image)(const Il2CppAssembly*);

	extern size_t(*il2cpp_image_get_class_count)(const Il2CppImage*);
	extern Il2CppClass* (*il2cpp_image_get_class)(const Il2CppImage*, size_t);

	// --- classes ---
	extern const char* (*il2cpp_class_get_name)(const Il2CppClass*);
	extern const char* (*il2cpp_class_get_namespace)(const Il2CppClass*);
	extern Il2CppClass* (*il2cpp_class_get_parent)(const Il2CppClass*);
	extern Il2CppClass* (*il2cpp_class_get_declaring_type)(const Il2CppClass*);
	extern Il2CppClass* (*il2cpp_class_get_element_class)(const Il2CppClass*);
	extern Il2CppClass* (*il2cpp_class_get_interfaces)(const Il2CppClass*, void**);
	extern Il2CppImage* (*il2cpp_class_get_image)(const Il2CppClass*);
	extern Il2CppType* (*il2cpp_class_get_type)(const Il2CppClass*);
	extern Il2CppClass* (*il2cpp_class_from_type)(const Il2CppType*);
	extern uint32_t(*il2cpp_class_get_flags)(const Il2CppClass*);
	extern int32_t(*il2cpp_class_instance_size)(const Il2CppClass*);
	extern bool                   (*il2cpp_class_is_enum)(const Il2CppClass*);

	// --- membros ---
	extern FieldInfo* (*il2cpp_class_get_field_from_name)(const Il2CppClass*, const char*);
	extern FieldInfo* (*il2cpp_class_get_fields)(const Il2CppClass*, void**);
	extern MethodInfo* (*il2cpp_class_get_method_from_name)(const Il2CppClass*, const char*, int);
	extern const MethodInfo* (*il2cpp_class_get_methods)(const Il2CppClass*, void**);

	extern size_t(*il2cpp_field_get_offset)(const FieldInfo*);
	extern const char* (*il2cpp_field_get_name)(const FieldInfo*);
	extern Il2CppType* (*il2cpp_field_get_type)(const FieldInfo*);
	extern void                   (*il2cpp_field_static_get_value)(const FieldInfo*, void*);
	extern void                   (*il2cpp_field_get_value)(const Il2CppObject*, const FieldInfo*, void*);
	extern void                   (*il2cpp_field_set_value)(const Il2CppObject*, const FieldInfo*, void*);
	extern Il2CppObject* (*il2cpp_field_get_value_object)(const FieldInfo*, const Il2CppObject*);

	extern const char* (*il2cpp_method_get_name)(const MethodInfo*);
	extern uint32_t(*il2cpp_method_get_param_count)(const MethodInfo*);
	extern const Il2CppType* (*il2cpp_method_get_param)(const MethodInfo*, uint32_t);
	extern Il2CppType* (*il2cpp_method_get_return_type)(const MethodInfo*);
	extern MethodInfo* (*il2cpp_object_get_virtual_method)(const Il2CppObject*, const MethodInfo*);

	// --- tipos ---
	extern char* (*il2cpp_type_get_name)(const Il2CppType*);
	extern Il2CppObject* (*il2cpp_type_get_object)(const Il2CppType*);

	// --- objetos ---
	extern Il2CppClass* (*il2cpp_object_get_class)(const Il2CppObject*);
	extern Il2CppObject* (*il2cpp_object_new)(const Il2CppClass*);
	extern void* (*il2cpp_object_unbox)(const Il2CppObject*);

	extern Il2CppObject* (*il2cpp_runtime_invoke)(const MethodInfo*, void*, void**, Il2CppException**);

	// --- strings ---
	extern Il2CppString* (*il2cpp_string_new)(const char*);
	extern const char* (*il2cpp_string_chars)(const Il2CppString*);
	extern int32_t(*il2cpp_string_length)(const Il2CppString*);

	// --- arrays ---
	extern uint32_t(*il2cpp_array_length)(const Il2CppArray*);
	extern Il2CppArray* (*il2cpp_array_new)(const Il2CppClass*, size_t);
	extern int32_t(*il2cpp_array_element_size)(const Il2CppClass*);

	// --- thread ---
	extern Il2CppDomain* (*il2cpp_thread_attach)(const Il2CppDomain*);

} // extern "C"

namespace IL2CPP_API { bool Resolve(); }