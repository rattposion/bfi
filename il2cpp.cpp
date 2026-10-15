#include "il2cpp.h"

namespace IL2CPP {
    Il2CppDomain* (*domain_get)();
    Il2CppAssembly** (*domain_get_assemblies)(Il2CppDomain*, size_t*);
    Il2CppImage* (*assembly_get_image)(Il2CppAssembly*);
    Il2CppClass* (*class_from_name)(Il2CppImage*, const char*, const char*);
    Il2CppClass* (*class_from_type)(const Il2CppType*);
    Il2CppClass* (*class_get_parent)(Il2CppClass*);
    const Il2CppType* (*class_get_type)(Il2CppClass*);
    Il2CppObject* (*type_get_object)(const Il2CppType*);
    const char* (*class_get_name)(Il2CppClass*);
    const char* (*class_get_namespace)(Il2CppClass*);
    void* (*class_get_field_from_name)(Il2CppClass*, const char*);
    void* (*class_get_fields)(Il2CppClass*, void**);
    const char* (*field_get_name)(void*);
    const Il2CppType* (*field_get_type)(void*);
    size_t(*field_get_offset)(void*);
    void (*field_static_get_value)(void*, void*);
    const MethodInfo* (*class_get_method_from_name)(Il2CppClass*, const char*, int);
    const MethodInfo* (*class_get_methods)(Il2CppClass*, void**);
    uint32_t(*method_get_param_count)(const MethodInfo*);
    const char* (*method_get_name)(const MethodInfo*);
    const Il2CppType* (*method_get_param)(const MethodInfo*, uint32_t);
    const Il2CppType* (*method_get_return_type)(const MethodInfo*);
    const char* (*type_get_name)(const Il2CppType*);
    Il2CppObject* (*runtime_invoke)(const MethodInfo*, void*, void**, Il2CppObject**);
    Il2CppDomain* (*thread_attach)(Il2CppDomain*);
    void* (*object_unbox)(Il2CppObject*);
    Il2CppString* (*string_new)(const char*);

    static thread_local bool t_attached = false;

    static void EnsureAttached() {
        if (t_attached) return;
        if (domain_get && thread_attach) thread_attach(domain_get());
        t_attached = true;
    }

    bool Init() {
        HMODULE ga = GetModuleHandleA("GameAssembly.dll");
        if (!ga) { Log("GameAssembly.dll nao carregada"); return false; }
#define R(f) f = (decltype(f))GetProcAddress(ga, "il2cpp_" #f)
        R(domain_get); R(domain_get_assemblies); R(assembly_get_image); R(class_from_name);
        R(class_get_parent); R(class_get_type); R(type_get_object);
        R(class_get_name); R(class_get_namespace); R(class_get_field_from_name);
        R(class_get_fields); R(field_get_name); R(field_get_type);
        R(field_get_offset); R(field_static_get_value); R(class_get_method_from_name);
        R(class_get_methods); R(method_get_param_count); R(method_get_name);
        R(method_get_param); R(method_get_return_type); R(type_get_name);
        R(runtime_invoke); R(thread_attach); R(object_unbox); R(string_new);
#undef R
        class_from_type = (decltype(class_from_type))GetProcAddress(ga, "il2cpp_class_from_il2cpp_type");

        bool ok = domain_get && class_from_name && runtime_invoke && class_get_field_from_name
            && class_get_methods && method_get_name && method_get_param
            && method_get_return_type && object_unbox && thread_attach
            && class_get_parent && class_get_type && type_get_object && class_get_fields
            && class_from_type && field_get_type && field_get_name;
        Log("il2cpp init: %s", ok ? "OK" : "FALHOU");
        if (ok) EnsureAttached();
        return ok;
    }

    std::string ToStr(Il2CppString* s) {
        if (!s || s->length <= 0 || s->length > 512) return "";
        std::string o; o.reserve(s->length);
        for (int i = 0; i < s->length; i++) o.push_back((char)(s->chars[i] & 0xFF));
        return o;
    }

