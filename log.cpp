#include "common.h"

static FILE* g_log = nullptr;
static std::mutex g_mx;

static std::string Dir() {
    char p[MAX_PATH]{};
    GetModuleFileNameA(g_hModule, p, MAX_PATH);
    std::string s(p);
    size_t k = s.find_last_of("\\/");
    return k == std::string::npos ? std::string(".") : s.substr(0, k);
}

void LogInit() {
    std::string tries[3] = { Dir() + "\\BFInternal.log", "C:\\BFInternal.log", "C:\\Windows\\Temp\\BFInternal.log" };
    for (auto& t : tries) {
        fopen_s(&g_log, t.c_str(), "w");
        if (g_log) { Log("log aberto: %s", t.c_str()); return; }
    }
}

void Log(const char* fmt, ...) {
    if (!g_log) return;
    std::lock_guard<std::mutex> lk(g_mx);
    char b[2048];
    va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof(b), fmt, ap); va_end(ap);
    SYSTEMTIME st; GetLocalTime(&st);
    fprintf(g_log, "[%02d:%02d:%02d.%03d] %s\n", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, b);
    fflush(g_log);
}