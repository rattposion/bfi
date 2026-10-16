// ============================================================================
//  aimbot.cpp — bfi  (v8)
//
//  >>> FIX v8:
//   1) struct Cand com inicializadores -> C26495 'Cand::obj' / 'Cand::score'
//      nao inicializados (linha 180).
//   2) O Log do probeLog tinha 7 formatos e 6 argumentos: 'dist' nem existia,
//      entao 'ang' era impresso como dist, o onScreen (int) caia no %.1f e
//      todo o resto saia deslocado. Agora calcula dist e passa na ordem.
//
//  >>> SELECAO DE ALVO (o que voce pediu):
//  C.aimVisCheck LIGADO  -> so visivel. Ninguem visivel = nao gira a camera.
//  C.aimVisCheck DESLIGADO -> mira atraves da parede.
//  C.aimStrictVis = false -> se ninguem estiver visivel, cai no mais perto.
//  C.aimStrictVis = true  -> se ninguem estiver visivel, nao mira em ninguem.
//
//  >>> '::Quat' explicito em tudo, chamadas internas qualificadas
//      (Aimbot::LookRot) porque o sdk.h faz 'using SDK::LookRot;' global.
//
//  >>> BUG ANTIGO (mantido corrigido): LocalPos() procurava o local player em
//      SDK::g_targets, mas RefreshTargets() EXCLUI o local -> retornava (0,0,0)
//      e o aimbot mirava a partir da ORIGEM DO MUNDO.
// ============================================================================
#include "common.h"
#include "sdk.h"
#include "config.h"
#include "features.h"
#include <cmath>

namespace Aimbot {

    static_assert(sizeof(::Quat) == 16, "Quat deve ter 4 floats");

    static Il2CppObject* g_best = nullptr;
    static Il2CppObject* g_sticky = nullptr;
    static bool          g_rotProbed = false;
    static bool          g_writePlayerRot = true;
    static uint64_t      g_lastAimLog = 0;

    // ---------------------------------------------------------------- vetores
    Vec3 Sub(Vec3 a, Vec3 b) { return Vec3{ a.x - b.x, a.y - b.y, a.z - b.z }; }
    float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
    static float Len(Vec3 v) { return sqrtf(Aimbot::Dot(v, v)); }
    Vec3 Norm(Vec3 v) { float l = Len(v); if (l > 1e-6f) { v.x /= l; v.y /= l; v.z /= l; } return v; }

    static bool PlausibleQ(const ::Quat& q) {
        float n = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
        return n > 0.25f && n < 2.25f;
    }

    static Vec3 FwdOfQ(const ::Quat& q) {
        return Aimbot::Norm(Vec3{
            2.f * (q.x * q.z + q.w * q.y),
            2.f * (q.y * q.z - q.w * q.x),
            1.f - 2.f * (q.x * q.x + q.y * q.y)
            });
    }

    // Unity YXZ (yaw em Y, pitch em X). Sem roll -> correto pra LookRotation.
    ::Quat LookRot(Vec3 dir)
    {
        float yaw = atan2f(dir.x, dir.z);
        float pitch = -asinf(dir.y);
        float sy = sinf(yaw * 0.5f), cy = cosf(yaw * 0.5f);
        float sp = sinf(pitch * 0.5f), cp = cosf(pitch * 0.5f);
        return ::Quat{ cy * sp, sy * cp, -sy * sp, cy * cp };
    }

    ::Quat Slerp(::Quat a, ::Quat b, float t)
    {
        float d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
        if (d < 0) { b.x = -b.x; b.y = -b.y; b.z = -b.z; b.w = -b.w; d = -d; }
        if (d > 0.9995f) {
            ::Quat r{ a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                      a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t };
            float l = sqrtf(r.x * r.x + r.y * r.y + r.z * r.z + r.w * r.w);
            if (l > 1e-6f) { r.x /= l; r.y /= l; r.z /= l; r.w /= l; }
            return r;
        }
        float th = acosf(d), s = sinf(th);
        if (fabsf(s) < 1e-6f) return b;
        float wa = sinf((1 - t) * th) / s, wb = sinf(t * th) / s;
        return ::Quat{ a.x * wa + b.x * wb, a.y * wa + b.y * wb,
                       a.z * wa + b.z * wb, a.w * wa + b.w * wb };
    }

