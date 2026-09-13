#include <windows.h>

#include "window.h"

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show)
{
    MainWindow window;
    if (!window.create(instance, show))
        return 1;

    int argc = 0;
    wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc > 1)
        window.openPath(argv[1]);
    LocalFree(argv);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (window.handleKey(msg))
            continue;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
