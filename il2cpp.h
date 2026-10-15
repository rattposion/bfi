#pragma once
#include "common.h"

// ---- layouts (Unity 2019+ / IL2CPP x64) ------------------------------------
struct Il2CppClass; struct Il2CppObject; struct Il2CppString; struct Il2CppType;
struct Il2CppImage; struct Il2CppAssembly; struct Il2CppDomain; struct MethodInfo;
struct FieldInfo; struct Il2CppThread; struct Il2CppException;

struct Il2CppObject { Il2CppClass* klass; void* monitor; };
struct Il2CppString : Il2CppObject { int32_t length; uint16_t chars[1]; };
struct Il2CppArray : Il2CppObject { void* bounds; uintptr_t max_length; };

struct MethodInfo {
    void* methodPointer;   // +0x00
    void* invoker_method;  // +0x08
    const char* name;            // +0x10
    Il2CppClass* klass;           // +0x18
    const Il2CppType* return_type;     // +0x20
    const Il2CppType** parameters;      // +0x28
};

// il2cpp array layout: [klass][monitor][bounds][length] + elements
inline size_t ArrayLen(Il2CppObject* a) { return a ? *(size_t*)((uintptr_t)a + 0x18) : 0; }
inline void** ArrayData(Il2CppObject* a) { return a ? (void**)((uintptr_t)a + 0x20) : nullptr; }

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
    extern FieldInfo* (*class_get_field_from_name)(Il2CppClass*, const char*);
    extern FieldInfo* (*class_get_fields)(Il2CppClass*, void**);
    extern const char* (*field_get_name)(FieldInfo*);
    extern const Il2CppType* (*field_get_type)(FieldInfo*);
    extern size_t(*field_get_offset)(FieldInfo*);
    extern void           (*field_static_get_value)(FieldInfo*, void*);
    extern const MethodInfo* (*class_get_method_from_name)(Il2CppClass*, const char*, int);
    extern const MethodInfo* (*class_get_methods)(Il2CppClass*, void**);
    extern uint32_t(*method_get_param_count)(const MethodInfo*);
    extern const char* (*method_get_name)(const MethodInfo*);
    extern const Il2CppType* (*method_get_param)(const MethodInfo*, uint32_t);
    extern const Il2CppType* (*method_get_return_type)(const MethodInfo*);
    extern char* (*type_get_name)(const Il2CppType*);
    extern Il2CppObject* (*runtime_invoke)(const MethodInfo*, void*, void**, Il2CppException**);
    extern Il2CppDomain* (*thread_attach)(Il2CppDomain*);
    extern void* (*object_unbox)(Il2CppObject*);
    extern Il2CppString* (*string_new)(const char*);

    bool Init();

    std::string ToStr(Il2CppString* s);
    void        StrToUtf8(Il2CppObject* s, char* out, int outSize);

    const MethodInfo* FindOverload(Il2CppClass* cls, const char* name,
        std::initializer_list<const char*> types);
    const MethodInfo* FindMethodByName(Il2CppClass* cls, const char* name, int paramCount);

    void DumpMethods(Il2CppClass* c, const char* name);
    void DumpMethodsLike(Il2CppClass* c, const char* sub);
    void DumpFields(Il2CppClass* c, const char* label);

    // exception-swallowing invoke: retorna nullptr se o metodo gerenciado lancar
    Il2CppObject* Invoke(const MethodInfo* m, void* obj, void** args);

    // helpers
    Il2CppClass* FindClassAny(const char* ns, const char* name);
    uintptr_t      OffsetOf(Il2CppClass* klass, const char* field);
    Il2CppClass* ClassOfField(Il2CppClass* owner, const char* field);
    void* StaticFieldPtr(Il2CppClass* klass, const char* name);
    const char* FieldTypeName(Il2CppClass* klass, const char* field);   // NULL se nao existe
    bool           FieldIs(Il2CppClass* klass, const char* field, const char* typeSubstr);
}

template <typename T> inline T& field(void* obj, uintptr_t off) {
    return *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(obj) + off);
}