    // =========================================================================
    //  posicao / forward do OLHO (camTf), com fallback pro player
    // =========================================================================
    static Vec3 EyePos()
    {
        if (!SDK::g_localPlayer) return Vec3{};

        if (Il2CppObject* ct = SDK::CamTfObj()) {
            Vec3 p = SDK::GetPos(ct);
            if (!p.zero()) return p;
        }
        Vec3 p = SDK::GetPos(SDK::g_localPlayer);
        if (p.zero()) return SDK::g_localPos;
        return Vec3{ p.x, p.y + 1.55f, p.z };
    }

    Vec3 LocalPos() { return EyePos(); }

    Vec3 CamForward()
    {
        if (Il2CppObject* ct = SDK::CamTfObj()) {
            ::Quat q{};
            if (SDK::GetRot(ct, q) && PlausibleQ(q)) return FwdOfQ(q);
        }
        if (SDK::g_localPlayer) {
            ::Quat q = SDK::GetPlayerRot(SDK::g_localPlayer);
            if (PlausibleQ(q)) return FwdOfQ(q);
        }
        return Vec3{ 0.f, 0.f, 1.f };
    }

    static Vec3 AimPointOf(const Target& t)
    {
        Vec3 h = t.head;
        if (t.headBone) { Vec3 b = SDK::GetPos(t.headBone); if (!b.zero()) h = b; }
        Vec3 f = t.feet;
        if (t.footBone) { Vec3 b = SDK::GetPos(t.footBone); if (!b.zero()) f = b; }

        if (C.aimBone == 0) return h;
        if (C.aimBone == 2) return Vec3(h.x, (h.y + f.y) * 0.5f, h.z);
        return Vec3(h.x, h.y - (h.y - f.y) * 0.25f, h.z);
    }

    Vec3 AimPoint(void* obj)
    {
        for (int i = 0; i < SDK::g_targetCount; ++i) {
            if (SDK::g_targets[i].obj != obj) continue;
            Vec3 a = AimPointOf(SDK::g_targets[i]);
            if (!a.zero()) return a;
        }
        return Vec3{};
    }

    static bool KeyDown() { return C.aimKey ? (GetAsyncKeyState(C.aimKey) & 0x8000) != 0 : false; }

    // =========================================================================
    //  score: <0 = descartado. Menor = mais perto da mira.
    // =========================================================================
    static float ScoreTarget(const Target& t, Vec3 eye, Vec3 fwd,
        float radius, bool screenMode, float cx, float cy)
    {
        if (!t.player || !t.alive) return -1.f;
        if (t.obj == SDK::g_localPlayer) return -1.f;
        if (C.aimTeam && !t.enemy) return -1.f;
        if (C.aimMaxDist > 0.f && t.dist > C.aimMaxDist) return -1.f;

        Vec3 aim = AimPointOf(t);
        if (aim.zero()) return -1.f;

        float score;
        if (screenMode) {
            Vec2 s{};
            if (!SDK::WorldToScreen(aim, s)) return -1.f;
            float dx = s.x - cx, dy = s.y - cy;
            score = sqrtf(dx * dx + dy * dy);
            if (score > radius) return -1.f;
        }
        else {
            Vec3 d3 = Aimbot::Norm(Aimbot::Sub(aim, eye));
            score = acosf(Aimbot::Dot(fwd, d3)) * (180.f / 3.14159265f);
            if (score > C.aimFov) return -1.f;
        }

        score += 0.5f;                              // piso anti-ruido
        if (t.player == g_sticky) score *= 0.75f;   // histerese
        return score;
    }

    // >>> FIX C26495: inicializadores explicitos.
    struct Cand
    {
        float         score = 0.f;
        Il2CppObject* obj = nullptr;
        Vec3          aim{};
    };

