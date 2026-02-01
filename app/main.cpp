#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDI
#define CloseWindow Win32_CloseWindow
#define ShowCursor  Win32_ShowCursor
#define Rectangle   Win32_Rectangle

#include <windows.h>

#undef CloseWindow
#undef ShowCursor
#undef LoadImage
#undef DrawText
#undef DrawTextEx
#undef PlaySound
#undef Rectangle

#define RAYLIB_NOGDI
#include "raylib.h"

#include <atomic>
#include <mutex>
#include <thread>
#include <string>
#include <vector>
#include <cstdio>
#include <shlobj.h>
#include <tchar.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")

#include "KwypeLogic.h"

using namespace std;

static string g_inputBuffer;
static mutex  g_inputMutex;

static atomic<bool> g_ignoreInjected{ false };
static atomic<int>  g_selectIndex{ -1 };
static atomic<bool> g_closeapp{ false };
static atomic<bool> g_clearSwipe{ false };
static atomic<bool> g_toggleTop{ false };

void sendChar(char c)
{
    SHORT vk = VkKeyScanA(c);
    if (vk == -1) return;
    INPUT in[2] = {};
    in[0].type = INPUT_KEYBOARD;
    in[0].ki.wVk = vk & 0xFF;
    in[0].ki.wScan = MapVirtualKeyA(in[0].ki.wVk, MAPVK_VK_TO_VSC);
    in[1] = in[0];
    in[1].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(2, in, sizeof(INPUT));
}

