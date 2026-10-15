// ============================================================================
//  log.cpp
//  Log unico do projeto. Thread-safe. LogInit() e idempotente.
// ============================================================================

#include <windows.h>
#include <cstdio>
#include <cstdarg>
#include <cstring>

extern HMODULE g_hModule;

void Log(const char* fmt, ...);

static FILE* g_log = nullptr;
static CRITICAL_SECTION g_cs;
static bool             g_csReady = false;

// ---------------------------------------------------------------------------
void LogInit() {
    if (g_log) return;                       // <-- idempotente: nao reinicializa nada

    if (!g_csReady) { InitializeCriticalSection(&g_cs); g_csReady = true; }

    char path[MAX_PATH] = { 0 };

    HMODULE mod = g_hModule ? g_hModule : GetModuleHandleA(nullptr);
    GetModuleFileNameA(mod, path, MAX_PATH);

    char* s = strrchr(path, '\\');
    if (s) s[1] = 0; else path[0] = 0;
    strncat_s(path, "BFInternal.log", _TRUNCATE);

    fopen_s(&g_log, path, "w");
    if (!g_log) return;

    Log("log aberto: %s", path);
}

// ---------------------------------------------------------------------------
void Log(const char* fmt, ...) {
    if (!g_log) return;

    if (g_csReady) EnterCriticalSection(&g_cs);

    SYSTEMTIME st;
    GetLocalTime(&st);

    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, ap);
    va_end(ap);

    fprintf(g_log, "[%02d:%02d:%02d.%03d] %s\n",
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, buf);
    fflush(g_log);

    if (g_csReady) LeaveCriticalSection(&g_cs);
}