// Luxor 3 fix: proxy D3D8.dll
// - Hooks user32!ChangeDisplaySettingsA: the game requests fullscreen modes with
//   DM_DISPLAYFREQUENCY set and a frequency of 0 Hz, which Windows 10/11 reject
//   with DISP_CHANGE_BADMODE. We request 60 Hz instead (or let Windows pick a
//   valid rate if 60 Hz is not available).
// - Hooks user32!EnumDisplaySettingsA: the game asserts (MAX_VID_MODES) when the
//   monitor reports too many display modes; we report one 32 bpp mode per resolution.
// - Raises the system timer resolution to 1 ms: the game paces frames with
//   Sleep(), which on modern Windows defaults to ~15.6 ms granularity.
// - Forces vsync on the Direct3D device (the game's defaults mean no vsync on
//   modern Windows: hundreds of fps and tearing) and 60 Hz in fullscreen.
// - Forwards Direct3DCreate8 to the system d3d8.dll, or to dgVoodoo2 if it is
//   present next to the game renamed to dgd3d8.dll (optional).
// - Test mode: if luxorfix_test.txt exists next to the game, frame times are
//   logged to luxorfix.log and the game exits after the given number of seconds.
//   A line "notimer" in that file disables the timer fix for comparison.
#include <windows.h>
#include <stdio.h>
#include <string.h>

typedef void *(WINAPI *Direct3DCreate8_t)(UINT);
typedef HRESULT(WINAPI *CreateDevice_t)(void *, UINT, int, HWND, DWORD, void *, void **);
typedef HRESULT(WINAPI *Present_t)(void *, const RECT *, const RECT *, HWND, const void *);

static Direct3DCreate8_t real_create;
static CreateDevice_t real_create_device;
static Present_t real_present;

static int test_mode, test_seconds = 20, timer_fix = 1;
static char log_path[MAX_PATH];
static LARGE_INTEGER freq, start, last;
static double frame_ms[100000];
static int frames;

static LONG WINAPI hook_ChangeDisplaySettingsA(DEVMODEA *dm, DWORD flags)
{
    if (dm) {
        DEVMODEA copy = *dm;
        if (!(copy.dmFields & DM_DISPLAYFREQUENCY) || copy.dmDisplayFrequency <= 1) {
            // The game was tuned for 60 Hz; prefer it so frame pacing is even.
            copy.dmFields |= DM_DISPLAYFREQUENCY;
            copy.dmDisplayFrequency = 60;
            if (ChangeDisplaySettingsExA(NULL, &copy, NULL, CDS_TEST, NULL) != DISP_CHANGE_SUCCESSFUL)
                copy.dmFields &= ~DM_DISPLAYFREQUENCY;
        }
        return ChangeDisplaySettingsExA(NULL, &copy, NULL, flags, NULL);
    }
    return ChangeDisplaySettingsExA(NULL, NULL, NULL, flags, NULL);
}

// The game stores display modes in a fixed-size array and asserts when the
// monitor reports too many (modern high-refresh monitors report hundreds).
// Expose only one 32 bpp entry per resolution, preferring 60 Hz.
#define MAX_MODES 24
static DEVMODEA modes[MAX_MODES];
static int num_modes = -1;

static void build_mode_list(void)
{
    DEVMODEA dm;
    DWORD i;
    int j;
    num_modes = 0;
    for (i = 0;; i++) {
        ZeroMemory(&dm, sizeof dm);
        dm.dmSize = sizeof dm;
        if (!EnumDisplaySettingsExA(NULL, i, &dm, 0))
            break;
        if (dm.dmBitsPerPel != 32 || dm.dmPelsWidth < 640 || dm.dmPelsHeight < 480)
            continue;
        for (j = 0; j < num_modes; j++)
            if (modes[j].dmPelsWidth == dm.dmPelsWidth && modes[j].dmPelsHeight == dm.dmPelsHeight)
                break;
        if (j < num_modes) {
            if (dm.dmDisplayFrequency == 60)
                modes[j] = dm;
        } else if (num_modes < MAX_MODES) {
            modes[num_modes++] = dm;
        }
    }
}

static BOOL WINAPI hook_EnumDisplaySettingsA(LPCSTR device, DWORD index, DEVMODEA *dm)
{
    // Like the real EnumDisplaySettingsA, ignore whatever the caller left in
    // dmSize/dmDriverExtra (the game leaves them uninitialized).
    dm->dmSize = sizeof(DEVMODEA);
    dm->dmDriverExtra = 0;
    if (index == ENUM_CURRENT_SETTINGS || index == ENUM_REGISTRY_SETTINGS || device)
        return EnumDisplaySettingsExA(device, index, dm, 0);
    if (num_modes < 0)
        build_mode_list();
    if ((int)index >= num_modes)
        return FALSE;
    *dm = modes[index];
    return TRUE;
}

