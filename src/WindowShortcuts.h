#pragma once
#include <common.h>
#include <windows.h>

namespace filespacer {

enum class GestureButton : unsigned { Left, Middle, Right };
enum GestureModifier : unsigned { GestureCtrl = 1, GestureAlt = 2, GestureShift = 4 };

struct WindowShortcut {
    bool enabled;
    unsigned modifiers;
    GestureButton button;
};
struct WindowShortcuts {
    WindowShortcut move;
    WindowShortcut resize;
};
constexpr WindowShortcuts DEFAULT_WINDOW_SHORTCUTS = {
    {true, GestureAlt, GestureButton::Left}, {true, GestureAlt, GestureButton::Right}
};
inline bool sameShortcut(const WindowShortcut &a, const WindowShortcut &b) {
    return a.enabled == b.enabled && a.modifiers == b.modifiers && a.button == b.button;
}
inline bool reservedShortcut(const WindowShortcut &binding) {
    return binding.button != GestureButton::Middle && !(binding.modifiers & GestureAlt);
}
inline bool validShortcut(const WindowShortcut &binding) {
    return binding.modifiers <= 7 && static_cast<unsigned>(binding.button) <= 2
        && (!binding.enabled || !reservedShortcut(binding));
}
inline bool conflictingShortcuts(const WindowShortcuts &bindings) {
    return bindings.move.enabled && bindings.resize.enabled
        && bindings.move.modifiers == bindings.resize.modifiers && bindings.move.button == bindings.resize.button;
}
inline bool validShortcuts(const WindowShortcuts &bindings) {
    return validShortcut(bindings.move) && validShortcut(bindings.resize) && !conflictingShortcuts(bindings);
}
inline bool matchesShortcut(const WindowShortcut &binding, GestureButton button, unsigned modifiers) {
    return binding.enabled && validShortcut(binding) && binding.button == button && binding.modifiers == modifiers;
}
inline unsigned currentGestureModifiers() {
    return (GetKeyState(VK_CONTROL) < 0 ? GestureCtrl : 0u)
        | (GetKeyState(VK_MENU) < 0 ? GestureAlt : 0u) | (GetKeyState(VK_SHIFT) < 0 ? GestureShift : 0u);
}
inline UINT gestureButtonMask(GestureButton button) {
    return button == GestureButton::Left ? MK_LBUTTON : button == GestureButton::Middle ? MK_MBUTTON : MK_RBUTTON;
}
inline UINT gestureButtonUp(GestureButton button) {
    return button == GestureButton::Left ? WM_LBUTTONUP : button == GestureButton::Middle ? WM_MBUTTONUP : WM_RBUTTONUP;
}

} // namespace filespacer
