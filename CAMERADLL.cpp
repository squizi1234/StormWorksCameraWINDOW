#include <windows.h>
#include <iostream>
#include <atomic>
#include <string>
#include <sstream>
#include <cmath>
#include "MinHook.h"
#include <gl/GL.h>

#pragma comment(lib, "opengl32.lib")

// ===================== TYPES =====================
typedef BOOL(__stdcall* twglSwapBuffers)    (HDC);
typedef void(__stdcall* tglBindFramebuffer) (unsigned int, unsigned int);
typedef void(__stdcall* tglGetIntegerv)     (unsigned int, int*);
typedef void(__stdcall* tglBindTexture)     (unsigned int, unsigned int);
typedef void(__stdcall* tglGetTexImage)     (unsigned int, int, unsigned int, unsigned int, void*);
typedef void(__stdcall* tglTexImage2D)      (unsigned int, int, int, int, int, int, unsigned int, unsigned int, const void*);
typedef void(__stdcall* tglDrawElements)    (unsigned int, int, unsigned int, const void*);
typedef void(__stdcall* tglTexLevelParam)   (unsigned int, int, unsigned int, int*);
typedef void(__stdcall* tglPixelStorei)     (unsigned int, int);

typedef void(__stdcall* tglGenBuffers)      (int, unsigned int*);
typedef void(__stdcall* tglBindBuffer)      (unsigned int, unsigned int);
typedef void(__stdcall* tglBufferData)      (unsigned int, ptrdiff_t, const void*, unsigned int);
typedef void* (__stdcall* tglMapBuffer)      (unsigned int, unsigned int);
typedef BOOL(__stdcall* tglUnmapBuffer)     (unsigned int);
typedef void(__stdcall* tglGenerateMipmap)  (unsigned int);

// ===================== ORIGINALS =====================
twglSwapBuffers    oWglSwapBuffers = nullptr;
tglTexImage2D      oGlTexImage2D_orig = nullptr;
tglBindTexture     oGlBindTexture_orig = nullptr;
tglDrawElements    oGlDrawElements_orig = nullptr;

// ===================== GL FUNCTION POINTERS =====================
tglGetIntegerv     glGetIntegerv_fn = nullptr;
tglBindTexture     glBindTexture_fn = nullptr;
tglGetTexImage     glGetTexImage_fn = nullptr;
tglTexLevelParam   glGetTexLevelParam_fn = nullptr;
tglPixelStorei     glPixelStorei_fn = nullptr;

tglGenBuffers      glGenBuffers_fn = nullptr;
tglBindBuffer      glBindBuffer_fn = nullptr;
tglBufferData      glBufferData_fn = nullptr;
tglMapBuffer       glMapBuffer_fn = nullptr;
tglUnmapBuffer     glUnmapBuffer_fn = nullptr;
tglGenerateMipmap  glGenerateMipmap_fn = nullptr;

bool                glFunctionsLoaded = false;

// ===================== CONSTANTS =====================
static const int CAM_WIDTH = 1152;
static const int CAM_HEIGHT = 640;
static const int BUFFER_SIZE = CAM_WIDTH * CAM_HEIGHT * 4;
static const int RAW_HALF_SIZE = CAM_WIDTH * CAM_HEIGHT * 4 * sizeof(unsigned short);
static const int MAX_CAMS = 16;
static const int MAX_PINNED = 8;

static const int TOTAL_SHARED_SIZE = BUFFER_SIZE * MAX_PINNED;

// ===================== GL ENUMS =====================
#ifndef GL_TEXTURE_BINDING_2D
#define GL_TEXTURE_BINDING_2D        0x8069
#endif
#ifndef GL_TEXTURE_WIDTH
#define GL_TEXTURE_WIDTH             0x1000
#endif
#ifndef GL_TEXTURE_HEIGHT
#define GL_TEXTURE_HEIGHT            0x1001
#endif
#ifndef GL_TEXTURE_INTERNAL_FORMAT
#define GL_TEXTURE_INTERNAL_FORMAT  0x1003
#endif
#ifndef GL_PACK_ALIGNMENT
#define GL_PACK_ALIGNMENT            0x0CF5
#endif
#ifndef GL_HALF_FLOAT
#define GL_HALF_FLOAT                0x140B
#endif
#ifndef GL_RGBA16F
#define GL_RGBA16F                  0x881A
#endif
#ifndef GL_PIXEL_PACK_BUFFER
#define GL_PIXEL_PACK_BUFFER        0x88EB
#endif
#ifndef GL_STREAM_READ
#define GL_STREAM_READ               0x88E4
#endif
#ifndef GL_READ_ONLY
#define GL_READ_ONLY                 0x88B8
#endif

