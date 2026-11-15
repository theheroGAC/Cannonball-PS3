#include <cstring>
#include "sdl2/input.hpp"
#include "frontend/config.hpp"
#include "../../ps3/ps3_platform.hpp"

Input input;

Input::Input(void)
{
    gamepad = true;
    rumble_supported = 1;
    analog = 1;
    key_press = -1;
    joy_button = -1;
    wheel = CENTRE;
    a_wheel = CENTRE;
    a_accel = 0;
    a_brake = 0;
    a_motor = 0;
    stick = nullptr;
    controller = nullptr;
    haptic = nullptr;
    asettings = nullptr;
    axis_last = -1;
    axis_counter = 0;
    axis_config = 0;
    memset(keys, 0, sizeof(keys));
    memset(keys_old, 0, sizeof(keys_old));
    memset(motor_limits, 0, sizeof(motor_limits));
}

Input::~Input(void)
{
    close_joy();
}

void Input::init(int pad_id, int* key_config, int* pad_config, const int analog, int* axis, bool* invert, int* asettings)
{
    (void)pad_id;
    (void)key_config;
    (void)pad_config;
    (void)axis;
    (void)invert;
    this->analog = analog;
    this->asettings = asettings;
    this->gamepad = true;
    this->rumble_supported = 1;
    ps3_input_init();
}

void Input::open_joy()
{
    gamepad = true;
    rumble_supported = 1;
}

void Input::close_joy()
{
    ps3_input_set_rumble(0.0f);
}

void Input::handle_key_up(SDL_Keysym* k) { (void)k; }
void Input::handle_key_down(SDL_Keysym* k) { (void)k; }
void Input::handle_joy_axis(SDL_JoyAxisEvent* e) { (void)e; }
void Input::handle_joy_down(SDL_JoyButtonEvent* e) { (void)e; }
void Input::handle_joy_up(SDL_JoyButtonEvent* e) { (void)e; }
void Input::handle_joy_hat(SDL_JoyHatEvent* e) { (void)e; }
void Input::handle_controller_axis(SDL_ControllerAxisEvent* e) { (void)e; }
void Input::handle_controller_down(SDL_ControllerButtonEvent* e) { (void)e; }
void Input::handle_controller_up(SDL_ControllerButtonEvent* e) { (void)e; }

void Input::frame_done()
{
    memcpy(keys_old, keys, sizeof(keys));

    ps3_input_poll();
    uint32_t btn = ps3_input_buttons();
    uint32_t pressed = ps3_input_pressed();
    int lx = 128, ly = 128, rx = 128, ry = 128, l2 = 0, r2 = 0;
    ps3_input_get_analog(&lx, &ly, &rx, &ry, &l2, &r2);
    (void)ly;
    (void)rx;
    (void)ry;

    if ((pressed & PS3_ACT_SELECT) || ((btn & PS3_ACT_L3) && (pressed & PS3_ACT_R3)) || ((btn & PS3_ACT_R3) && (pressed & PS3_ACT_L3))) {
        g_ps3_pause_requested = 1;
    }
    if ((pressed & PS3_ACT_R3) && !(btn & PS3_ACT_L3)) {
        char sp[512];
        if (ps3_video_save_screenshot(sp, sizeof(sp))) {
            ps3_video_toast("Screenshot Saved PNG!", 90);
        }
    }

    int zone = (asettings && asettings[0] > 0) ? asettings[0] : 75;
    int dead = (asettings && asettings[1] >= 0) ? asettings[1] : 8;
    if (dead < 8) dead = 8;

    int dx = lx - 128;
    int abs_dx = dx < 0 ? -dx : dx;
    if (abs_dx <= dead) {
        if (btn & PS3_ACT_LEFT) a_wheel = 0x20;
        else if (btn & PS3_ACT_RIGHT) a_wheel = 0xE0;
        else a_wheel = CENTRE;
    } else {
        int scaled = (dx * 100) / zone;
        int w = CENTRE + scaled;
        if (w < 0) w = 0;
        if (w > 0xFF) w = 0xFF;
        a_wheel = w;
    }

    bool accel_dig = false;
    bool brake_dig = false;
    bool gear1_dig = false;
    bool gear2_dig = false;

    if (g_ps3_cfg.control_preset == 0) {
        accel_dig = (btn & (PS3_ACT_CROSS | PS3_ACT_R2)) != 0;
        brake_dig = (btn & (PS3_ACT_SQUARE | PS3_ACT_L2)) != 0;
        gear1_dig = (btn & (PS3_ACT_TRIANGLE | PS3_ACT_R1)) != 0;
        gear2_dig = (btn & PS3_ACT_L1) != 0;
    } else if (g_ps3_cfg.control_preset == 1) {
        accel_dig = (btn & PS3_ACT_CROSS) != 0;
        brake_dig = (btn & PS3_ACT_SQUARE) != 0;
        gear1_dig = (btn & (PS3_ACT_R1 | PS3_ACT_TRIANGLE)) != 0;
        gear2_dig = (btn & PS3_ACT_L1) != 0;
    } else {
        accel_dig = (btn & (PS3_ACT_CROSS | PS3_ACT_R2)) != 0;
        brake_dig = (btn & (PS3_ACT_SQUARE | PS3_ACT_L2)) != 0;
        gear1_dig = (btn & (PS3_ACT_TRIANGLE | PS3_ACT_R1)) != 0;
        gear2_dig = (btn & PS3_ACT_L1) != 0;
    }

    if (btn & PS3_ACT_CROSS) a_accel = 0xFF;
    else if (r2 > 12) a_accel = r2;
    else a_accel = accel_dig ? 0xFF : 0;

    if (btn & PS3_ACT_SQUARE) a_brake = 0xFF;
    else if (l2 > 12) a_brake = l2;
    else a_brake = brake_dig ? 0xFF : 0;

    keys[LEFT]      = (btn & PS3_ACT_LEFT) != 0;
    keys[RIGHT]     = (btn & PS3_ACT_RIGHT) != 0;
    keys[UP]        = (btn & PS3_ACT_UP) != 0;
    keys[DOWN]      = (btn & PS3_ACT_DOWN) != 0;
    keys[ACCEL]     = accel_dig;
    keys[BRAKE]     = brake_dig;
    keys[GEAR1]     = gear1_dig;
    keys[GEAR2]     = gear2_dig;
    keys[START]     = (btn & PS3_ACT_START) != 0;
    keys[COIN]      = (btn & (PS3_ACT_L3 | PS3_ACT_START)) != 0;
    keys[VIEWPOINT] = (btn & PS3_ACT_CIRCLE) != 0;
    keys[PAUSE]     = false;
    keys[STEP]      = false;
    keys[TIMER]     = false;
    keys[MENU]      = ((btn & PS3_ACT_L2) && (btn & PS3_ACT_R2) && (btn & PS3_ACT_START));
}

bool Input::is_pressed(presses p)
{
    return keys[p];
}

bool Input::is_pressed_clear(presses p)
{
    bool pressed = keys[p];
    keys[p] = false;
    return pressed;
}

bool Input::has_pressed(presses p)
{
    return keys[p] && !keys_old[p];
}

void Input::reset_axis_config()
{
    axis_last = -1;
}

int Input::get_axis_config()
{
    return -1;
}

void Input::set_rumble(bool enable, float strength)
{
    ps3_input_set_rumble(enable ? strength : 0.0f);
}
