#pragma once

#include <windows.h>

const UINT WM_SEEKBAR_SEEK = WM_APP + 10;

namespace seekbar {

HWND create(HWND parent, HINSTANCE instance);
void setRange(HWND bar, int length);
void setPosition(HWND bar, int position);

}
