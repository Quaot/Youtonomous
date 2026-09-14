#pragma once

#include <string>

#include <windows.h>

class Player {
public:
    virtual ~Player() = default;

    virtual bool ready() const = 0;
    virtual bool loaded() const = 0;

    virtual void attach(HWND window) = 0;
    virtual void open(const std::string& path, int start) = 0;
    virtual void togglePause() = 0;

    virtual bool playing() const = 0;
    virtual int time() const = 0;
    virtual int length() const = 0;

    virtual void seek(int seconds) = 0;
    void skip(int seconds) { seek(time() + seconds); }
};
