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
#include <cstdio>
#include <string>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

void Log(const char* fmt, ...) {
    va_list a; va_start(a, fmt); vprintf(fmt, a); va_end(a); printf("\n");
}

Memory mem;
Overlay ov;

bool  esp_enabled    = true;
bool  aimbot_enabled = true;
bool  tracers        = true;
bool  show_names     = true;
bool  show_health    = true;
float aimbot_fov     = 220.f;
float aimbot_smooth  = 0.15f;
bool  menu_open      = true;

// ---- runtime offsets (filled by scanner) ----
uintptr_t OFF_DataModel      = 0;
uintptr_t OFF_Children       = 0x70;
uintptr_t OFF_Name           = 104;
uintptr_t OFF_HumanoidHealth = 388;
uintptr_t OFF_Primitive      = 376;
uintptr_t OFF_Position       = 212;
uintptr_t OFF_CameraVM       = 192;
uintptr_t OFF_CurrentCamera  = 1184;
uintptr_t OFF_LocalPlayer    = 280;
uintptr_t OFF_Character      = 656;

struct Player { std::string name; Vec3 headPos, rootPos; float health; };

// fetch latest offsets.json from rbxoffsets API
bool FetchOffsets() {
    Log("[scan] fetching offsets from rbxoffsets.com...");

    HINTERNET hSession = WinHttpOpen(L"v67/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
    if (!hSession) { Log("[scan] WinHttpOpen failed"); return false; }

    HINTERNET hConn = WinHttpConnect(hSession, L"www.rbxoffsets.com",
        INTERNET_DEFAULT_HTTPS_PORT, 0);
    HINTERNET hReq  = WinHttpOpenRequest(hConn, L"GET",
        L"/api/v1/windows/offsets/latest/json",
        NULL, NULL, NULL, WINHTTP_FLAG_SECURE);
    WinHttpSendRequest(hReq, NULL, 0, NULL, 0, 0, 0);
    WinHttpReceiveResponse(hReq, NULL);

    std::string body;
    DWORD read = 0;
    char buf[4096];
    while (WinHttpReadData(hReq, buf, sizeof(buf)-1, &read) && read)
        { buf[read]=0; body += buf; }
    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConn);
    WinHttpCloseHandle(hSession);

    if (body.empty()) { Log("[scan] empty response"); return false; }
    Log("[scan] got %zu bytes", body.size());

    // parse key offsets — simple strstr JSON extraction
    auto getVal = [&](const char* key) -> uintptr_t {
        std::string k = std::string("\"") + key + "\":";
        auto pos = body.find(k);
        if (pos == std::string::npos) return 0;
        pos += k.size();
        while (pos < body.size() && (body[pos]==' '||body[pos]=='"')) pos++;
        // handle hex string "0x..." or decimal
        if (body[pos]=='0' && body[pos+1]=='x')
            return (uintptr_t)strtoull(body.c_str()+pos, nullptr, 16);
        return (uintptr_t)strtoull(body.c_str()+pos, nullptr, 10);
    };

    // Instance children / name
    uintptr_t childStart = getVal("ChildrenStart");
    uintptr_t nameOff    = getVal("NameContainer");
    uintptr_t hmHealth   = getVal("Health");
    uintptr_t primitive  = getVal("Primitive");
    uintptr_t primPos    = getVal("Position");
    uintptr_t camVM      = getVal("Rotation"); // Camera Rotation = ViewMatrix
    uintptr_t curCam     = getVal("CurrentCamera");
    uintptr_t localPl    = getVal("LocalPlayer");
    uintptr_t charOff    = getVal("ModelInstance");

    if (childStart) { OFF_Children      = childStart; Log("[scan] ChildrenStart  = 0x%llX", OFF_Children); }
    if (nameOff)    { OFF_Name          = nameOff;    Log("[scan] NameContainer  = 0x%llX", OFF_Name); }
    if (hmHealth)   { OFF_HumanoidHealth= hmHealth;   Log("[scan] Health         = 0x%llX", OFF_HumanoidHealth); }
    if (primitive)  { OFF_Primitive     = primitive;  Log("[scan] Primitive      = 0x%llX", OFF_Primitive); }
    if (primPos)    { OFF_Position      = primPos;    Log("[scan] Position       = 0x%llX", OFF_Position); }
    if (camVM)      { OFF_CameraVM      = camVM;      Log("[scan] CameraVM       = 0x%llX", OFF_CameraVM); }
    if (curCam)     { OFF_CurrentCamera = curCam;     Log("[scan] CurrentCamera  = 0x%llX", OFF_CurrentCamera); }
    if (localPl)    { OFF_LocalPlayer   = localPl;    Log("[scan] LocalPlayer    = 0x%llX", OFF_LocalPlayer); }
    if (charOff)    { OFF_Character     = charOff;    Log("[scan] Character      = 0x%llX", OFF_Character); }

    return true;
}