// ===================== PBO STATE =====================
unsigned int g_pbo[2] = { 0, 0 };
int          g_pboIdx = 0;
bool         g_pboInitialized = false;

// ===================== DETECTED TEXTURE REGISTRY =====================
struct TexEntry {
    unsigned int    texID = 0;
    int             internalFmt = 0;
    int             w = 0;
    int             h = 0;
    unsigned short* rawHalfBuffer = nullptr;
    unsigned char* frontBuffer = nullptr;
    bool            pinned = false;
    char            label[64] = { 0 };
    std::atomic<bool> hasNewRawData{ false };
};

TexEntry           g_textures[MAX_CAMS] = {};
std::atomic<int>   g_numTextures(0);

// Pinned cameras state
struct PinnedCam {
    int   texIdx = -1;
    HWND  hWindow = nullptr;
    HDC   hDC = nullptr;
    bool  active = false;
};
PinnedCam g_pinned[MAX_PINNED] = {};
std::atomic<int> g_numPinned(0);

// ===================== SYNC & THREADING =====================
CRITICAL_SECTION g_cs;
std::atomic<bool> g_runWorker{ true };

// ===================== SHARED MEMORY =====================
unsigned char* pSharedBuffer = nullptr;
HANDLE          hMapFile = nullptr;

// ===================== INSPECTOR WINDOW =====================
HWND  g_hInspector = nullptr;
HWND  g_hListBox = nullptr;
HWND  g_hPreview = nullptr;
HWND  g_hPinBtn = nullptr;
HWND  g_hInfoLabel = nullptr;
std::atomic<int> g_selectedTex(-1);

// ===================== HELPERS =====================
static void EnsureGLFunctions()
{
    if (glFunctionsLoaded) return;
    HMODULE hOgl = GetModuleHandleA("opengl32.dll");
    if (!hOgl) return;

    glGetIntegerv_fn = (tglGetIntegerv)GetProcAddress(hOgl, "glGetIntegerv");
    glBindTexture_fn = (tglBindTexture)GetProcAddress(hOgl, "glBindTexture");
    glGetTexImage_fn = (tglGetTexImage)GetProcAddress(hOgl, "glGetTexImage");
    glGetTexLevelParam_fn = (tglTexLevelParam)GetProcAddress(hOgl, "glGetTexLevelParameteriv");
    glPixelStorei_fn = (tglPixelStorei)GetProcAddress(hOgl, "glPixelStorei");

    glGenBuffers_fn = (tglGenBuffers)wglGetProcAddress("glGenBuffers");
    glBindBuffer_fn = (tglBindBuffer)wglGetProcAddress("glBindBuffer");
    glBufferData_fn = (tglBufferData)wglGetProcAddress("glBufferData");
    glMapBuffer_fn = (tglMapBuffer)wglGetProcAddress("glMapBuffer");
    glUnmapBuffer_fn = (tglUnmapBuffer)wglGetProcAddress("glUnmapBuffer");
    glGenerateMipmap_fn = (tglGenerateMipmap)wglGetProcAddress("glGenerateMipmap");

    glFunctionsLoaded = true;
}

static inline float HalfToFloat(unsigned short h)
{
    int sign = (h >> 15) & 0x0001;
    int exp = (h >> 10) & 0x001F;
    int mant = h & 0x03FF;

    if (exp == 0) {
        if (mant == 0) return sign ? -0.0f : 0.0f;
        return (sign ? -1.0f : 1.0f) * std::ldexp((float)mant / 1024.0f, -14);
    }
    else if (exp == 31) {
        return 0.0f;
    }
    float val = (1.0f + (float)mant / 1024.0f) * std::ldexp(1.0f, exp - 15);
    return sign ? -val : val;
}