static void jmp_hook(const char *name, void *hook)
{
    BYTE *target = (BYTE *)GetProcAddress(GetModuleHandleA("user32.dll"), name);
    DWORD old;
    if (!target || !VirtualProtect(target, 5, PAGE_EXECUTE_READWRITE, &old))
        return;
    target[0] = 0xE9; // jmp rel32
    *(DWORD *)(target + 1) = (DWORD)((BYTE *)hook - (target + 5));
    VirtualProtect(target, 5, old, &old);
    FlushInstructionCache(GetCurrentProcess(), target, 5);
}

static void install_hook(void)
{
    jmp_hook("ChangeDisplaySettingsA", hook_ChangeDisplaySettingsA);
    jmp_hook("EnumDisplaySettingsA", hook_EnumDisplaySettingsA);
}

static void patch_vtable(void *obj, int index, void *hook, void **orig)
{
    void **vtbl = *(void ***)obj;
    DWORD old;
    if (vtbl[index] == hook)
        return;
    *orig = vtbl[index];
    VirtualProtect(&vtbl[index], sizeof(void *), PAGE_READWRITE, &old);
    vtbl[index] = hook;
    VirtualProtect(&vtbl[index], sizeof(void *), old, &old);
}

static int cmp_double(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x > y) - (x < y);
}

static void write_report(void)
{
    static double sorted[100000];
    FILE *f = fopen(log_path, "a");
    double total = 0;
    int i, over20 = 0, over34 = 0;
    if (!f || frames < 2)
        return;
    for (i = 0; i < frames; i++) {
        total += frame_ms[i];
        sorted[i] = frame_ms[i];
        if (frame_ms[i] > 20.0) over20++;
        if (frame_ms[i] > 34.0) over34++;
    }
    qsort(sorted, frames, sizeof(double), cmp_double);
    fprintf(f, "timer_fix=%d frames=%d avg_fps=%.1f avg_ms=%.2f median_ms=%.2f p99_ms=%.2f max_ms=%.2f frames_over_20ms=%d frames_over_34ms=%d\n",
            timer_fix, frames, 1000.0 * frames / total, total / frames, sorted[frames / 2],
            sorted[frames * 99 / 100], sorted[frames - 1], over20, over34);
    fclose(f);
}

static HRESULT WINAPI hook_Present(void *dev, const RECT *src, const RECT *dst, HWND wnd, const void *rgn)
{
    HRESULT hr = real_present(dev, src, dst, wnd, rgn);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (start.QuadPart == 0) {
        start = last = now;
    } else {
        double since_start = (double)(now.QuadPart - start.QuadPart) / freq.QuadPart;
        // skip the first 5 s (loading), then record
        if (since_start > 5.0 && frames < 100000)
            frame_ms[frames++] = 1000.0 * (now.QuadPart - last.QuadPart) / freq.QuadPart;
        last = now;
        if (since_start > test_seconds + 5) {
            write_report();
            ExitProcess(0);
        }
    }
    return hr;
}

// D3DPRESENT_PARAMETERS (D3D8), as DWORDs
enum { PP_COUNT = 3, PP_MULTISAMPLE = 4, PP_SWAPEFFECT = 5, PP_WINDOWED = 7, PP_REFRESH = 11, PP_INTERVAL = 12, PP_SIZE = 13 };
#define D3DSWAPEFFECT_DISCARD 1
#define D3DSWAPEFFECT_COPY 3
#define D3DSWAPEFFECT_COPY_VSYNC 4

// The game asks for the default present interval / plain DISCARD swaps, which
// on modern Windows means no vsync: hundreds of fps and tearing. Request vsync
// (and 60 Hz in fullscreen, which the game was made for).
static void force_vsync(DWORD *pp)
{
    if (!pp[PP_WINDOWED]) {
        if (pp[PP_REFRESH] == 0)
            pp[PP_REFRESH] = 60;
        pp[PP_INTERVAL] = 1; // D3DPRESENT_INTERVAL_ONE
    } else if ((pp[PP_SWAPEFFECT] == D3DSWAPEFFECT_DISCARD || pp[PP_SWAPEFFECT] == D3DSWAPEFFECT_COPY) &&
               pp[PP_COUNT] <= 1 && pp[PP_MULTISAMPLE] == 0) {
        pp[PP_SWAPEFFECT] = D3DSWAPEFFECT_COPY_VSYNC;
    }
}

