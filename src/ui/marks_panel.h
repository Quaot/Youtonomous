#pragma once

#include <string>

#include <windows.h>

#include "controls.h"
#include "library.h"

class MarksPanel {
public:
    void create(const ui::Context& context);
    void layout(int width, int height);
    void setVisible(bool visible);

    void show(const Video* video);
    void showStart(int seconds);
    void showStoppedAt(int seconds);
    std::string startText() const;
    std::string takeLabel();
    int selected(size_t count) const;

    void showNote(const std::string& note);
    void beginEdit(const std::string& label, const std::string& note);
    void endEdit();
    std::string noteText() const;

    bool isStartBox(HWND window) const { return window == start_edit_; }
    bool isLabelBox(HWND window) const { return window == label_edit_; }
    bool isNoteBox(HWND window) const { return window == note_edit_; }

private:
    HWND start_label_ = nullptr;
    HWND start_edit_ = nullptr;
    HWND set_start_button_ = nullptr;
    HWND go_start_button_ = nullptr;
    HWND resume_button_ = nullptr;

    HWND marks_label_ = nullptr;
    HWND list_ = nullptr;
    HWND label_edit_ = nullptr;
    HWND note_edit_ = nullptr;
    HWND add_button_ = nullptr;
    HWND edit_button_ = nullptr;
    HWND delete_button_ = nullptr;
};