// PARALLEL PROCESSING WORKER
DWORD WINAPI ImageProcessingWorker(LPVOID)
{
    unsigned short* localRaw = (unsigned short*)malloc(RAW_HALF_SIZE);
    unsigned char* localRGB = (unsigned char*)malloc(BUFFER_SIZE);
    if (!localRaw || !localRGB) return 1;

    while (g_runWorker.load()) {
        int n = g_numTextures.load();
        int sel = g_selectedTex.load();
        bool processedSomething = false;

        for (int i = 0; i < n; i++) {
            // FIX: Обрабатываем текстуру, если она ЕЙ САМОЙ выставлен флаг pinned ИЛИ она выбрана в UI
            if ((i == sel || g_textures[i].pinned) && g_textures[i].hasNewRawData.load()) {

                EnterCriticalSection(&g_cs);
                if (g_textures[i].rawHalfBuffer) {
                    memcpy(localRaw, g_textures[i].rawHalfBuffer, RAW_HALF_SIZE);
                }
                g_textures[i].hasNewRawData.store(false);
                LeaveCriticalSection(&g_cs);

                const float exposure = 1.6f;
                const float gamma = 1.0f / 2.2f;

                for (int y = 0; y < CAM_HEIGHT; y++) {
                    int srcY = (CAM_HEIGHT - 1 - y);
                    for (int x = 0; x < CAM_WIDTH; x++) {
                        int srcIdx = (srcY * CAM_WIDTH + x) * 4;
                        int dstIdx = (y * CAM_WIDTH + x) * 4;

                        float r = HalfToFloat(localRaw[srcIdx + 0]);
                        float g = HalfToFloat(localRaw[srcIdx + 1]);
                        float b = HalfToFloat(localRaw[srcIdx + 2]);

                        r = 1.0f - std::exp(-r * exposure);
                        g = 1.0f - std::exp(-g * exposure);
                        b = 1.0f - std::exp(-b * exposure);

                        localRGB[dstIdx + 0] = (unsigned char)(std::pow(b, gamma) * 255.0f);
                        localRGB[dstIdx + 1] = (unsigned char)(std::pow(g, gamma) * 255.0f);
                        localRGB[dstIdx + 2] = (unsigned char)(std::pow(r, gamma) * 255.0f);
                        localRGB[dstIdx + 3] = 255;
                    }
                }

                EnterCriticalSection(&g_cs);
                if (g_textures[i].frontBuffer) {
                    memcpy(g_textures[i].frontBuffer, localRGB, BUFFER_SIZE);
                }
                LeaveCriticalSection(&g_cs);

                // FIX: Пишем в Shared Memory строго по привязке к слоту окна, игнорируя UI выборку
                for (int p = 0; p < MAX_PINNED; p++) {
                    if (g_pinned[p].active && g_pinned[p].texIdx == i && pSharedBuffer) {
                        unsigned char* pSlotDestination = pSharedBuffer + (p * BUFFER_SIZE);
                        memcpy(pSlotDestination, localRGB, BUFFER_SIZE);
                    }
                }
                processedSomething = true;
            }
        }

        if (!processedSomething) {
            Sleep(2);
        }
    }

    free(localRaw);
    free(localRGB);
    return 0;
}