typedef HRESULT(WINAPI *Reset_t)(void *, void *);
static Reset_t real_reset;

static void log_params(const char *what, const DWORD *p, HRESULT hr)
{
    FILE *f;
    if (!test_mode || !(f = fopen(log_path, "a")))
        return;
    fprintf(f, "%s: %lux%lu swap=%lu windowed=%lu refresh=%lu interval=%lx hr=%lx\n",
            what, p[0], p[1], p[PP_SWAPEFFECT], p[PP_WINDOWED], p[PP_REFRESH], p[PP_INTERVAL], (unsigned long)hr);
    fclose(f);
}

static HRESULT WINAPI hook_Reset(void *dev, void *pp)
{
    DWORD orig[PP_SIZE];
    HRESULT hr;
    CopyMemory(orig, pp, sizeof orig);
    force_vsync((DWORD *)pp);
    hr = real_reset(dev, pp);
    log_params("Reset", pp, hr);
    if (FAILED(hr)) {
        CopyMemory(pp, orig, sizeof orig);
        hr = real_reset(dev, pp);
    }
    return hr;
}

static HRESULT WINAPI hook_CreateDevice(void *d3d, UINT adapter, int type, HWND wnd, DWORD flags, void *pp, void **dev)
{
    DWORD orig[PP_SIZE];
    HRESULT hr;
    CopyMemory(orig, pp, sizeof orig);
    force_vsync((DWORD *)pp);
    hr = real_create_device(d3d, adapter, type, wnd, flags, pp, dev);
    log_params("CreateDevice", pp, hr);
    if (FAILED(hr)) {
        CopyMemory(pp, orig, sizeof orig);
        hr = real_create_device(d3d, adapter, type, wnd, flags, pp, dev);
    }
    if (SUCCEEDED(hr) && dev && *dev) {
        patch_vtable(*dev, 14, hook_Reset, (void **)&real_reset);
        if (test_mode)
            patch_vtable(*dev, 15, hook_Present, (void **)&real_present);
    }
    return hr;
}

static void load_real(void)
{
    char path[MAX_PATH];
    HMODULE h = LoadLibraryA("dgd3d8.dll");
    if (!h) {
        GetSystemDirectoryA(path, MAX_PATH);
        lstrcatA(path, "\\d3d8.dll");
        h = LoadLibraryA(path);
    }
    if (h)
        real_create = (Direct3DCreate8_t)GetProcAddress(h, "Direct3DCreate8");
}

void *WINAPI Direct3DCreate8(UINT sdk)
{
    void *d3d;
    static int timer_set;
    if (timer_fix && !timer_set) {
        // done here rather than in DllMain to stay out of the loader lock
        typedef UINT(WINAPI *timeBeginPeriod_t)(UINT);
        timeBeginPeriod_t tbp = (timeBeginPeriod_t)GetProcAddress(LoadLibraryA("winmm.dll"), "timeBeginPeriod");
        if (tbp)
            tbp(1);
        timer_set = 1;
    }
    if (!real_create)
        load_real();
    d3d = real_create ? real_create(sdk) : NULL;
    if (d3d)
        patch_vtable(d3d, 15, hook_CreateDevice, (void **)&real_create_device);
    return d3d;
}

static void read_test_config(HINSTANCE inst)
{
    char dir[MAX_PATH], cfg[MAX_PATH], line[128];
    char *slash;
    FILE *f;
    GetModuleFileNameA(inst, dir, MAX_PATH);
    slash = strrchr(dir, '\\');
    if (slash)
        slash[1] = 0;
    lstrcpyA(cfg, dir);
    lstrcatA(cfg, "luxorfix_test.txt");
    f = fopen(cfg, "r");
    if (!f)
        return;
    test_mode = 1;
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, "notimer", 7) == 0)
            timer_fix = 0;
        else if (atoi(line) > 0)
            test_seconds = atoi(line);
    }
    fclose(f);
    lstrcpyA(log_path, dir);
    lstrcatA(log_path, "luxorfix.log");
    QueryPerformanceFrequency(&freq);
}

BOOL WINAPI DllMain(HINSTANCE inst, DWORD reason, LPVOID reserved)
{
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(inst);
        read_test_config(inst);
        install_hook();
    }
    return TRUE;
}
