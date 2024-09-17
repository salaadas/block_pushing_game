#include "player_control.h"

#include "main.h"
#include "events.h"
#include "sokoban.h"
#include "time_info.h"
#include "editor.h"
#include "undo.h"

const f32 TIME_PER_MOVE_REPEAT = 0.22;

Key_Info key_left;
Key_Info key_right;
Key_Info key_up;
Key_Info key_down;
Key_Info key_undo;
Key_Info key_action;

bool should_ignore_input = false;
i32 undo_count = 0;
i32 restart_count = 0;

void toggle_editor()
{
    if (program_mode == Program_Mode::GAME)
    {
        program_mode = Program_Mode::EDITOR;
    }
    else
    {
        // maybe_push_editor_data_to_game();
        program_mode = Program_Mode::GAME;
    }
}

void press(Key_Info *info, bool pressed, bool is_repeat = false)
{
    auto old_pressed = info->pressed;

    info->pressed = pressed;
    info->repeat = is_repeat;

    if (!pressed) return;

    if (!old_pressed)
    {
        info->time_accumulated = 0;
        info->signalled = true;
    }
}

void update_guy_active_flags(Entity_Manager *manager)
{
    auto hero = get_active_hero(manager);
    auto hero_id = hero->base->entity_id;

    auto active_index = manager->active_hero_index;

    for (auto it : manager->_by_guys)
    {
        // Also enable the active flag if it is a clone of the active hero
        /*
        if ((it == hero) || (it->clone_of_id == hero_id))
        {
            it->active = true;
        }
        else
        {
            it->active = false;
        }
        */
        it->active = (it == hero) || (it->clone_of_id == hero_id);

        // This is for when we do animation!
        // if (!it->dead)
        // {
        //     change_active_state(it, it->active);
        // }
    }

    // @Later
    // In certain cases when undoing until there are no guys left
    // on a level, we might have an active_hero_index that no longer
    // makes sense.
}

void switch_heroes()
{
    auto manager = get_entity_manager();
    auto turn_order = manager->turn_order;

    if (!turn_order) return;

    auto count = turn_order.count;
    manager->active_hero_index = (manager->active_hero_index + 1) % count;

    update_guy_active_flags(manager);
}

bool handle_global_event(Event *event)
{
    auto key = event->key_code;
    auto pressed = static_cast<bool>(event->key_pressed);

#ifdef DEVELOPER_MODE
    if (pressed && (key == Key_Code::CODE_F9))
    {
        toggle_editor();
        return true;
    }
    // else
    // {
    //     if (!in_an_editing_mode())
    //     {
    //         handle_event_for_developer_keymap(event);
    //     }
    // }
#endif

    return false;
}

void handle_event_for_game(Event *event)
{
    if (event->type == EVENT_KEYBOARD)
    {
        auto key     = event->key_code;
        auto pressed = event->key_pressed;
        auto repeat  = event->repeat;

        switch (key)
        {
            case Key_Code::CODE_ARROW_LEFT:  press(&key_left,  pressed,  repeat); break;
            case Key_Code::CODE_ARROW_RIGHT: press(&key_right, pressed,  repeat); break;
            case Key_Code::CODE_ARROW_DOWN:  press(&key_down,  pressed,  repeat); break;
            case Key_Code::CODE_ARROW_UP:    press(&key_up,    pressed,  repeat); break;

            // case Key_Code::CODE_Y: // :DeprecateMe @Temporary :SaveGame
            // {
            //     if (!pressed) break;

            //     auto ctrl_held  = ui_button_is_down(Key_Code::CODE_CTRL);

            //     if (!ctrl_held) break;

            //     auto manager = get_entity_manager();

            //     auto shift_held  = ui_button_is_down(Key_Code::CODE_SHIFT);
            //     if (shift_held)
            //     {
            //         load_game(manager);
            //         break;
            //     }

            //     save_game(manager);
            // } break;

            case Key_Code::CODE_ENTER:
            {
                // Doing the same action as 'x' key

                if (!pressed) break;

                auto manager = get_entity_manager();
                switch_heroes();
                // use_magic(manager);

                undo_end_frame(manager->undo_handler);
            } break;
        }
    }

    if (event->type == EVENT_TEXT_INPUT)
    {
        switch (event->utf32)
        {
            case 'p': noclip = !noclip; break;
            case 'z': undo_count    += 1; break;
            case 'r': restart_count += 1; break;
            case 'x': switch_heroes(); break;
        }
    }
}

void restart_current_level()
{
    init_level_general(true);
}

// #include "ui.h"  // :DeprecateMe see the below @Temporary comment. :IncompleteUI

void read_input()
{
    undo_count = 0;
    restart_count = 0;

    for (auto event : events_this_frame)
    {
        if (should_ignore_input) break;

        auto handled = handle_global_event(&event);
        if (handled) continue;

        // ui_handle_event(event); // This will be called within editor.cpp @Temporary. :IncompleteUI

#ifdef DEVELOPER_MODE
        if (program_mode == Program_Mode::EDITOR)
        {
            editor_handle_event(&event);
            continue;
        }
#endif

        handle_event_for_game(&event); // @Fixme: Not handling any input for game since we are testing the UI. :IncompleteUI
    }

    auto manager = get_entity_manager();

    for (i32 i = 0; i < undo_count; ++i)
    {
        do_one_undo(manager);
    }

    if (undo_count)
    {
        post_undo_reevaluate(manager);
    }

    if (restart_count)
    {
        restart_current_level();
    }

    per_frame_update_mouse_position();
}