// INSTANT REQUEST FROM GAME
static void ReadbackTextureAsync(unsigned int texID, TexEntry& te)
{
    if (!glFunctionsLoaded || !glBindBuffer_fn || !glMapBuffer_fn || !te.rawHalfBuffer) return;

    if (!g_pboInitialized && glGenBuffers_fn) {
        glGenBuffers_fn(2, g_pbo);
        glBindBuffer_fn(GL_PIXEL_PACK_BUFFER, g_pbo[0]);
        glBufferData_fn(GL_PIXEL_PACK_BUFFER, RAW_HALF_SIZE, nullptr, GL_STREAM_READ);
        glBindBuffer_fn(GL_PIXEL_PACK_BUFFER, g_pbo[1]);
        glBufferData_fn(GL_PIXEL_PACK_BUFFER, RAW_HALF_SIZE, nullptr, GL_STREAM_READ);
        glBindBuffer_fn(GL_PIXEL_PACK_BUFFER, 0);
        g_pboInitialized = true;
    }

    if (!g_pboInitialized) return;

    int nextPboIdx = (g_pboIdx + 1) % 2;

    glBindTexture_fn(GL_TEXTURE_2D, texID);
    glBindBuffer_fn(GL_PIXEL_PACK_BUFFER, g_pbo[g_pboIdx]);

    int oldAlignment = 4;
    if (glPixelStorei_fn && glGetIntegerv_fn) {
        glGetIntegerv_fn(GL_PACK_ALIGNMENT, &oldAlignment);
        glPixelStorei_fn(GL_PACK_ALIGNMENT, 1);
    }

    glGetTexImage_fn(GL_TEXTURE_2D, 0, GL_RGBA, GL_HALF_FLOAT, nullptr);

    glBindBuffer_fn(GL_PIXEL_PACK_BUFFER, g_pbo[nextPboIdx]);
    unsigned short* src = (unsigned short*)glMapBuffer_fn(GL_PIXEL_PACK_BUFFER, GL_READ_ONLY);

    if (src) {
        EnterCriticalSection(&g_cs);
        memcpy(te.rawHalfBuffer, src, RAW_HALF_SIZE);
        te.hasNewRawData.store(true);
        LeaveCriticalSection(&g_cs);
        glUnmapBuffer_fn(GL_PIXEL_PACK_BUFFER);
    }

    if (glPixelStorei_fn) glPixelStorei_fn(GL_PACK_ALIGNMENT, oldAlignment);
    glBindBuffer_fn(GL_PIXEL_PACK_BUFFER, 0);

    g_pboIdx = nextPboIdx;
}

static int RegisterOrFindTex(unsigned int texID, int w, int h, int fmt)
{
    if (w != CAM_WIDTH || h != CAM_HEIGHT || fmt != GL_RGBA16F) return -1;

    int n = g_numTextures.load();
    for (int i = 0; i < n; i++)
        if (g_textures[i].texID == texID) return i;

    if (n >= MAX_CAMS) return -1;

    g_textures[n].rawHalfBuffer = (unsigned short*)malloc(RAW_HALF_SIZE);
    g_textures[n].frontBuffer = (unsigned char*)calloc(BUFFER_SIZE, 1);

    if (!g_textures[n].rawHalfBuffer || !g_textures[n].frontBuffer) {
        if (g_textures[n].rawHalfBuffer) free(g_textures[n].rawHalfBuffer);
        if (g_textures[n].frontBuffer) free(g_textures[n].frontBuffer);
        return -1;
    }

    g_textures[n].texID = texID;
    g_textures[n].internalFmt = fmt;
    g_textures[n].w = w;
    g_textures[n].h = h;
    g_textures[n].pinned = false;

    sprintf_s(g_textures[n].label, "CAMERA #%d (ID: %u)", n, texID);
    g_numTextures.fetch_add(1);
    return n;
}

static void UpdateListBox()
{
    if (!g_hListBox) return;
    SendMessageA(g_hListBox, LB_RESETCONTENT, 0, 0);
    int n = g_numTextures.load();
    for (int i = 0; i < n; i++) {
        char buf[128];
        sprintf_s(buf, "%s%s", g_textures[i].pinned ? "[PIN] " : "      ", g_textures[i].label);
        SendMessageA(g_hListBox, LB_ADDSTRING, 0, (LPARAM)buf);
    }
}

