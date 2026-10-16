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
    FieldInfo* (*class_get_field_from_name)(Il2CppClass*, const char*);
    FieldInfo* (*class_get_fields)(Il2CppClass*, void**);
    const char* (*field_get_name)(FieldInfo*);
    const Il2CppType* (*field_get_type)(FieldInfo*);
    size_t(*field_get_offset)(FieldInfo*);
    void           (*field_static_get_value)(FieldInfo*, void*);
    const MethodInfo* (*class_get_method_from_name)(Il2CppClass*, const char*, int);
    const MethodInfo* (*class_get_methods)(Il2CppClass*, void**);
    uint32_t(*method_get_param_count)(const MethodInfo*);
    const char* (*method_get_name)(const MethodInfo*);
    const Il2CppType* (*method_get_param)(const MethodInfo*, uint32_t);
    const Il2CppType* (*method_get_return_type)(const MethodInfo*);
    char* (*type_get_name)(const Il2CppType*);
    Il2CppObject* (*runtime_invoke)(const MethodInfo*, void*, void**, Il2CppException**);
    Il2CppDomain* (*thread_attach)(Il2CppDomain*);
    void* (*object_unbox)(Il2CppObject*);
    Il2CppString* (*string_new)(const char*);

    static thread_local bool t_attached = false;

    static void EnsureAttached() {
        if (t_attached) return;
        t_attached = true;
        if (domain_get && thread_attach) thread_attach(domain_get());
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

        // >>> FIX: o nome do export varia entre versoes do Unity. Tenta os DOIS,
        //     senao este ponteiro fica nulo e o codigo que depende dele morre calado.
        class_from_type = (decltype(class_from_type))GetProcAddress(ga, "il2cpp_class_from_type");
        if (!class_from_type)
            class_from_type = (decltype(class_from_type))GetProcAddress(ga, "il2cpp_class_from_il2cpp_type");

        bool ok = domain_get && domain_get_assemblies && assembly_get_image && class_from_name
            && runtime_invoke && class_get_field_from_name && field_get_offset
            && class_get_methods && method_get_name && method_get_param
            && method_get_return_type && object_unbox && thread_attach
            && class_get_parent && class_get_type && type_get_object && class_get_fields
            && class_from_type && field_get_type && field_get_name && type_get_name;

        Log("il2cpp init: %s (class_from_type=%p)", ok ? "OK" : "FALHOU", class_from_type);
        if (ok) EnsureAttached();
        return ok;
    }

    // ------------------------------------------------------------------ strings
    void StrToUtf8(Il2CppObject* s, char* out, int outSize) {
        if (outSize < 2) return;
        out[0] = 0;
        if (!s) return;
        __try {
            int len = *(int*)((uintptr_t)s + 0x10);
            if (len <= 0) return;
            if (len > outSize - 1) len = outSize - 1;
            const wchar_t* src = (const wchar_t*)((uintptr_t)s + 0x14);
            int n = WideCharToMultiByte(CP_UTF8, 0, src, len, out, outSize - 1, nullptr, nullptr);
            out[n > 0 ? n : 0] = 0;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { out[0] = 0; }
    }

    std::string ToStr(Il2CppString* s) {
        if (!s) return "";
        char buf[256];
        StrToUtf8((Il2CppObject*)s, buf, sizeof(buf));
        return std::string(buf);
    }

    // ------------------------------------------------------------------ invoke
    Il2CppObject* Invoke(const MethodInfo* m, void* obj, void** args) {
        if (!m || !m->methodPointer) return nullptr;
        EnsureAttached();
        Il2CppException* exc = nullptr;
        __try {
            Il2CppObject* r = runtime_invoke(m, obj, args, &exc);
            return exc ? nullptr : r;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
    }

    // ------------------------------------------------------------------ search
    const MethodInfo* FindOverload(Il2CppClass* cls, const char* name,
        std::initializer_list<const char*> types) {
        if (!cls || !class_get_methods || !method_get_name) return nullptr;
        void* it = nullptr;
        while (const MethodInfo* m = class_get_methods(cls, &it)) {
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
        while (const MethodInfo* m = class_get_methods(cls, &it)) {
            const char* mn = method_get_name(m);
            if (!mn || strcmp(mn, name)) continue;
            uint32_t pc = method_get_param_count ? method_get_param_count(m) : 0;
            if (pc == (uint32_t)paramCount) return m;
        }
        return nullptr;
    }

    Il2CppClass* FindClassAny(const char* ns, const char* name) {
        if (!domain_get || !domain_get_assemblies) return nullptr;
        const Il2CppDomain* dom = domain_get();
        if (!dom) return nullptr;
        size_t count = 0;
        Il2CppAssembly** asms = domain_get_assemblies((Il2CppDomain*)dom, &count);
        if (!asms) return nullptr;
        for (size_t i = 0; i < count; ++i) {
            Il2CppImage* img = assembly_get_image(asms[i]);
            if (!img) continue;
            Il2CppClass* k = class_from_name(img, ns, name);
            if (k) return k;
        }
        return nullptr;
    }

    uintptr_t OffsetOf(Il2CppClass* klass, const char* fname) {
        if (!klass) return 0;
        FieldInfo* f = class_get_field_from_name(klass, fname);
        return f ? (uintptr_t)field_get_offset(f) : 0;
    }

    Il2CppClass* ClassOfField(Il2CppClass* owner, const char* fname) {
        if (!owner) return nullptr;
        FieldInfo* f = class_get_field_from_name(owner, fname);
        if (!f) return nullptr;
        const Il2CppType* t = field_get_type(f);
        return t ? class_from_type(t) : nullptr;
    }

    void* StaticFieldPtr(Il2CppClass* klass, const char* name) {
        if (!klass || !class_get_field_from_name || !field_static_get_value) return nullptr;
        FieldInfo* f = class_get_field_from_name(klass, name);
        if (!f) return nullptr;
        void* out = nullptr;
        field_static_get_value(f, &out);
        return out;
    }

    const char* FieldTypeName(Il2CppClass* klass, const char* fname) {
        if (!klass) return nullptr;
        FieldInfo* f = class_get_field_from_name(klass, fname);
        if (!f) return nullptr;
        const Il2CppType* t = field_get_type(f);
        if (!t || !type_get_name) return nullptr;
        __try { return type_get_name(t); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
    }

    bool FieldIs(Il2CppClass* klass, const char* fname, const char* typeSubstr) {
        const char* tn = FieldTypeName(klass, fname);
        return tn && strstr(tn, typeSubstr) != nullptr;
    }

    // ------------------------------------------------------------------- dumps
    void DumpMethods(Il2CppClass* c, const char* name) {
        if (!c) { Log("  dump %s: SEM CLASSE", name); return; }
        void* it = nullptr;
        int found = 0;
        while (const MethodInfo* m = class_get_methods(c, &it)) {
            const char* mn = method_get_name(m);
            if (!mn || strcmp(mn, name)) continue;
            char buf[256] = { 0 };
            uint32_t pc = method_get_param_count(m);
            for (uint32_t i = 0; i < pc && i < 5; i++) {
                const Il2CppType* pt = method_get_param(m, i);
                const char* tn = pt ? type_get_name(pt) : "?";
                // >>> FIX: strcat_s com 2 args usa sizeof(buf); mas com 5 tipos
                //     longos dava para estourar. Checa antes.
                if (strlen(buf) + strlen(tn ? tn : "?") + 3 >= sizeof(buf)) break;
                strcat_s(buf, tn ? tn : "?");
                strcat_s(buf, ", ");
            }
            Log("  %s(%s)", mn, buf);
            found++;
        }
        if (!found) Log("  %s: nenhuma sobrecarga", name);
    }

    void DumpMethodsLike(Il2CppClass* c, const char* sub) {
        if (!c) return;
        void* it = nullptr;
        int n = 0;
        while (const MethodInfo* m = class_get_methods(c, &it)) {
            const char* mn = method_get_name(m);
            if (!mn || !strstr(mn, sub)) continue;
            Log("  ~%s: %s(%u)", sub, mn, method_get_param_count(m));
            if (++n > 30) break;
        }
        if (!n) Log("  ~%s: nada", sub);
    }

    void DumpFields(Il2CppClass* c, const char* label) {
        if (!c) { Log("  [%s] SEM CLASSE", label); return; }
        void* it = nullptr;
        int n = 0;
        while (FieldInfo* f = class_get_fields(c, &it)) {
            const char* fn = field_get_name ? field_get_name(f) : "?";
            const Il2CppType* ft = field_get_type ? field_get_type(f) : nullptr;
            const char* tn = (ft && type_get_name) ? type_get_name(ft) : "?";
            Log("  [%s] %s : %s @0x%X", label, fn ? fn : "?", tn ? tn : "?", (unsigned)field_get_offset(f));
            if (++n > 600) break;
        }
        Log("  [%s] total %d campos", label, n);
    }
}