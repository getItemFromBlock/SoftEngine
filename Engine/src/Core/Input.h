#pragma once
#include <unordered_map>

enum class KeyEvent
{
    None,
    Pressed,
    Down,
    Released,
    Up
};

inline const char* to_string(KeyEvent e)
{
    switch (e)
    {
    case KeyEvent::None: return "None";
    case KeyEvent::Pressed: return "Pressed";
    case KeyEvent::Down: return "Down";
    case KeyEvent::Released: return "Released";
    case KeyEvent::Up: return "Up";
    default: return "unknown";
    }
}

enum class Key : int
{
    NONE = 0,
    SPACE = 32,
    APOSTROPHE = 39,
    COMMA = 44,
    MINUS = 45,
    PERIOD = 46,
    SLASH = 47,
    KEY_0 = 48,
    KEY_1 = 49,
    KEY_2 = 50,
    KEY_3 = 51,
    KEY_4 = 52,
    KEY_5 = 53,
    KEY_6 = 54,
    KEY_7 = 55,
    KEY_8 = 56,
    KEY_9 = 57,
    SEMICOLON = 59,
    EQUAL = 61,
    A = 65,
    B = 66,
    C = 67,
    D = 68,
    E = 69,
    F = 70,
    G = 71,
    H = 72,
    I = 73,
    J = 74,
    K = 75,
    L = 76,
    M = 77,
    N = 78,
    O = 79,
    P = 80,
    Q = 81,
    R = 82,
    S = 83,
    T = 84,
    U = 85,
    V = 86,
    W = 87,
    X = 88,
    Y = 89,
    Z = 90,
    LEFT_BRACKET = 91,
    BACKSLASH = 92,
    RIGHT_BRACKET = 93,
    GRAVE_ACCENT = 96,
    WORLD_1 = 161,
    WORLD_2 = 162,
    ESCAPE = 256,
    ENTER = 257,
    TAB = 258,
    BACKSPACE = 259,
    INSERT = 260,
    KEY_DELETE = 261,
    RIGHT = 262,
    LEFT = 263,
    DOWN = 264,
    UP = 265,
    PAGE_UP = 266,
    PAGE_DOWN = 267,
    HOME = 268,
    END = 269,
    CAPS_LOCK = 280,
    SCROLL_LOCK = 281,
    NUM_LOCK = 282,
    PRINT_SCREEN = 283,
    PAUSE = 284,
    F1 = 290,
    F2 = 291,
    F3 = 292,
    F4 = 293,
    F5 = 294,
    F6 = 295,
    F7 = 296,
    F8 = 297,
    F9 = 298,
    F10 = 299,
    F11 = 300,
    F12 = 301,
    F13 = 302,
    F14 = 303,
    F15 = 304,
    F16 = 305,
    F17 = 306,
    F18 = 307,
    F19 = 308,
    F20 = 309,
    F21 = 310,
    F22 = 311,
    F23 = 312,
    F24 = 313,
    F25 = 314,
    KP_0 = 320,
    KP_1 = 321,
    KP_2 = 322,
    KP_3 = 323,
    KP_4 = 324,
    KP_5 = 325,
    KP_6 = 326,
    KP_7 = 327,
    KP_8 = 328,
    KP_9 = 329,
    KP_DECIMAL = 330,
    KP_DIVIDE = 331,
    KP_MULTIPLY = 332,
    KP_SUBTRACT = 333,
    KP_ADD = 334,
    KP_ENTER = 335,
    KP_EQUAL = 336,
    LEFT_SHIFT = 340,
    LEFT_CONTROL = 341,
    LEFT_ALT = 342,
    LEFT_SUPER = 343,
    RIGHT_SHIFT = 344,
    RIGHT_CONTROL = 345,
    RIGHT_ALT = 346,
    RIGHT_SUPER = 347,
    MENU = 348,
};

