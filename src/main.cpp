#include <windows.h>

#include "window.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show)
{
    MainWindow window;
    if (!window.create(instance, show))
        return 1;

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