// scan memory for DataModel by looking for "Players" service
uintptr_t ScanDataModel() {
    Log("[scan] scanning for DataModel...");
    // try known pointer offsets — walk through candidates
    uintptr_t candidates[] = {
        0x05628F28, 0x08ECA150, 0x089C9150, 0x056A3F28, 0x05700000
    };
    for (auto off : candidates) {
        uintptr_t dm = mem.Read<uintptr_t>(mem.base + off);
        if (!dm || dm < 0x10000) continue;
        // check children list sanity
        uintptr_t ch  = mem.Read<uintptr_t>(dm + OFF_Children);
        int cnt = mem.Read<int>(ch + 0x8);
        if (cnt < 3 || cnt > 500) continue;
        uintptr_t arr = mem.Read<uintptr_t>(ch);
        bool found = false;
        for (int i = 0; i < cnt && i < 30; i++) {
            uintptr_t inst = mem.Read<uintptr_t>(arr + i*8);
            if (!inst) continue;
            uintptr_t np = mem.Read<uintptr_t>(inst + OFF_Name);
            std::string nm = mem.ReadString(np);
            if (nm == "Players" || nm == "Workspace" || nm == "Lighting") {
                found = true; break;
            }
        }
        if (found) {
            Log("[scan] DataModel found @ base+0x%llX = 0x%llX", off, dm);
            return dm;
        }
    }
    // brute-force scan .data section ±8MB around known offsets
    Log("[scan] brute forcing DataModel...");
    for (uintptr_t off = 0x04000000; off < 0x0C000000; off += 8) {
        uintptr_t dm = mem.Read<uintptr_t>(mem.base + off);
        if (!dm || dm < 0x100000 || dm > 0x7FFFFFFFFFFF) continue;
        uintptr_t ch = mem.Read<uintptr_t>(dm + OFF_Children);
        if (!ch || ch < 0x100000) continue;
        int cnt = mem.Read<int>(ch + 0x8);
        if (cnt < 5 || cnt > 100) continue;
        uintptr_t arr = mem.Read<uintptr_t>(ch);
        if (!arr || arr < 0x100000) continue;
        int hits = 0;
        for (int i = 0; i < cnt && i < 20; i++) {
            uintptr_t inst = mem.Read<uintptr_t>(arr + i*8);
            if (!inst) continue;
            uintptr_t np = mem.Read<uintptr_t>(inst + OFF_Name);
            std::string nm = mem.ReadString(np);
            if (nm=="Players"||nm=="Workspace"||nm=="Lighting"||nm=="RunService") hits++;
        }
        if (hits >= 3) {
            Log("[scan] DataModel brute @ base+0x%llX = 0x%llX (hits=%d)", off, dm, hits);
            OFF_DataModel = off;
            return dm;
        }
    }
    return 0;
}

std::vector<uintptr_t> GetChildren(uintptr_t inst) {
    std::vector<uintptr_t> out;
    uintptr_t ch = mem.Read<uintptr_t>(inst + OFF_Children);
    int cnt = mem.Read<int>(ch + 0x8);
    uintptr_t arr = mem.Read<uintptr_t>(ch);
    for (int i = 0; i < cnt && i < 256; i++) {
        uintptr_t c = mem.Read<uintptr_t>(arr + i*8);
        if (c) out.push_back(c);
    }
    return out;
}

uintptr_t FindChild(uintptr_t inst, const std::string& name) {
    for (auto c : GetChildren(inst)) {
        uintptr_t np = mem.Read<uintptr_t>(c + OFF_Name);
        if (mem.ReadString(np) == name) return c;
    }
    return 0;
}