void sendWord(const string& word)
{
    g_ignoreInjected = true;
    for (char c : word) {
        sendChar(c);
        Sleep(15);
    }
    sendChar(' ');
    g_ignoreInjected = false;
}

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    if (nCode == HC_ACTION)
    {
        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)
        {
            KBDLLHOOKSTRUCT* kb = (KBDLLHOOKSTRUCT*)lParam;
            if ((kb->flags & LLKHF_INJECTED) && g_ignoreInjected)
                return CallNextHookEx(nullptr, nCode, wParam, lParam);
            bool handled = false;
            if (kb->vkCode >= 'A' && kb->vkCode <= 'Z') {
                char c = char('a' + (kb->vkCode - 'A'));
                lock_guard<mutex> lock(g_inputMutex);
                g_inputBuffer.push_back(c);
                handled = true;
            }
            else {
                switch (kb->vkCode) {
                case VK_TAB: g_toggleTop = true; handled = true; break;
                case VK_ESCAPE: g_closeapp = true; handled = true; break;
                case VK_BACK: g_clearSwipe = true; handled = true; break;
                case VK_SPACE: g_selectIndex = 1; handled = true; break;
                case VK_LEFT: g_selectIndex = 0; handled = true; break;
                case VK_RIGHT: g_selectIndex = 2; handled = true; break;
                }
            }
            if (handled) return 1;
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

struct PredictionLabel {
    string word;
    float confidence;
};

class KwypeApp {
private:
    KwypeLogic kwype;
    vector<PredictionLabel> predictions;
    string currentSwipe;
    mutex predMutex;
    atomic<bool> running{ true };
    atomic<bool> needsUpdate{ false };
    bool isTopMost = false;
    HHOOK keyboardHook = nullptr;
    int selectedPrediction = -1;

public:
    KwypeApp(const string& m, const string& c, const string& l)
        : kwype(m, c, l) {}

    void startHook()
    {
        thread([] {
            SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, GetModuleHandle(nullptr), 0);
            MSG msg;
            while (GetMessage(&msg, nullptr, 0, 0)) {}
            }).detach();
    }

    bool init()
    {
        if (!kwype.load()) return false;
        SetTraceLogLevel(LOG_ALL);
        InitWindow(500, 70, "Kwype");
        SetTargetFPS(60);
        SetWindowState(FLAG_WINDOW_UNDECORATED);
        startHook();
        return true;
    }

    void updatePredictions()
    {
        if (currentSwipe.empty()) {
            lock_guard<mutex> lock(predMutex);
            predictions.clear();
            selectedPrediction = -1;
            return;
        }
        auto preds = kwype.predict(currentSwipe);
        lock_guard<mutex> lock(predMutex);
        predictions.clear();
        for (auto& p : preds) predictions.push_back({ p.label, p.confidence });
    }

    void selectPrediction(int idx) {
        lock_guard<mutex> lock(predMutex);
        if (predictions.empty() || idx >= predictions.size()) return;
        selectedPrediction = idx;
        sendWord(predictions[idx].word);
        currentSwipe.clear();
        predictions.clear();
        selectedPrediction = -1;
    }

    void handleKeyboard()
    {
        {
            lock_guard<mutex> lock(g_inputMutex);
            if (!g_inputBuffer.empty()) {
                currentSwipe += g_inputBuffer;
                g_inputBuffer.clear();
                needsUpdate = true;
            }
        }
        if (g_clearSwipe.exchange(false)) {
            currentSwipe.clear();
            predictions.clear();
            selectedPrediction = -1;
        }
        if (g_closeapp) { exit(0); }

        int sel = g_selectIndex.exchange(-1);
        if (sel != -1) selectPrediction(sel);

        if (g_toggleTop.exchange(false)) {
            isTopMost = !isTopMost;
            if (isTopMost) SetWindowState(FLAG_WINDOW_TOPMOST | FLAG_WINDOW_UNDECORATED);
            else ClearWindowState(FLAG_WINDOW_TOPMOST | FLAG_WINDOW_UNDECORATED);
        }

        if (needsUpdate.exchange(false)) updatePredictions();
    }

    void draw()
    {
        BeginDrawing();
        ClearBackground({ 40, 44, 52, 255 });

        lock_guard<mutex> lock(predMutex);
        for (int i = 0; i < predictions.size() && i < 3; i++) {
            Color bgColor = (i == selectedPrediction) ? Color{ 70, 74, 82, 255 } : Color{ 50, 54, 62, 255 };
            DrawRectangle(10 + i * 160, 10, 150, 40, bgColor);
            DrawRectangleLines(10 + i * 160, 10, 150, 40, (i == selectedPrediction) ? BLUE : DARKGRAY);
            DrawText(predictions[i].word.c_str(), 20 + i * 160, 20, 18, WHITE);
        }

        DrawText("TAB | ESC: Close | BACKSPACE: Clear | SPACE/LEFT/RIGHT: Select", 10, 60, 12, GRAY);

        EndDrawing();
    }


    void GenerateShortcut(LPCTSTR targetPath, LPCTSTR workingDir, LPCTSTR shortcutName, LPCTSTR description, LPCTSTR iconPath) {
        TCHAR shortcutFolderPath[MAX_PATH];
        if (FAILED(SHGetFolderPath(NULL, CSIDL_PROGRAMS, NULL, 0, shortcutFolderPath))) { return; }
        _tcscat_s(shortcutFolderPath, MAX_PATH, _T("\\Kwype"));
        SHCreateDirectoryEx(NULL, shortcutFolderPath, NULL);
        TCHAR fullShortcutPath[MAX_PATH];
        _stprintf_s(fullShortcutPath, MAX_PATH, _T("%s\\%s.lnk"), shortcutFolderPath, shortcutName);
        if (GetFileAttributes(fullShortcutPath) != INVALID_FILE_ATTRIBUTES) return;
        HRESULT hr = CoInitialize(NULL);
        if (FAILED(hr)) return;

        IShellLink* pShellLink = NULL;
        if (SUCCEEDED(CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&pShellLink))) {
            pShellLink->SetPath(targetPath);
            pShellLink->SetWorkingDirectory(workingDir);
            pShellLink->SetDescription(description);
            pShellLink->SetIconLocation(iconPath, 0);

            IPersistFile* pPersistFile = NULL;
            if (SUCCEEDED(pShellLink->QueryInterface(IID_IPersistFile, (void**)&pPersistFile))) {
                #ifdef UNICODE
                pPersistFile->Save(fullShortcutPath, TRUE);
                #else
                WCHAR widePath[MAX_PATH];
                MultiByteToWideChar(CP_ACP, 0, fullShortcutPath, -1, widePath, MAX_PATH);
                pPersistFile->Save(widePath, TRUE);
                #endif
                pPersistFile->Release();
            }
            pShellLink->Release();
        }
        CoUninitialize();
    }

    void run()
    {
        TCHAR szExePath[MAX_PATH];
        GetModuleFileName(NULL, szExePath, MAX_PATH);
        TCHAR szDir[MAX_PATH];
        _tcscpy_s(szDir, szExePath);
        for (size_t i = _tcslen(szDir); i > 0; i--) {
            if (szDir[i] == _T('\\')) { szDir[i] = _T('\0'); break; }
        }
        GenerateShortcut(
            szExePath, 
            szDir,      
            _T("Kwype"),     
            _T("Fast Typing Assistant"),
            szExePath
        );

        ShowWindow(GetConsoleWindow(), SW_HIDE);
        while (!WindowShouldClose()) {
            handleKeyboard();
            draw();
        }
        CloseWindow();
    }
};

int main()
{
    KwypeApp app("data\\model.tflite", "data\\classes.json", "data\\max_len.txt");
    if (!app.init()) return 1;
    app.run();
    return 0;
}