    Il2CppObject* Invoke(const MethodInfo* m, void* obj, void** args) {
        if (!m || !m->methodPointer) return nullptr;
        EnsureAttached();
        Il2CppObject* exc = nullptr;
        __try { return runtime_invoke(m, obj, args, &exc); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
    }

    const MethodInfo* FindOverload(Il2CppClass* cls, const char* name, std::initializer_list<const char*> types) {
        if (!cls || !class_get_methods || !method_get_name) return nullptr;
        void* it = nullptr;
        const MethodInfo* m = nullptr;
        while ((m = class_get_methods(cls, &it))) {
            const char* mn = method_get_name(m);
            if (!mn || strcmp(mn, name)) continue;
            uint32_t pc = method_get_param_count ? method_get_param_count(m) : 0;
            if (pc != (uint32_t)types.size()) continue;
            if (types.size() == 0) return m;
            bool ok = true; uint32_t i = 0;
            for (auto t : types) {
                const Il2CppType* pt = method_get_param ? method_get_param(m, i++) : nullptr;
                const char* tn = (pt && type_get_name) ? type_get_name(pt) : nullptr;
                if (!tn || !strstr(tn, t)) { ok = false; break; }
            }
            if (ok) return m;
        }
        return nullptr;
    }

    const MethodInfo* FindMethodByName(Il2CppClass* cls, const char* name, int paramCount) {
        if (!cls || !class_get_methods || !method_get_name) return nullptr;
        void* it = nullptr;
        const MethodInfo* m = nullptr;
        while ((m = class_get_methods(cls, &it))) {
            const char* mn = method_get_name(m);
            if (!mn || strcmp(mn, name)) continue;
            uint32_t pc = method_get_param_count ? method_get_param_count(m) : 0;
            if (pc == (uint32_t)paramCount) return m;
        }
        return nullptr;
    }

    void DumpMethods(Il2CppClass* c, const char* name) {
        if (!c) { Log("  dump %s: SEM CLASSE", name); return; }
        void* it = nullptr;
        const MethodInfo* m = nullptr;
        int found = 0;
        while ((m = class_get_methods(c, &it))) {
            const char* mn = method_get_name(m);
            if (!mn || strcmp(mn, name)) continue;
            char buf[256] = { 0 };
            uint32_t pc = method_get_param_count(m);
            for (uint32_t i = 0; i < pc && i < 5; i++) {
                const Il2CppType* pt = method_get_param(m, i);
                const char* tn = pt ? type_get_name(pt) : "?";
                strcat_s(buf, tn ? tn : "?"); strcat_s(buf, ", ");
            }
            Log("  %s(%s)", mn, buf);
            found++;
        }
        if (!found) Log("  %s: nenhuma sobrecarga", name);
    }

    void DumpMethodsLike(Il2CppClass* c, const char* sub) {
        if (!c) return;
        void* it = nullptr;
        const MethodInfo* m = nullptr;
        int n = 0;
        while ((m = class_get_methods(c, &it))) {
            const char* mn = method_get_name(m);
            if (!mn || !strstr(mn, sub)) continue;
            Log("  ~%s: %s(%u)", sub, mn, method_get_param_count(m));
            if (++n > 25) break;
        }
        if (!n) Log("  ~%s: nada", sub);
    }

    void DumpFields(Il2CppClass* c, const char* label) {
        if (!c) { Log("  [%s] SEM CLASSE", label); return; }
        void* it = nullptr;
        void* f = nullptr;
        int n = 0;
        while ((f = class_get_fields(c, &it))) {
            const char* fn = field_get_name ? field_get_name(f) : "?";
            const Il2CppType* ft = field_get_type ? field_get_type(f) : nullptr;
            const char* tn = (ft && type_get_name) ? type_get_name(ft) : "?";
            Log("  [%s] %s : %s", label, fn ? fn : "?", tn ? tn : "?");
            if (++n > 250) break;
        }
        Log("  [%s] total %d campos", label, n);
    }
}