    // =========================================================================
    //  >>> AQUI esta o comportamento que voce pediu
    // =========================================================================
    static Il2CppObject* BestStrict()
    {
        if (SDK::g_targetCount <= 0) return nullptr;

        const float cx = (float)SDK::ScreenW() * 0.5f;
        const float cy = (float)SDK::ScreenH() * 0.5f;
        const float radius = SDK::FovRadiusPx(C.aimFov);
        const bool screenMode = (radius > 2.f && radius < 100000.f);

        const Vec3 eye = EyePos();
        const Vec3 fwd = Aimbot::CamForward();

        // 1) coleta quem esta dentro do FOV
        Cand list[64];
        int n = 0;
        for (int i = 0; i < SDK::g_targetCount && n < 64; ++i) {
            const Target& t = SDK::g_targets[i];
            float s = ScoreTarget(t, eye, fwd, radius, screenMode, cx, cy);
            if (s < 0.f) continue;
            Vec3 ap = AimPointOf(t);
            if (ap.zero()) continue;
            list[n].score = s;
            list[n].obj = t.player;
            list[n].aim = ap;
            ++n;
        }
        if (n == 0) return nullptr;

        // 2) ordena do mais perto da mira pro mais longe (insertion sort, n<=64)
        for (int i = 1; i < n; ++i) {
            Cand key = list[i];
            int j = i - 1;
            while (j >= 0 && list[j].score > key.score) { list[j + 1] = list[j]; --j; }
            list[j + 1] = key;
        }

        // 3) visCheck desligado ou LOS indisponivel -> sem filtro
        const bool filtrar = (C.aimVisCheck != 0) && SDK::LosReady();
        if (!filtrar) return list[0].obj;

        // 4) percorre na ordem e pega o PRIMEIRO com linha de visao de verdade.
        int tentativas = 0;
        for (int i = 0; i < n && tentativas < 12; ++i, ++tentativas) {
            if (SDK::HasLineOfSight(eye, list[i].aim)) return list[i].obj;
        }

        // 5) nenhum visivel:
        //    strict = false (padrao) -> cai no mais perto (atras do muro)
        //    strict = true          -> nao mira em ninguem
        return C.aimStrictVis ? nullptr : list[0].obj;
    }