// These are axis of the world:
Vector3 axis_right   = Vector3(1, 0, 0);
Vector3 axis_forward = Vector3(0, 1, 0);
Vector3 axis_up      = Vector3(0, 0, 1);

// @Temporary: For now, our editor only consists of the camera
// so we don't bother making an editor_control struct.
// But consider doing that when the editor is more feature-full
f32 editor_default_speed = 2.0f;
f32 editor_speed_shift   = 5.0f;
f32 editor_speed_ctrl    = 10.0f;

void do_camera_orientation_input(f32 *camera_theta_pointer, f32 *camera_phi_pointer)
{
    auto theta = *camera_theta_pointer;
    auto phi   = *camera_phi_pointer;

    auto dt = timez.ui_dt;

    auto MOUSE_SENS = 0.05 * 1;
    theta += -mouse_delta_x * MOUSE_SENS * dt;
    phi   +=  mouse_delta_y * MOUSE_SENS * dt;

    auto PHI_LIMIT = static_cast<f32>(TAU * 0.25 * 0.95);
    Clamp(&phi, -PHI_LIMIT, PHI_LIMIT);

    *camera_theta_pointer = theta;
    *camera_phi_pointer   = phi;
}

Quaternion get_orientation_from_angles(f32 theta, f32 phi, f32 rho)
{
    Quaternion rotation_theta = Quaternion(1, 0, 0, 0);
    Quaternion rotation_phi   = Quaternion(1, 0, 0, 0);
    Quaternion rotation_rho   = Quaternion(1, 0, 0, 0);

    get_ori_from_rot(&rotation_theta, Vector3(0, 0, 1), theta - M_PI / 2.0f);

    auto new_y = axis_right;
    new_y = rotate(new_y, rotation_theta);

    get_ori_from_rot(&rotation_phi, new_y, phi);
    auto new_forward = rotate(rotate(axis_forward, rotation_theta), rotation_phi);
    
    Quaternion result;

    if (rho)
    {
        get_ori_from_rot(&rotation_rho, new_forward, rho);
        result = rotation_rho * rotation_phi * rotation_theta;
    }
    else
    {
        result = rotation_phi * rotation_theta;
    }

    return result;
}

void do_camera_position_input(Vector3 *camera_pos, Quaternion ori)
{
    Vector3 dir  = Vector3(0, 0, 0);
    Vector3 zdir = Vector3(0, 0, 0);

    // if (!is_typing())
    {
        if (ui_button_is_down(Key_Code::CODE_W)) dir += axis_forward;
        if (ui_button_is_down(Key_Code::CODE_S)) dir -= axis_forward;
        if (ui_button_is_down(Key_Code::CODE_A)) dir -= axis_right;
        if (ui_button_is_down(Key_Code::CODE_D)) dir += axis_right;

        // We don't want to use the camera orientation when moving up/down
        if (ui_button_is_down(Key_Code::CODE_N)) zdir += axis_up;
        if (ui_button_is_down(Key_Code::CODE_M)) zdir -= axis_up;
    }

    normalize_or_zero(&dir);
    normalize_or_zero(&zdir);

    auto dt = timez.ui_dt;
    auto world_dir = rotate(dir, ori);

    // Add the up/down direction
    world_dir += zdir;

    auto shift_held = ui_button_is_down(Key_Code::CODE_SHIFT);
    auto ctrl_held  = ui_button_is_down(Key_Code::CODE_CTRL);

    auto speed_desired = editor_default_speed;

    if (shift_held) speed_desired = editor_speed_shift;
    if (ctrl_held)  speed_desired = editor_speed_ctrl;

    auto dx = world_dir * speed_desired * dt;
    *camera_pos += dx;
}

void do_camera_zoom_input(f32 *camera_fov_vertical)
{
    auto fov = *camera_fov_vertical;

    auto y_offset = mouse_wheel_delta.vertical;

    fov += y_offset;

    Clamp(&fov, 1.0f, 90.0f);

    *camera_fov_vertical = fov;
}

void update_game_camera(Entity_Manager *manager)
{
    auto camera = &manager->camera;

    if (program_mode == Program_Mode::EDITOR)
    {
        // Do camera rotation
        f32 *theta  = &camera->theta;
        f32 *phi    = &camera->phi;

        do_camera_orientation_input(theta, phi);

        camera->forward.x = cosf(*theta) * cosf(*phi);
        camera->forward.y = sinf(*theta) * cosf(*phi);
        camera->forward.z = sinf(*phi);

        camera->orientation = get_orientation_from_angles(*theta, *phi);

        // Do camera movement
        do_camera_position_input(&camera->position, camera->orientation);

        // Do camera zooming
        do_camera_zoom_input(&camera->fov_vertical);
    }

    refresh_camera_matrices(camera);
}
