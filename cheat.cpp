#include "memory.hpp"
#include "offsets.hpp"
#include "math.hpp"
#include "overlay.hpp"
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_dx11.h"
#include "imgui/backends/imgui_impl_win32.h"
#include <vector>
#include <thread>
#include <Windows.h>

Memory mem;
Overlay ov;

struct Player {
    std::string name;
    Vec3 headPos;
    Vec3 rootPos;
    float health;
};

std::vector<uintptr_t> GetChildren(uintptr_t inst) {
    std::vector<uintptr_t> out;
    uintptr_t children = mem.Read<uintptr_t>(inst + Offsets::InstanceChildren);
    int count = mem.Read<int>(children + 0x8);
    uintptr_t arr = mem.Read<uintptr_t>(children + 0x0);
    for (int i = 0; i < count && i < 256; i++) {
        uintptr_t c = mem.Read<uintptr_t>(arr + i * 8);
        if (c) out.push_back(c);
    }
    return out;
}

uintptr_t FindChild(uintptr_t inst, const std::string& name) {
    for (auto c : GetChildren(inst)) {
        uintptr_t np = mem.Read<uintptr_t>(c + Offsets::InstanceName);
        if (mem.ReadString(np) == name) return c;
    }
    return 0;
}

uintptr_t FindService(uintptr_t dm, const std::string& name) {
    uintptr_t children = mem.Read<uintptr_t>(dm + Offsets::DataModelChildren);
    int count = mem.Read<int>(children + 0x8);
    uintptr_t arr = mem.Read<uintptr_t>(children + 0x0);
    for (int i = 0; i < count && i < 512; i++) {
        uintptr_t inst = mem.Read<uintptr_t>(arr + i * 8);
        if (!inst) continue;
        uintptr_t np = mem.Read<uintptr_t>(inst + Offsets::InstanceName);
        if (mem.ReadString(np) == name) return inst;
    }
    return 0;
}

std::vector<Player> GetPlayers(uintptr_t playersService, const std::string& localName) {
    std::vector<Player> out;
    for (auto plInst : GetChildren(playersService)) {
        uintptr_t np = mem.Read<uintptr_t>(plInst + Offsets::InstanceName);
        std::string pname = mem.ReadString(np);
        if (pname == localName || pname.empty()) continue;
        uintptr_t charInst = FindChild(plInst, pname);
        if (!charInst) continue;
        uintptr_t humanoid = FindChild(charInst, "Humanoid");
        uintptr_t head     = FindChild(charInst, "Head");
        uintptr_t root     = FindChild(charInst, "HumanoidRootPart");
        if (!humanoid || !head || !root) continue;
        float hp = mem.Read<float>(humanoid + Offsets::HumanoidHealth);
        if (hp <= 0) continue;
        Vec3 hPos = mem.Read<Vec3>(head + Offsets::PartPosition);
        Vec3 rPos = mem.Read<Vec3>(root + Offsets::PartPosition);
        out.push_back({pname, hPos, rPos, hp});
    }
    return out;
}

void AimbotMove(Vec2 target, float sw, float sh, float smooth) {
    float dx = (target.x - sw / 2.f) * smooth;
    float dy = (target.y - sh / 2.f) * smooth;
    INPUT inp{};
    inp.type       = INPUT_MOUSE;
    inp.mi.dwFlags = MOUSEEVENTF_MOVE;
    inp.mi.dx      = (LONG)dx;
    inp.mi.dy      = (LONG)dy;
    SendInput(1, &inp, sizeof(INPUT));
}