// ===================== WINDOWS WNDPROCS =====================
LRESULT CALLBACK PinnedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LONG_PTR slotPtr = GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    int slot = static_cast<int>(slotPtr);

    if (msg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        // FIX: Окно берет свой ID камеры прямо из структуры закрепленного слота
        if (slot >= 0 && slot < MAX_PINNED && g_pinned[slot].active) {
            int texIdx = g_pinned[slot].texIdx;
            if (texIdx >= 0 && texIdx < g_numTextures.load()) {
                TexEntry& te = g_textures[texIdx];
                if (te.frontBuffer) {
                    BITMAPINFO bmi = {};
                    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                    bmi.bmiHeader.biWidth = CAM_WIDTH;
                    bmi.bmiHeader.biHeight = -CAM_HEIGHT;
                    bmi.bmiHeader.biPlanes = 1;
                    bmi.bmiHeader.biBitCount = 32;
                    bmi.bmiHeader.biCompression = BI_RGB;

                    RECT rc;
                    GetClientRect(hwnd, &rc);

                    EnterCriticalSection(&g_cs);
                    StretchDIBits(hdc, 0, 0, rc.right, rc.bottom, 0, 0, CAM_WIDTH, CAM_HEIGHT, te.frontBuffer, &bmi, DIB_RGB_COLORS, SRCCOPY);
                    LeaveCriticalSection(&g_cs);
                }
            }
        }
        EndPaint(hwnd, &ps);
        return 0;
    }

    if (msg == WM_CLOSE) {
        if (slot >= 0 && slot < MAX_PINNED) {
            int texIdx = g_pinned[slot].texIdx;
            if (texIdx >= 0 && texIdx < MAX_CAMS) {
                g_textures[texIdx].pinned = false;
            }
            g_pinned[slot].active = false;
            g_pinned[slot].texIdx = -1;
            g_pinned[slot].hWindow = nullptr;
            g_pinned[slot].hDC = nullptr;
            g_numPinned.fetch_sub(1);
        }
        DestroyWindow(hwnd);
        UpdateListBox();
        return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static void CreatePinnedWindow(int texIdx)
{
    if (texIdx < 0 || texIdx >= g_numTextures.load()) return;

    for (int i = 0; i < MAX_PINNED; i++) {
        if (g_pinned[i].active && g_pinned[i].texIdx == texIdx) {
            SetForegroundWindow(g_pinned[i].hWindow);
            return;
        }
    }

    int slot = -1;
    for (int i = 0; i < MAX_PINNED; i++) {
        if (!g_pinned[i].active) { slot = i; break; }
    }
    if (slot < 0) return;

    char title[128];
    sprintf_s(title, "LIVE STREAM: %s", g_textures[texIdx].label);

    HWND hw = CreateWindowA("PinnedCamWnd", title, WS_OVERLAPPEDWINDOW, 50 + slot * 40, 50 + slot * 40, CAM_WIDTH / 2 + 16, CAM_HEIGHT / 2 + 39, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    if (hw) {
        SetWindowLongPtrA(hw, GWLP_USERDATA, (LONG_PTR)slot);
        ShowWindow(hw, SW_SHOW);

        g_pinned[slot].texIdx = texIdx;
        g_pinned[slot].hWindow = hw;
        g_pinned[slot].hDC = GetDC(hw);
        g_pinned[slot].active = true;
        g_numPinned.fetch_add(1);

        g_textures[texIdx].pinned = true;
        UpdateListBox();
    }
}

#define IDC_LISTBOX  101
#define IDC_PIN_BTN  102
#define IDC_PREVIEW  103
#define IDC_INFO     104

LRESULT CALLBACK InspectorWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        g_hListBox = CreateWindowA("LISTBOX", nullptr, WS_CHILD | WS_VISIBLE | WS_BORDER | LBS_NOTIFY | WS_VSCROLL, 5, 5, 340, 140, hwnd, (HMENU)IDC_LISTBOX, nullptr, nullptr);
        g_hInfoLabel = CreateWindowA("STATIC", "Waiting for in-game camera activation...", WS_CHILD | WS_VISIBLE, 5, 150, 340, 40, hwnd, (HMENU)IDC_INFO, nullptr, nullptr);
        g_hPinBtn = CreateWindowA("BUTTON", "Open clean stream window", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 5, 195, 180, 30, hwnd, (HMENU)IDC_PIN_BTN, nullptr, nullptr);
        g_hPreview = CreateWindowA("STATIC", nullptr, WS_CHILD | WS_VISIBLE | WS_BORDER | SS_OWNERDRAW, 5, 235, 340, 190, hwnd, (HMENU)IDC_PREVIEW, nullptr, nullptr);
        return 0;
    case WM_DRAWITEM: {
        DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
        int sel = g_selectedTex.load();
        if (dis->CtlID == IDC_PREVIEW && sel >= 0 && sel < g_numTextures.load() && g_textures[sel].frontBuffer) {
            BITMAPINFO bmi = {};
            bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            bmi.bmiHeader.biWidth = CAM_WIDTH;
            bmi.bmiHeader.biHeight = -CAM_HEIGHT;
            bmi.bmiHeader.biPlanes = 1;
            bmi.bmiHeader.biBitCount = 32;
            bmi.bmiHeader.biCompression = BI_RGB;
            RECT& rc = dis->rcItem;

            EnterCriticalSection(&g_cs);
            StretchDIBits(dis->hDC, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top, 0, 0, CAM_WIDTH, CAM_HEIGHT, g_textures[sel].frontBuffer, &bmi, DIB_RGB_COLORS, SRCCOPY);
            LeaveCriticalSection(&g_cs);
        }
        return TRUE;
    }
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_LISTBOX && HIWORD(wParam) == LBN_SELCHANGE) {
            int sel = (int)SendMessageA(g_hListBox, LB_GETCURSEL, 0, 0);
            if (sel >= 0 && sel < g_numTextures.load()) {
                g_selectedTex.store(sel);
                char info[256];
                sprintf_s(info, "Active video\nResolution: %dx%d (HDR RGBA16F)", g_textures[sel].w, g_textures[sel].h);
                SetWindowTextA(g_hInfoLabel, info);
            }
        }
        if (LOWORD(wParam) == IDC_PIN_BTN) {
            int sel = g_selectedTex.load();
            if (sel >= 0) CreatePinnedWindow(sel);
        }
        return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

DWORD WINAPI WindowThread(LPVOID)
{
    WNDCLASSA wc = { 0 }; wc.lpfnWndProc = InspectorWndProc; wc.lpszClassName = "SWCamInspector"; wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1); RegisterClassA(&wc);
    WNDCLASSA wc2 = { 0 }; wc2.lpfnWndProc = PinnedWndProc; wc2.lpszClassName = "PinnedCamWnd"; wc2.style = CS_OWNDC; RegisterClassA(&wc2);
    g_hInspector = CreateWindowA("SWCamInspector", "Stormworks Camera Link", WS_OVERLAPPEDWINDOW, 900, 50, 366, 475, nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);
    if (!g_hInspector) return 1;
    ShowWindow(g_hInspector, SW_SHOW);

    int lastCount = 0; MSG msg = {};
    while (true) {
        int n = g_numTextures.load();
        if (n != lastCount) { UpdateListBox(); lastCount = n; }

        int sel = g_selectedTex.load();
        if (g_hPreview && sel >= 0) InvalidateRect(g_hPreview, nullptr, FALSE);

        for (int i = 0; i < MAX_PINNED; i++) {
            if (g_pinned[i].active && g_pinned[i].hWindow) {
                InvalidateRect(g_pinned[i].hWindow, nullptr, FALSE);
            }
        }

        while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) return 0;
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        }

        if (GetAsyncKeyState(VK_DELETE) & 0x8000) {
            g_runWorker.store(false);
            MH_DisableHook(MH_ALL_HOOKS);
            MH_Uninitialize();
            FreeLibraryAndExitThread(GetModuleHandleA(nullptr), 0);
        }
        Sleep(16);
    }
    return 0;
}

