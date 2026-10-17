#pragma once
// ============================================================================
//  dupe.h — bfi (v3)   CATALOGO PERSISTENTE EM JSON
//  v3: Item ganhou 'aux' (o weaponType do gunStats, para equipar de verdade)
//      e o JSON passa a gravar "a". Log de contagem corrigido no unlock.cpp.
// ============================================================================
#include <windows.h>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>
#include <mutex>
#include <atomic>

extern HMODULE g_hModule;

namespace Dupe {

    struct Item {
        int value = 0;
        std::string name{};
        int aux = 0;
    };

    inline std::mutex        g_mx;
    inline std::vector<Item> g_weapons, g_camos, g_chars, g_attach[3];

    inline std::vector<std::string> g_wpnStr, g_camoStr, g_charStr, g_atStr[3];
    inline std::vector<const char*> g_wpnPtr, g_camoPtr, g_charPtr, g_atPtr[3];

    inline std::atomic<bool>     g_loadTried{ false };
    inline std::atomic<bool>     g_wantBuild{ false };
    inline std::atomic<bool>     g_building{ false };
    inline std::atomic<int>      g_stage{ 0 };
    inline std::atomic<uint32_t> g_epoch{ 1 };

    inline char g_status[192] = "sem arquivo - aperte DUPE COMPLETO";
    inline char g_path[600] = "";
    inline char g_stamp[40] = "";

    inline bool Path(char* out, size_t n);

    inline void SetStatus(const char* fmt, ...) {
        va_list a; va_start(a, fmt);
        vsnprintf_s(g_status, sizeof(g_status), _TRUNCATE, fmt, a);
        va_end(a);
    }
    inline const char* Status() { return g_status; }

    inline bool HasFile() {
        char p[600];
        if (!Path(p, sizeof(p))) return false;
        DWORD at = GetFileAttributesA(p);
        return at != INVALID_FILE_ATTRIBUTES && !(at & FILE_ATTRIBUTE_DIRECTORY);
    }