inline const char* to_string(Key e)
{
    switch (e)
    {
    case Key::NONE: return "NONE";
    case Key::SPACE: return "SPACE";
    case Key::APOSTROPHE: return "APOSTROPHE";
    case Key::COMMA: return "COMMA";
    case Key::MINUS: return "MINUS";
    case Key::PERIOD: return "PERIOD";
    case Key::SLASH: return "SLASH";
    case Key::KEY_0: return "KEY_0";
    case Key::KEY_1: return "KEY_1";
    case Key::KEY_2: return "KEY_2";
    case Key::KEY_3: return "KEY_3";
    case Key::KEY_4: return "KEY_4";
    case Key::KEY_5: return "KEY_5";
    case Key::KEY_6: return "KEY_6";
    case Key::KEY_7: return "KEY_7";
    case Key::KEY_8: return "KEY_8";
    case Key::KEY_9: return "KEY_9";
    case Key::SEMICOLON: return "SEMICOLON";
    case Key::EQUAL: return "EQUAL";
    case Key::A: return "A";
    case Key::B: return "B";
    case Key::C: return "C";
    case Key::D: return "D";
    case Key::E: return "E";
    case Key::F: return "F";
    case Key::G: return "G";
    case Key::H: return "H";
    case Key::I: return "I";
    case Key::J: return "J";
    case Key::K: return "K";
    case Key::L: return "L";
    case Key::M: return "M";
    case Key::N: return "N";
    case Key::O: return "O";
    case Key::P: return "P";
    case Key::Q: return "Q";
    case Key::R: return "R";
    case Key::S: return "S";
    case Key::T: return "T";
    case Key::U: return "U";
    case Key::V: return "V";
    case Key::W: return "W";
    case Key::X: return "X";
    case Key::Y: return "Y";
    case Key::Z: return "Z";
    case Key::LEFT_BRACKET: return "LEFT_BRACKET";
    case Key::BACKSLASH: return "BACKSLASH";
    case Key::RIGHT_BRACKET: return "RIGHT_BRACKET";
    case Key::GRAVE_ACCENT: return "GRAVE_ACCENT";
    case Key::WORLD_1: return "WORLD_1";
    case Key::WORLD_2: return "WORLD_2";
    case Key::ESCAPE: return "ESCAPE";
    case Key::ENTER: return "ENTER";
    case Key::TAB: return "TAB";
    case Key::BACKSPACE: return "BACKSPACE";
    case Key::INSERT: return "INSERT";
    case Key::KEY_DELETE: return "KEY_DELETE";
    case Key::RIGHT: return "RIGHT";
    case Key::LEFT: return "LEFT";
    case Key::DOWN: return "DOWN";
    case Key::UP: return "UP";
    case Key::PAGE_UP: return "PAGE_UP";
    case Key::PAGE_DOWN: return "PAGE_DOWN";
    case Key::HOME: return "HOME";
    case Key::END: return "END";
    case Key::CAPS_LOCK: return "CAPS_LOCK";
    case Key::SCROLL_LOCK: return "SCROLL_LOCK";
    case Key::NUM_LOCK: return "NUM_LOCK";
    case Key::PRINT_SCREEN: return "PRINT_SCREEN";
    case Key::PAUSE: return "PAUSE";
    case Key::F1: return "F1";
    case Key::F2: return "F2";
    case Key::F3: return "F3";
    case Key::F4: return "F4";
    case Key::F5: return "F5";
    case Key::F6: return "F6";
    case Key::F7: return "F7";
    case Key::F8: return "F8";
    case Key::F9: return "F9";
    case Key::F10: return "F10";
    case Key::F11: return "F11";
    case Key::F12: return "F12";
    case Key::F13: return "F13";
    case Key::F14: return "F14";
    case Key::F15: return "F15";
    case Key::F16: return "F16";
    case Key::F17: return "F17";
    case Key::F18: return "F18";
    case Key::F19: return "F19";
    case Key::F20: return "F20";
    case Key::F21: return "F21";
    case Key::F22: return "F22";
    case Key::F23: return "F23";
    case Key::F24: return "F24";
    case Key::F25: return "F25";
    case Key::KP_0: return "KP_0";
    case Key::KP_1: return "KP_1";
    case Key::KP_2: return "KP_2";
    case Key::KP_3: return "KP_3";
    case Key::KP_4: return "KP_4";
    case Key::KP_5: return "KP_5";
    case Key::KP_6: return "KP_6";
    case Key::KP_7: return "KP_7";
    case Key::KP_8: return "KP_8";
    case Key::KP_9: return "KP_9";
    case Key::KP_DECIMAL: return "KP_DECIMAL";
    case Key::KP_DIVIDE: return "KP_DIVIDE";
    case Key::KP_MULTIPLY: return "KP_MULTIPLY";
    case Key::KP_SUBTRACT: return "KP_SUBTRACT";
    case Key::KP_ADD: return "KP_ADD";
    case Key::KP_ENTER: return "KP_ENTER";
    case Key::KP_EQUAL: return "KP_EQUAL";
    case Key::LEFT_SHIFT: return "LEFT_SHIFT";
    case Key::LEFT_CONTROL: return "LEFT_CONTROL";
    case Key::LEFT_ALT: return "LEFT_ALT";
    case Key::LEFT_SUPER: return "LEFT_SUPER";
    case Key::RIGHT_SHIFT: return "RIGHT_SHIFT";
    case Key::RIGHT_CONTROL: return "RIGHT_CONTROL";
    case Key::RIGHT_ALT: return "RIGHT_ALT";
    case Key::RIGHT_SUPER: return "RIGHT_SUPER";
    case Key::MENU: return "MENU";
    default: return "unknown";
    }
}