uintptr_t FindService(uintptr_t dm, const std::string& name) {
    uintptr_t ch = mem.Read<uintptr_t>(dm + OFF_Children);
    int cnt = mem.Read<int>(ch + 0x8);
    uintptr_t arr = mem.Read<uintptr_t>(ch);
    for (int i = 0; i < cnt && i < 512; i++) {
        uintptr_t inst = mem.Read<uintptr_t>(arr + i*8);
        if (!inst) continue;
        uintptr_t np = mem.Read<uintptr_t>(inst + OFF_Name);
        if (mem.ReadString(np) == name) return inst;
    }
    return 0;
}

std::vector<Player> GetPlayers(uintptr_t playersService, const std::string& localName) {
    std::vector<Player> out;
    for (auto plInst : GetChildren(playersService)) {
        uintptr_t np = mem.Read<uintptr_t>(plInst + OFF_Name);
        std::string pname = mem.ReadString(np);
        if (pname == localName || pname.empty()) continue;
        uintptr_t charInst = FindChild(plInst, pname);
        if (!charInst) continue;
        uintptr_t humanoid = FindChild(charInst, "Humanoid");
        uintptr_t head     = FindChild(charInst, "Head");
        uintptr_t root     = FindChild(charInst, "HumanoidRootPart");
        if (!humanoid || !head || !root) continue;
        float hp = mem.Read<float>(humanoid + OFF_HumanoidHealth);
        if (hp <= 0) continue;
        uintptr_t hPrim = mem.Read<uintptr_t>(head + OFF_Primitive);
        uintptr_t rPrim = mem.Read<uintptr_t>(root + OFF_Primitive);
        Vec3 hPos = mem.Read<Vec3>(hPrim + OFF_Position);
        Vec3 rPos = mem.Read<Vec3>(rPrim + OFF_Position);
        out.push_back({pname, hPos, rPos, hp});
    }
    return out;
}

void AimbotMove(Vec2 target, float sw, float sh, float smooth) {
    float dx = (target.x - sw/2.f) * smooth;
    float dy = (target.y - sh/2.f) * smooth;
    INPUT inp{}; inp.type = INPUT_MOUSE;
    inp.mi.dwFlags = MOUSEEVENTF_MOVE;
    inp.mi.dx = (LONG)dx; inp.mi.dy = (LONG)dy;
    SendInput(1, &inp, sizeof(INPUT));
}

