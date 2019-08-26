//----------------------------------------------------------------------------------------------------------------------
// 2d game engine Win32 stub
//----------------------------------------------------------------------------------------------------------------------

#include <cstdint>

static int kWidth = 800;
static int kHeight = 600;
static int kScale = 2;
static char kTitle[] = "2D Game Engine";

//----------------------------------------------------------------------------------------------------------------------
// Basic types and definitions
//----------------------------------------------------------------------------------------------------------------------

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float    f32;
typedef double   f64;

#define local_persist static
#define global_variable static

//----------------------------------------------------------------------------------------------------------------------
// Includes
//----------------------------------------------------------------------------------------------------------------------

#include <Windows.h>
#include <windowsx.h>
#undef min
#undef max

#include "game_impl.h"

//----------------------------------------------------------------------------------------------------------------------
// Structures
//----------------------------------------------------------------------------------------------------------------------

struct Win32OffScreenBuffer
{
    BITMAPINFO info;
    void* memory;
    int width;
    int height;
    int pitch;
};

struct Win32WindowDimension
{
    int width;
    int height;
};

//----------------------------------------------------------------------------------------------------------------------
// Global state
//----------------------------------------------------------------------------------------------------------------------

global_variable bool gRunning = true;
global_variable Win32OffScreenBuffer gGlobalBackBuffer;
global_variable GameInput gInput;

//----------------------------------------------------------------------------------------------------------------------
// Window management
//----------------------------------------------------------------------------------------------------------------------

Win32WindowDimension win32GetWindowDimension(HWND wnd)
{
    RECT clientRect;
    GetClientRect(wnd, &clientRect);
    return {
        clientRect.right - clientRect.left,
        clientRect.bottom - clientRect.top
    };
}