// ===================== GL HOOKS =====================
void __stdcall hkGlDrawElements(unsigned int mode, int count, unsigned int type, const void* indices)
{
    if (glFunctionsLoaded && glGetIntegerv_fn && glGetTexLevelParam_fn) {
        int texBinding = 0; glGetIntegerv_fn(GL_TEXTURE_BINDING_2D, &texBinding);
        if (texBinding > 0) {
            int w = 0, h = 0, fmt = 0;
            glGetTexLevelParam_fn(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
            glGetTexLevelParam_fn(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
            glGetTexLevelParam_fn(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &fmt);
            RegisterOrFindTex((unsigned int)texBinding, w, h, fmt);
        }
    }
    if (oGlDrawElements_orig) oGlDrawElements_orig(mode, count, type, indices);
}

void __stdcall hkGlBindTexture(unsigned int target, unsigned int texture)
{
    if (oGlBindTexture_orig) oGlBindTexture_orig(target, texture);
    if (target == GL_TEXTURE_2D && texture > 0 && glGetTexLevelParam_fn) {
        int w = 0, h = 0, fmt = 0;
        glGetTexLevelParam_fn(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
        glGetTexLevelParam_fn(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
        glGetTexLevelParam_fn(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT, &fmt);
        RegisterOrFindTex(texture, w, h, fmt);
    }
}

BOOL __stdcall hkWglSwapBuffers(HDC hDc)
{
    EnsureGLFunctions();

    if (!pSharedBuffer) {
        hMapFile = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, TOTAL_SHARED_SIZE, "StormworksCamMap");
        if (hMapFile && hMapFile != INVALID_HANDLE_VALUE)
            pSharedBuffer = (unsigned char*)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, TOTAL_SHARED_SIZE);
    }

    int n = g_numTextures.load();
    int sel = g_selectedTex.load();

    for (int i = 0; i < n; i++) {
        // FIX: Снимаем текстуру из буфера PBO, если она закреплена за окном ИЛИ выделена в UI
        if (i == sel || g_textures[i].pinned) {
            if (g_textures[i].pinned && !g_textures[i].hasNewRawData.load() && glBindTexture_fn) {
                glBindTexture_fn(GL_TEXTURE_2D, g_textures[i].texID);
                if (glGenerateMipmap_fn) {
                    glGenerateMipmap_fn(GL_TEXTURE_2D);
                }
            }

            if (g_textures[i].rawHalfBuffer && g_textures[i].frontBuffer) {
                ReadbackTextureAsync(g_textures[i].texID, g_textures[i]);
            }
        }
    }

    if (oWglSwapBuffers) return oWglSwapBuffers(hDc);
    return FALSE;
}

DWORD WINAPI InitThread(LPVOID hMod)
{
    InitializeCriticalSection(&g_cs);
    HMODULE hOgl = nullptr;
    while (!hOgl) { hOgl = GetModuleHandleA("opengl32.dll"); Sleep(100); }
    if (MH_Initialize() != MH_OK) return 1;

#define HOOK(name, hook, orig) { void* p = GetProcAddress(hOgl, name); if (p) { MH_CreateHook(p, hook, reinterpret_cast<LPVOID*>(orig)); MH_EnableHook(p); } }
    HOOK("wglSwapBuffers", &hkWglSwapBuffers, &oWglSwapBuffers);
    HOOK("glDrawElements", &hkGlDrawElements, &oGlDrawElements_orig);
    HOOK("glBindTexture", &hkGlBindTexture, &oGlBindTexture_orig);
#undef HOOK

    CreateThread(nullptr, 0, WindowThread, nullptr, 0, nullptr);
    CreateThread(nullptr, 0, ImageProcessingWorker, nullptr, 0, nullptr);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, InitThread, (LPVOID)hModule, 0, nullptr);
    }
    else if (reason == DLL_PROCESS_DETACH) {
        g_runWorker.store(false);
        MH_DisableHook(MH_ALL_HOOKS);
        MH_Uninitialize();

        int n = g_numTextures.load();
        for (int i = 0; i < n; i++) {
            if (g_textures[i].rawHalfBuffer) free(g_textures[i].rawHalfBuffer);
            if (g_textures[i].frontBuffer) free(g_textures[i].frontBuffer);
        }

        if (pSharedBuffer) UnmapViewOfFile(pSharedBuffer);
        if (hMapFile) CloseHandle(hMapFile);
        DeleteCriticalSection(&g_cs);
    }
    return TRUE;
}