int main() {
    if (!mem.Attach(L"RobloxPlayerBeta.exe")) {
        MessageBoxA(nullptr, "Roblox no encontrado.\nAbre Roblox primero.", "v67", MB_OK|MB_ICONERROR);
        return 1;
    }

    HWND robloxHwnd = FindWindowA(nullptr, "Roblox");
    if (!robloxHwnd) {
        MessageBoxA(nullptr, "Ventana de Roblox no encontrada.", "v67", MB_OK|MB_ICONERROR);
        return 1;
    }

    ov.Init(robloxHwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplWin32_Init(ov.hwnd);
    ImGui_ImplDX11_Init(ov.device, ov.ctx);
    ImGui::GetIO().IniFilename = nullptr;

    RECT wr{};
    GetWindowRect(robloxHwnd, &wr);
    float sw = (float)(wr.right  - wr.left);
    float sh = (float)(wr.bottom - wr.top);

    uintptr_t dm        = mem.Read<uintptr_t>(mem.base + Offsets::DataModel);
    uintptr_t players   = FindService(dm, "Players");
    uintptr_t workspace = FindService(dm, "Workspace");
    uintptr_t camera    = FindChild(workspace, "Camera");
    uintptr_t localPl   = FindChild(players, "LocalPlayer");
    uintptr_t lpNameP   = mem.Read<uintptr_t>(localPl + Offsets::InstanceName);
    std::string lpName  = mem.ReadString(lpNameP);

    MSG msg{};
    while (true) {
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE))
            DispatchMessageA(&msg);

        GetWindowRect(robloxHwnd, &wr);
        sw = (float)(wr.right - wr.left);
        sh = (float)(wr.bottom - wr.top);
        SetWindowPos(ov.hwnd, HWND_TOPMOST,
            wr.left, wr.top, (int)sw, (int)sh, 0);

        Mat4 vp         = mem.Read<Mat4>(camera + Offsets::CameraViewMatrix);
        auto playerList = GetPlayers(players, lpName);

        // aimbot
        bool aiming     = GetAsyncKeyState(VK_RBUTTON) & 0x8000;
        Player* best    = nullptr;
        float   bestDist = 220.f;

        for (auto& pl : playerList) {
            Vec2 s = WorldToScreen(pl.headPos, vp, sw, sh);
            if (s.x < 0) continue;
            float d = Distance2D(s, {sw/2.f, sh/2.f});
            if (d < bestDist) { bestDist = d; best = &pl; }
        }

        if (aiming && best) {
            Vec2 s = WorldToScreen(best->headPos, vp, sw, sh);
            if (s.x > 0) AimbotMove(s, sw, sh, 0.15f);
        }

        // render
        ov.BeginFrame();
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        ImGui::SetNextWindowPos({0,0});
        ImGui::SetNextWindowSize({sw, sh});
        ImGui::SetNextWindowBgAlpha(0.f);
        ImGui::Begin("##v67", nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs |
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground);

        auto* dl = ImGui::GetWindowDrawList();

        for (auto& pl : playerList) {
            Vec2 head = WorldToScreen(pl.headPos, vp, sw, sh);
            Vec2 root = WorldToScreen(pl.rootPos, vp, sw, sh);
            if (head.x < 0 || root.x < 0) continue;

            float h  = fabsf(root.y - head.y) * 1.2f;
            float w  = h * 0.45f;
            float cx = head.x;

            // box
            dl->AddRect(
                {cx-w/2.f, head.y-5.f},
                {cx+w/2.f, root.y+5.f},
                IM_COL32(255,0,0,220), 0, 0, 1.5f
            );
            // name
            dl->AddText(
                {cx-20.f, head.y-18.f},
                IM_COL32(255,255,255,255), pl.name.c_str()
            );
            // hp bar
            float ratio = pl.health / 100.f;
            dl->AddRectFilled(
                {cx-w/2.f-6.f, head.y-5.f},
                {cx-w/2.f-3.f, head.y-5.f + h*(1.f-ratio)},
                IM_COL32(255,0,0,200)
            );
            dl->AddRectFilled(
                {cx-w/2.f-6.f, head.y-5.f + h*(1.f-ratio)},
                {cx-w/2.f-3.f, root.y+5.f},
                IM_COL32(0,255,0,200)
            );
            // tracer
            dl->AddLine(
                {sw/2.f, sh},
                {cx, root.y},
                IM_COL32(0,255,100,180), 1.f
            );
        }

        ImGui::End();
        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        ov.EndFrame();

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}