    inline bool Path(char* out, size_t n) {
        out[0] = 0;
        HMODULE h = g_hModule;
        if (!h) {
            GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                (LPCSTR)&Dupe::Path, &h);
        }
        if (!h) return false;
        char buf[600] = "";
        DWORD r = GetModuleFileNameA(h, buf, (DWORD)sizeof(buf));
        if (!r || r >= sizeof(buf)) return false;
        char* s = strrchr(buf, '\\');
        if (!s) return false;
        s[1] = 0;
        strcpy_s(out, n, buf);
        strcat_s(out, n, "bfi_dupe.json");
        return true;
    }

    inline void RebuildPtrs(std::vector<Item>& src, std::vector<std::string>& disp,
        std::vector<const char*>& ptr) {
        disp.clear(); ptr.clear();
        disp.reserve(src.size());
        for (size_t i = 0; i < src.size(); ++i) {
            char line[192];
            if (src[i].name.empty()) sprintf_s(line, sizeof(line), "%03d", (int)i);
            else                     sprintf_s(line, sizeof(line), "%03d - %s", (int)i, src[i].name.c_str());
            disp.push_back(line);
        }
        ptr.reserve(disp.size());
        for (size_t i = 0; i < disp.size(); ++i) ptr.push_back(disp[i].c_str());
    }

    inline void SwapIn(std::vector<Item>& tmp, std::vector<Item>& dst,
        std::vector<std::string>& disp, std::vector<const char*>& ptr) {
        std::lock_guard<std::mutex> lk(g_mx);
        dst.swap(tmp);
        RebuildPtrs(dst, disp, ptr);
        ++g_epoch;
    }

    inline void EnsureLoaded();

    inline int  WeaponCount() { EnsureLoaded(); return (int)g_wpnPtr.size(); }
    inline const char* const* WeaponList() { EnsureLoaded(); return g_wpnPtr.empty() ? nullptr : g_wpnPtr.data(); }

    inline int  CamoCount() { EnsureLoaded(); return (int)g_camoPtr.size(); }
    inline const char* const* CamoList() { EnsureLoaded(); return g_camoPtr.empty() ? nullptr : g_camoPtr.data(); }

    inline int  CharCount() { EnsureLoaded(); return (int)g_charPtr.size(); }
    inline const char* const* CharList() { EnsureLoaded(); return g_charPtr.empty() ? nullptr : g_charPtr.data(); }

    inline int  AttachCount(int s) { EnsureLoaded(); return (s >= 0 && s < 3) ? (int)g_atPtr[s].size() : 0; }
    inline const char* const* AttachList(int s) {
        EnsureLoaded();
        return (s >= 0 && s < 3 && !g_atPtr[s].empty()) ? g_atPtr[s].data() : nullptr;
    }
    inline int  AttachValue(int s, int idx) {
        EnsureLoaded();
        if (s < 0 || s > 2 || idx < 0 || idx >= (int)g_attach[s].size()) return -1;
        return g_attach[s][idx].value;
    }
    inline int  WeaponValue(int idx) {
        EnsureLoaded();
        if (idx < 0 || idx >= (int)g_weapons.size()) return idx;
        return g_weapons[idx].value;
    }
    inline const char* WeaponName(int idx) {
        EnsureLoaded();
        if (idx < 0 || idx >= (int)g_weapons.size()) return "";
        return g_weapons[idx].name.c_str();
    }
    inline int  WeaponAux(int idx) {
        EnsureLoaded();
        if (idx < 0 || idx >= (int)g_weapons.size()) return 0;
        return g_weapons[idx].aux;
    }
    inline int  CamoValue(int idx) {
        EnsureLoaded();
        if (idx < 0 || idx >= (int)g_camos.size()) return idx;
        return g_camos[idx].value;
    }

    inline void Clear() {
        std::lock_guard<std::mutex> lk(g_mx);
        g_weapons.clear(); g_camos.clear(); g_chars.clear();
        for (int i = 0; i < 3; ++i) g_attach[i].clear();
        RebuildPtrs(g_weapons, g_wpnStr, g_wpnPtr);
        RebuildPtrs(g_camos, g_camoStr, g_camoPtr);
        RebuildPtrs(g_chars, g_charStr, g_charPtr);
        for (int i = 0; i < 3; ++i) RebuildPtrs(g_attach[i], g_atStr[i], g_atPtr[i]);
        ++g_epoch;
    }

    inline void RequestBuild() {
        if (g_wantBuild.load()) return;
        g_stage.store(1);
        g_building.store(false);
        g_wantBuild.store(true);
        SetStatus("gerando...");
    }
    inline void Cancel() { g_wantBuild.store(false); g_stage.store(0); g_building.store(false); }
    inline bool Wants() { return g_wantBuild.load(); }
    inline bool Building() { return g_building.load(); }
    inline int  Stage() { return g_stage.load(); }
    inline void SetStage(int s) { g_stage.store(s); }

    inline void Esc(FILE* f, const char* s) {
        fputc('"', f);
        for (; s && *s; ++s) {
            unsigned char c = (unsigned char)*s;
            if (c == '"' || c == '\\') { fputc('\\', f); fputc((char)c, f); }
            else if (c == '\n') fputs("\\n", f);
            else if (c == '\r') fputs("\\r", f);
            else if (c == '\t') fputs("\\t", f);
            else if (c < 32)    fprintf(f, "\\u%04X", c);
            else                fputc((char)c, f);
        }
        fputc('"', f);
    }

    inline void WriteArr(FILE* f, const char* key, std::vector<Item>& v, bool last) {
        fprintf(f, "  \"%s\": [", key);
        for (size_t i = 0; i < v.size(); ++i) {
            fprintf(f, "%s\n    {\"v\": %d, \"a\": %d, \"n\": ", i ? "," : "", v[i].value, v[i].aux);
            Esc(f, v[i].name.c_str());
            fputc('}', f);
        }
        fprintf(f, "%s]\n", v.empty() ? "" : "\n  ");
        if (!last) fputc(',', f);
    }

    inline bool Save() {
        char p[600];
        if (!Path(p, sizeof(p))) { SetStatus("path da DLL falhou"); return false; }
        strcpy_s(g_path, sizeof(g_path), p);

        char tmp[640];
        sprintf_s(tmp, sizeof(tmp), "%s.tmp", p);

        FILE* f = nullptr;
        fopen_s(&f, tmp, "wb");
        if (!f) { SetStatus("nao consegui escrever em %s", tmp); return false; }

        SYSTEMTIME st{}; GetLocalTime(&st);
        sprintf_s(g_stamp, sizeof(g_stamp), "%04d-%02d-%02d %02d:%02d:%02d",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

        std::lock_guard<std::mutex> lk(g_mx);

        fputs("{\n  \"bfi\": 1,\n  \"gerado\": ", f);
        Esc(f, g_stamp);
        fputs(",\n", f);
        WriteArr(f, "armas", g_weapons, false);
        WriteArr(f, "camos", g_camos, false);
        WriteArr(f, "personagens", g_chars, false);
        WriteArr(f, "anexo_mira", g_attach[0], false);
        WriteArr(f, "anexo_acessorios", g_attach[1], false);
        WriteArr(f, "anexo_cano", g_attach[2], true);
        fputs("}\n", f);
        fclose(f);

        if (!MoveFileExA(tmp, p, MOVEFILE_REPLACE_EXISTING)) {
            DeleteFileA(tmp);
            SetStatus("MoveFileEx falhou");
            return false;
        }
        SetStatus("ok - %d armas, %d camos, %d personagens, %d anexos (%s)",
            (int)g_weapons.size(), (int)g_camos.size(), (int)g_chars.size(),
            (int)(g_attach[0].size() + g_attach[1].size() + g_attach[2].size()), g_stamp);
        return true;
    }

    inline const char* SkipWS(const char* p) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
        return p;
    }

    inline const char* ObjEnd(const char* p) {
        bool inStr = false, esc = false;
        for (; *p; ++p) {
            char c = *p;
            if (inStr) {
                if (esc) { esc = false; continue; }
                if (c == '\\') { esc = true; continue; }
                if (c == '"') inStr = false;
                continue;
            }
            if (c == '"') { inStr = true; continue; }
            if (c == '}') return p;
        }
        return nullptr;
    }

    inline bool ParseItems(const std::string& s, const char* key, std::vector<Item>& out) {
        std::string k = "\""; k += key; k += "\"";
        size_t pos = s.find(k);
        if (pos == std::string::npos) return false;
        pos = s.find('[', pos);
        if (pos == std::string::npos) return false;

        const char* p = s.c_str() + pos + 1;
        out.clear();
        for (;;) {
            p = SkipWS(p);
            if (*p == ']' || !*p) break;
            if (*p != '{') { ++p; continue; }

            const char* e = ObjEnd(p);
            if (!e) break;
            std::string obj(p, (size_t)(e - p));

            int val = 0, aux = 0;
            std::string name;

            size_t vp = obj.find("\"v\"");
            if (vp != std::string::npos) {
                size_t c = obj.find(':', vp);
                if (c != std::string::npos) val = atoi(obj.c_str() + c + 1);
            }
            size_t ap = obj.find("\"a\"");
            if (ap != std::string::npos) {
                size_t c = obj.find(':', ap);
                if (c != std::string::npos) aux = atoi(obj.c_str() + c + 1);
            }
            size_t np = obj.find("\"n\"");
            if (np != std::string::npos) {
                size_t c = obj.find(':', np);
                size_t q = (c == std::string::npos) ? std::string::npos : obj.find('"', c);
                if (q != std::string::npos) {
                    std::string raw;
                    for (size_t i = q + 1; i < obj.size(); ++i) {
                        char ch = obj[i];
                        if (ch == '\\' && i + 1 < obj.size()) {
                            char x = obj[++i];
                            raw += (x == 'n') ? '\n' : (x == 'r') ? '\r' : (x == 't') ? '\t' : x;
                        }
                        else if (ch == '"') break;
                        else raw += ch;
                    }
                    name = raw;
                }
            }
            out.push_back({ val, name, aux });
            p = e + 1;
        }
        return true;
    }

    inline bool Load() {
        char p[600];
        if (!Path(p, sizeof(p))) { SetStatus("path da DLL falhou"); return false; }
        strcpy_s(g_path, sizeof(g_path), p);

        FILE* f = nullptr;
        fopen_s(&f, p, "rb");
        if (!f) { SetStatus("bfi_dupe.json ausente - aperte DUPE COMPLETO"); Clear(); return false; }
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz <= 0 || sz > (long)(16 << 20)) { fclose(f); SetStatus("json vazio/grande demais"); return false; }

        std::string s;
        s.resize((size_t)sz);
        size_t rd = fread(&s[0], 1, (size_t)sz, f);
        fclose(f);
        s.resize(rd);

        std::vector<Item> w, c, ch, a0, a1, a2;
        ParseItems(s, "armas", w);
        ParseItems(s, "camos", c);
        ParseItems(s, "personagens", ch);
        ParseItems(s, "anexo_mira", a0);
        ParseItems(s, "anexo_acessorios", a1);
        ParseItems(s, "anexo_cano", a2);

        if (w.empty() && c.empty() && ch.empty()) {
            SetStatus("json sem conteudo - aperte DUPE COMPLETO");
            Clear();
            return false;
        }

        {
            std::lock_guard<std::mutex> lk(g_mx);
            g_weapons.swap(w); g_camos.swap(c); g_chars.swap(ch);
            g_attach[0].swap(a0); g_attach[1].swap(a1); g_attach[2].swap(a2);
            RebuildPtrs(g_weapons, g_wpnStr, g_wpnPtr);
            RebuildPtrs(g_camos, g_camoStr, g_camoPtr);
            RebuildPtrs(g_chars, g_charStr, g_charPtr);
            for (int i = 0; i < 3; ++i) RebuildPtrs(g_attach[i], g_atStr[i], g_atPtr[i]);
            ++g_epoch;
        }
        SetStatus("carregado: %d armas, %d camos, %d personagens, %d anexos",
            (int)g_weapons.size(), (int)g_camos.size(), (int)g_chars.size(),
            (int)(g_attach[0].size() + g_attach[1].size() + g_attach[2].size()));
        return true;
    }

    inline void EnsureLoaded() {
        bool exp = false;
        if (!g_loadTried.compare_exchange_strong(exp, true)) return;
        Load();
    }
    inline void RequestReload() { g_loadTried.store(false); EnsureLoaded(); }

} // namespace Dupe