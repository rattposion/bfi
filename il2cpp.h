#pragma once
#include "common.h"

struct Il2CppClass; struct Il2CppObject; struct Il2CppString; struct Il2CppType;
struct Il2CppImage; struct Il2CppAssembly; struct Il2CppDomain; struct MethodInfo;

struct Il2CppObject { Il2CppClass* klass; void* monitor; };
struct Il2CppString : Il2CppObject { int32_t length; uint16_t chars[1]; };
struct Il2CppArray { Il2CppObject obj; void* bounds; uintptr_t max_length; };

inline uintptr_t ArrayLen(Il2CppArray* a) { return a ? a->max_length : 0; }
inline void** ArrayData(Il2CppArray* a) { return a ? (void**)((uint8_t*)a + sizeof(Il2CppArray)) : nullptr; }

struct MethodInfo {
    void* methodPointer;
    void* invoker_method;
    const char* name;
    Il2CppClass* klass;
    const Il2CppType* return_type;
    const Il2CppType** parameters;
};

namespace IL2CPP {
    extern Il2CppDomain* (*domain_get)();
    extern Il2CppAssembly** (*domain_get_assemblies)(Il2CppDomain*, size_t*);
    extern Il2CppImage* (*assembly_get_image)(Il2CppAssembly*);
    extern Il2CppClass* (*class_from_name)(Il2CppImage*, const char*, const char*);
    extern Il2CppClass* (*class_from_type)(const Il2CppType*);
    extern Il2CppClass* (*class_get_parent)(Il2CppClass*);
    extern const Il2CppType* (*class_get_type)(Il2CppClass*);
    extern Il2CppObject* (*type_get_object)(const Il2CppType*);
    extern const char* (*class_get_name)(Il2CppClass*);
    extern const char* (*class_get_namespace)(Il2CppClass*);
    extern void* (*class_get_field_from_name)(Il2CppClass*, const char*);
    extern void* (*class_get_fields)(Il2CppClass*, void**);
    extern const char* (*field_get_name)(void*);
    extern const Il2CppType* (*field_get_type)(void*);
    extern size_t(*field_get_offset)(void*);
    extern void (*field_static_get_value)(void*, void*);
    extern const MethodInfo* (*class_get_method_from_name)(Il2CppClass*, const char*, int);
    extern const MethodInfo* (*class_get_methods)(Il2CppClass*, void**);
    extern uint32_t(*method_get_param_count)(const MethodInfo*);
    extern const char* (*method_get_name)(const MethodInfo*);
    extern const Il2CppType* (*method_get_param)(const MethodInfo*, uint32_t);
    extern const Il2CppType* (*method_get_return_type)(const MethodInfo*);
    extern const char* (*type_get_name)(const Il2CppType*);
    extern Il2CppObject* (*runtime_invoke)(const MethodInfo*, void*, void**, Il2CppObject**);
    extern Il2CppDomain* (*thread_attach)(Il2CppDomain*);
    extern void* (*object_unbox)(Il2CppObject*);
    extern Il2CppString* (*string_new)(const char*);

    bool Init();
    std::string ToStr(Il2CppString* s);
    const MethodInfo* FindOverload(Il2CppClass* cls, const char* name, std::initializer_list<const char*> types);
    const MethodInfo* FindMethodByName(Il2CppClass* cls, const char* name, int paramCount);
    void DumpMethods(Il2CppClass* c, const char* name);
    void DumpMethodsLike(Il2CppClass* c, const char* sub);
    void DumpFields(Il2CppClass* c, const char* label);
    Il2CppObject* Invoke(const MethodInfo* m, void* obj, void** args);
}