enum class CursorType
{
    Arrow,
    IBeam,
    CrossHair,
    Hand,
    HResize,
    WResize
};

enum class CursorMode
{
    Normal,
    Hidden,
    Disabled
};

enum class MouseButton
{
    BUTTON_1 = 0,
    BUTTON_2 = 1,
    BUTTON_3 = 2,
    BUTTON_4 = 3,
    BUTTON_5 = 4,
    BUTTON_6 = 5,
    BUTTON_7 = 6,
    BUTTON_8 = 7,
    BUTTON_LAST = BUTTON_8,
    BUTTON_LEFT = BUTTON_1,
    BUTTON_RIGHT = BUTTON_2,
    BUTTON_MIDDLE = BUTTON_3,
};

inline const char* to_string(MouseButton e)
{
    switch (e)
    {
    case MouseButton::BUTTON_1: return "BUTTON_1";
    case MouseButton::BUTTON_2: return "BUTTON_2";
    case MouseButton::BUTTON_3: return "BUTTON_3";
    case MouseButton::BUTTON_4: return "BUTTON_4";
    case MouseButton::BUTTON_5: return "BUTTON_5";
    case MouseButton::BUTTON_6: return "BUTTON_6";
    case MouseButton::BUTTON_7: return "BUTTON_7";
    case MouseButton::BUTTON_8: return "BUTTON_8";
    default: return "unknown";
    }
}

class Input
{
public:
    void OnKeyCallback(Key key, KeyEvent event)
    {
        m_keys[key] = event;
    }
    
    void OnMouseButtonCallback(MouseButton button, KeyEvent event)
    {
        m_mouseButtons[button] = event;
    }
    
    void UpdateStates();
    
    KeyEvent GetKeyState(Key key) const;
    KeyEvent GetMouseButtonState(MouseButton button) const;

    bool IsKeyDown(Key key) const;
    bool IsKeyPressed(Key key) const;
    bool IsKeyReleased(Key key) const;
    bool IsKeyUp(Key key) const;

    bool IsMouseButtonDown(MouseButton button) const;
    bool IsMouseButtonPressed(MouseButton button) const;
    bool IsMouseButtonReleased(MouseButton button) const;
    bool IsMouseButtonUp(MouseButton button) const;

private:
    std::unordered_map<Key, KeyEvent> m_keys;
    std::unordered_map<MouseButton, KeyEvent> m_mouseButtons;
};