void win32ResizeDIBSection(Win32OffScreenBuffer& buffer, int width, int height)
{
    if (buffer.memory)
    {
        VirtualFree(buffer.memory, 0, MEM_RELEASE);
    }

    buffer.width = width;
    buffer.height = height;
    int bytesPerPixel = 4;

    // Create the new DIB Section
    buffer.info.bmiHeader.biSize = sizeof(buffer.info.bmiHeader);
    buffer.info.bmiHeader.biWidth = buffer.width;
    buffer.info.bmiHeader.biHeight = -buffer.height;
    buffer.info.bmiHeader.biPlanes = 1;
    buffer.info.bmiHeader.biBitCount = 32;
    buffer.info.bmiHeader.biCompression = BI_RGB;

    int bitmapMemorySize = (buffer.width * buffer.height) * bytesPerPixel;
    buffer.memory = VirtualAlloc(0, bitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

void win32DisplayBufferInWindow(const Win32OffScreenBuffer& buffer, HDC dc, int windowWidth, int windowHeight)
{
    StretchDIBits(
        dc,
        0, 0, windowWidth, windowHeight,
        0, 0, buffer.width, buffer.height,
        buffer.memory,
        &buffer.info,
        DIB_RGB_COLORS,
        SRCCOPY);
}

//----------------------------------------------------------------------------------------------------------------------

LRESULT CALLBACK win32MainWindowCallback(HWND wnd, UINT msg, WPARAM w, LPARAM l)
{
    LRESULT result = 0;

    switch (msg)
    {
    case WM_CLOSE:
        gRunning = false;
        break;

    case WM_ACTIVATEAPP:
        break;

    case WM_DESTROY:
        gRunning = false;
        break;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYUP:
    case WM_KEYDOWN:
        break;

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(wnd, &ps);
        Win32WindowDimension dimension = win32GetWindowDimension(wnd);
        win32DisplayBufferInWindow(gGlobalBackBuffer, dc, dimension.width, dimension.height);
        EndPaint(wnd, &ps);
    }

    default:
        result = DefWindowProcA(wnd, msg, w, l);
    }

    return result;
}

//----------------------------------------------------------------------------------------------------------------------
// Time handling
//----------------------------------------------------------------------------------------------------------------------

using TimePoint = LARGE_INTEGER;
using TimePeriod = LARGE_INTEGER;

TimePoint timeNow()
{
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t;
}

TimePeriod timePeriod(TimePoint a, TimePoint b)
{
    LARGE_INTEGER t;
    t.QuadPart = b.QuadPart - a.QuadPart;
    return t;
}

f64 timeToSecs(TimePeriod period)
{
    LARGE_INTEGER freq;
    QueryPerformanceFrequency(&freq);
    return (f64)period.QuadPart / (f64)freq.QuadPart;
}

//----------------------------------------------------------------------------------------------------------------------
// Main Loop
//----------------------------------------------------------------------------------------------------------------------

void win32ProcessPendingMessages()
{
    MSG message;

    while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
    {
        if (message.message == WM_QUIT)
        {
            gRunning = false;
        }

        gInput.lastClick = gInput.click;

        // Process keyboard
        switch (message.message)
        {
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYUP:
        case WM_KEYDOWN: {
            u32 vkCode = (u32)message.wParam;
            bool wasDown = ((message.lParam & (1 << 30)) != 0);
            bool isDown = ((message.lParam & (1 << 31)) == 0);
            bool altKeyWasDown = (message.lParam & (1ull << 29));

            if (isDown && (wasDown != isDown))
            {
                if (vkCode == VK_ESCAPE || (vkCode == VK_F4 && altKeyWasDown))
                {
                    gRunning = false;
                }
                else if (vkCode >= VK_F1 && vkCode <= VK_F12)
                {
                    gInput.functionKey = int(vkCode - VK_F1 + 1);
                }
                else if (vkCode >= '0' && vkCode <= '9')
                {
                    gInput.number = int(vkCode - '0');
                }
            }

            break;
        }

        case WM_LBUTTONDOWN:
            gInput.click = true;
            gInput.x = int(GET_X_LPARAM(message.lParam)) / kScale;
            gInput.y = int(GET_Y_LPARAM(message.lParam)) / kScale;
            break;

        case WM_LBUTTONUP:
            gInput.click = false;
            gInput.x = int(GET_X_LPARAM(message.lParam)) / kScale;
            gInput.y = int(GET_Y_LPARAM(message.lParam)) / kScale;
            break;

        case WM_MOUSEMOVE:
            gInput.x = int(GET_X_LPARAM(message.lParam)) / kScale;
            gInput.y = int(GET_Y_LPARAM(message.lParam)) / kScale;
            break;

        default:
            TranslateMessage(&message);
            DispatchMessage(&message);
            break;

        }
    }
}

//----------------------------------------------------------------------------------------------------------------------

int CALLBACK WinMain(HINSTANCE inst, HINSTANCE, LPSTR cmdLine, int cmdShow)
{
    initGame();

    WNDCLASSA wc = { };
    win32ResizeDIBSection(gGlobalBackBuffer, kWidth, kHeight);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = &win32MainWindowCallback;
    wc.hInstance = inst;
    wc.lpszClassName = "EpykFrame";
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.hIcon = LoadIcon(0, IDI_APPLICATION);

    if (RegisterClassA(&wc))
    {
        RECT rc = { 0, 0, kWidth * kScale, kHeight * kScale };
        DWORD styles = WS_CAPTION | WS_SYSMENU | WS_OVERLAPPED | WS_MINIMIZEBOX | WS_VISIBLE;
        AdjustWindowRect(&rc, styles, FALSE);

        int dx = -rc.left + 20;
        int dy = -rc.top + 20;
        rc.left += dx;
        rc.top += dy;
        rc.right += dx;
        rc.bottom += dy;

        HWND wnd = CreateWindowExA(
            0,
            wc.lpszClassName,
            kTitle,
            styles,
            rc.left,
            rc.top,
            rc.right - rc.left,
            rc.bottom - rc.top,
            0, 0,
            inst,
            0);
        if (wnd)
        {
            HDC dc = GetDC(wnd);

            gRunning = true;
            gInput.functionKey = 0;
            gInput.click = false;
            gInput.lastClick = false;

            TimePoint time;
            time = timeNow();

            while (gRunning)
            {
                win32ProcessPendingMessages();

                GameOffScreenBuffer buffer = {};
                buffer.memory = (u32 *)gGlobalBackBuffer.memory;
                buffer.width = gGlobalBackBuffer.width;
                buffer.height = gGlobalBackBuffer.height;

                GameInput input = {};
                input.functionKey = gInput.functionKey;
                input.number = gInput.number;
                input.x = gInput.x;
                input.y = gInput.y;
                input.click = gInput.click;
                input.lastClick = gInput.lastClick;

                TimePoint now = timeNow();
                TimePeriod dt = timePeriod(time, now);
                time = now;
                input.dt = timeToSecs(dt);

                gameUpdateAndRender(buffer, input);

                gInput.number = -1;
                gInput.functionKey = 0;

                Win32WindowDimension dimension = win32GetWindowDimension(wnd);
                win32DisplayBufferInWindow(gGlobalBackBuffer, dc, dimension.width, dimension.height);
            }
        }
        else
        {
            // Failed to create window
            MessageBoxA(0, "Unable to create game window!", "ERROR", MB_ICONERROR | MB_OK);
        }
    }
    else
    {
        // Failed to create window
        MessageBoxA(0, "Unable to create game window!", "ERROR", MB_ICONERROR | MB_OK);
    }

    doneGame();
    return 0;
}