    Il2CppObject* Best()
    {
        g_best = nullptr;
        if (!C.aimbot || !SDK::g_localPlayer) return nullptr;

        __try {
            Il2CppObject* pick = BestStrict();
            g_best = pick;
            g_sticky = pick;

            if (C.probeLog && g_best) {
                uint64_t now = GetTickCount64();
                if (now - g_lastAimLog >= 250) {
                    g_lastAimLog = now;

                    Vec3 ap = Aimbot::AimPoint((void*)g_best);
                    Vec3 eye = EyePos();
                    Vec3 dv = Aimbot::Sub(ap, eye);
                    float dist = Len(dv);                          // >>> FIX
                    Vec3 d = Aimbot::Norm(dv);
                    float ang = acosf(Aimbot::Dot(Aimbot::CamForward(), d)) * (180.f / 3.14159265f);
                    Vec2 s{};
                    bool on = SDK::WorldToScreen(ap, s);
                    bool los = SDK::HasLineOfSight(eye, ap);

                    Log("[aim] alvo=%p dist=%.1f ang=%.1f onScreen=%d los=%d visCheck=%d strict=%d",
                        g_best, dist, ang, (int)on, (int)los,
                        (int)C.aimVisCheck, (int)C.aimStrictVis);
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { g_best = nullptr; }

        return g_best;
    }

    // =========================================================================
    //  rotacao: via SDK (SyncVar-safe), no camTf e no player
    // =========================================================================
    static void ProbePlayerRot()
    {
        if (g_rotProbed || !SDK::g_localPlayer) return;
        g_rotProbed = true;
        ::Quat q = SDK::GetPlayerRot(SDK::g_localPlayer);
        g_writePlayerRot = PlausibleQ(q);
        Log("[aim] playerRot legivel=%d (%.3f,%.3f,%.3f,%.3f) -> %s",
            (int)g_writePlayerRot, q.x, q.y, q.z, q.w,
            g_writePlayerRot ? "escrevendo no player tambem"
            : "SO no camTf (cheque 'syncRot=' no [sdk] log)");
    }

    bool Install()
    {
        Il2CppObject* ct = SDK::CamTfObj();
        Log("[aimbot] install: fov=%.1f tecla=0x%02X camTf=%p bone=%d smooth=%.1f "
            "visCheck=%d strict=%d",
            C.aimFov, C.aimKey, ct, C.aimBone, C.aimSmooth,
            (int)C.aimVisCheck, (int)C.aimStrictVis);
        return true;
    }

    void Tick()
    {
        if (!C.aimbot || !SDK::g_localPlayer) return;

        ProbePlayerRot();

        Il2CppObject* best = SDK::g_aimTarget ? SDK::g_aimTarget : g_best;
        if (!best) return;
        if (!KeyDown()) return;

        __try {
            Il2CppObject* camTf = SDK::CamTfObj();
            if (!camTf) return;

            Vec3 aim = Aimbot::AimPoint((void*)best);
            if (aim.zero()) aim = SDK::GetPos(best);
            Vec3 eye = EyePos();

            Vec3 dir = Aimbot::Sub(aim, eye);
            float L = Len(dir);
            if (L < 0.0001f) return;
            dir = dir * (1.f / L);

            ::Quat want = Aimbot::LookRot(dir);

            ::Quat cur;
            bool haveCur = false;

            ::Quat curSdk{};
            if (SDK::GetRot(camTf, curSdk) && PlausibleQ(curSdk)) {
                cur = curSdk;
                haveCur = true;
            }
            else {
                ::Quat p = SDK::GetPlayerRot(SDK::g_localPlayer);
                if (PlausibleQ(p)) { cur = p; haveCur = true; }
            }
            if (!haveCur) cur = ::Quat{ 0.f, 0.f, 0.f, 1.f };

            float t = (C.aimSmooth <= 0.f) ? 1.f : (1.f / C.aimSmooth);
            if (t > 1.f) t = 1.f;
            ::Quat q = Aimbot::Slerp(cur, want, t);

            SDK::SetRot(camTf, q);
            if (g_writePlayerRot) SDK::SetPlayerRot(SDK::g_localPlayer, q);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { Log("[crash] Aimbot::Tick"); }
    }

    // =========================================================================
    //  SILENT AIM / MAGIC BULLETS
    // =========================================================================
    namespace Silent {
        bool Override(Vec3 origin, Vec3& dir)
        {
            if (!C.silentAim && !C.magicBullets) return false;
            void* local = SDK::g_localPlayer;
            if (!local) return false;

            __try {
                const bool filtrar = (C.aimVisCheck != 0) && SDK::LosReady();

                float best = 180.f;
                void* bestT = nullptr;
                float bestCego = 180.f;
                void* bestCegoT = nullptr;

                for (int i = 0; i < SDK::g_targetCount; ++i) {
                    const Target& t = SDK::g_targets[i];
                    if (!t.player || !t.alive || t.obj == local) continue;
                    if (C.aimTeam && !t.enemy) continue;
                    if (C.aimMaxDist > 0.f && t.dist > C.aimMaxDist) continue;

                    Vec3 ap = AimPointOf(t);
                    if (ap.zero()) continue;

                    Vec3 d = Aimbot::Norm(Aimbot::Sub(ap, origin));
                    float ang = acosf(Aimbot::Dot(dir, d)) * (180.f / 3.14159265f);

                    if (ang < bestCego) { bestCego = ang; bestCegoT = t.obj; }

                    if (filtrar) {
                        if (!SDK::HasLineOfSight(origin, ap)) continue;
                        if (ang < best) { best = ang; bestT = t.obj; }
                    }
                }

                void* escolhido = nullptr;
                if (bestT) escolhido = bestT;
                else if (!filtrar) escolhido = bestCegoT;
                else escolhido = C.aimStrictVis ? nullptr : bestCegoT;

                if (!escolhido) return false;

                Vec3 ap = Aimbot::AimPoint(escolhido);
                if (ap.zero()) return false;
                dir = Aimbot::Norm(Aimbot::Sub(ap, origin));
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        }
    }

    void Shutdown()
    {
        g_best = nullptr;
        g_sticky = nullptr;
        g_rotProbed = false;
        g_writePlayerRot = true;
    }

} // namespace Aimbot