int main() {
    AllocConsole();
    freopen_s((FILE**)stdout, "CONOUT$", "w", stdout);
    SetConsoleTitleA("v67suite debug");

    Log("[v67] starting...");
    Log("[v67] attaching to Roblox...");

    if (!mem.Attach(L"RobloxPlayerBeta.exe")) {
        Log("[v67] ERROR: Roblox not found");
        MessageBoxA(nullptr, "Abre Roblox primero.", "v67", MB_OK|MB_ICONERROR);
        system("pause"); return 1;
    }
    Log("[v67] base = 0x%llX", mem.base);

    HWND robloxHwnd = FindWindowA(nullptr, "Roblox");
    if (!robloxHwnd) {
        Log("[v67] ERROR: window not found");
        system("pause"); return 1;
    }

    FetchOffsets();
    Log("[v67] scanning DataModel...");
    uintptr_t dm = ScanDataModel();
    if (!dm) {
        Log("[v67] ERROR: DataModel not found — try running in-game");
        system("pause"); return 1;
    }

    uintptr_t players   = FindService(dm, "Players");
    uintptr_t workspace = FindService(dm, "Workspace");
    Log("[v67] Players   = 0x%llX", players);
    Log("[v67] Workspace = 0x%llX", workspace);

    uintptr_t camera  = FindChild(workspace, "Camera");
    uintptr_t localPl = mem.Read<uintptr_t>(players + OFF_LocalPlayer);
    uintptr_t lpNameP = mem.Read<uintptr_t>(localPl + OFF_Name);
    std::string lpName = mem.ReadString(lpNameP);
    Log("[v67] local = %s | camera = 0x%llX", lpName.c_str(), camera);

    ov.Init(robloxHwnd);
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(ov.hwnd);
    ImGui_ImplDX11_Init(ov.device, ov.ctx);
    ImGui::GetIO().IniFilename = nullptr;
    Log("[v67] overlay ready. INSERT = menu");

    RECT wr{};
    MSG msg{};
    while (true) {
        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) DispatchMessageA(&msg);
        if (GetAsyncKeyState(VK_INSERT) & 1) menu_open = !menu_open;

        GetWindowRect(robloxHwnd, &wr);
        float sw = (float)(wr.right-wr.left);
        float sh = (float)(wr.bottom-wr.top);
        SetWindowPos(ov.hwnd, HWND_TOPMOST, wr.left, wr.top, (int)sw, (int)sh, 0);

        Mat4 vp = mem.Read<Mat4>(camera + OFF_CameraVM);
        auto playerList = GetPlayers(players, lpName);

        bool aiming = aimbot_enabled && (GetAsyncKeyState(VK_RBUTTON) & 0x8000);
        Player* best = nullptr; float bestDist = aimbot_fov;
        for (auto& pl : playerList) {
            Vec2 s = WorldToScreen(pl.headPos, vp, sw, sh);
            if (s.x < 0) continue;
            float d = Distance2D(s, {sw/2.f, sh/2.f});
            if (d < bestDist) { bestDist = d; best = &pl; }
        }
        if (aiming && best) {
            Vec2 s = WorldToScreen(best->headPos, vp, sw, sh);
            if (s.x > 0) AimbotMove(s, sw, sh, aimbot_smooth);
        }

        ov.BeginFrame();
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (esp_enabled) {
            ImGui::SetNextWindowPos({0,0});
            ImGui::SetNextWindowSize({sw,sh});
            ImGui::SetNextWindowBgAlpha(0.f);
            ImGui::Begin("##esp", nullptr,
                ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoInputs|
                ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoBackground|
                ImGuiWindowFlags_NoBringToFrontOnFocus);
            auto* dl = ImGui::GetWindowDrawList();
            for (auto& pl : playerList) {
                Vec2 head = WorldToScreen(pl.headPos, vp, sw, sh);
                Vec2 root = WorldToScreen(pl.rootPos, vp, sw, sh);
                if (head.x < 0 || root.x < 0) continue;
                float h = fabsf(root.y-head.y)*1.2f;
                float w = h*0.45f, cx = head.x;
                dl->AddRect({cx-w/2.f,head.y-5.f},{cx+w/2.f,root.y+5.f},IM_COL32(255,0,0,220),0,0,1.5f);
                if (show_names) dl->AddText({cx-20.f,head.y-18.f},IM_COL32(255,255,255,255),pl.name.c_str());
                if (show_health) {
                    float r = pl.health/100.f;
                    dl->AddRectFilled({cx-w/2.f-6.f,head.y-5.f},{cx-w/2.f-3.f,head.y-5.f+h*(1.f-r)},IM_COL32(255,0,0,200));
                    dl->AddRectFilled({cx-w/2.f-6.f,head.y-5.f+h*(1.f-r)},{cx-w/2.f-3.f,root.y+5.f},IM_COL32(0,255,0,200));
                }
                if (tracers) dl->AddLine({sw/2.f,sh},{cx,root.y},IM_COL32(0,255,100,180),1.f);
            }
            ImGui::End();
        }

        if (menu_open) {
            ImGui::SetNextWindowPos({20,20},ImGuiCond_Once);
            ImGui::SetNextWindowSize({280,320},ImGuiCond_Once);
            ImGui::Begin("v67suite",&menu_open);
            ImGui::SeparatorText("ESP");
            ImGui::Checkbox("ESP",&esp_enabled);
            ImGui::Checkbox("Names",&show_names);
            ImGui::Checkbox("Health",&show_health);
            ImGui::Checkbox("Tracers",&tracers);
            ImGui::SeparatorText("Aimbot");
            ImGui::Checkbox("Aimbot (RMB)",&aimbot_enabled);
            ImGui::SliderFloat("FOV",&aimbot_fov,50.f,500.f);
            ImGui::SliderFloat("Smooth",&aimbot_smooth,0.01f,1.f);
            ImGui::SeparatorText("Info");
            ImGui::Text("Players: %d", (int)playerList.size());
            ImGui::Text("DataModel: 0x%llX", dm);
            ImGui::Text("INSERT = toggle");
            ImGui::End();
        }

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        ov.EndFrame();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}