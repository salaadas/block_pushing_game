#include "hud.h"

#include "time_info.h"
#include "opengl.h"
#include "draw.h"
#include "main.h"

RArr<Fader*> game_faders;
Dynamic_Font *fader_font = NULL;
Dynamic_Font *fps_font = NULL;

void clear_faders()
{
    // if (in_and_editing_mode())
    // {
    //     for (editor_faders) my_free(it);
    //     array_reset(&editor_faders);
    // }
    // else

    for (auto it : game_faders) my_free(it);
    array_reset(&game_faders);
}

Fader *game_report(String text)
{
    return do_report(text, &game_faders);
}

Fader *short_game_report(String text)
{
    auto fader = do_report(text, &game_faders);

    if (fader)
    {
        fader->fade_in_t  = 0.05f;
        fader->sustain_t  = 0.5f;
        fader->fade_out_t = 0.5f;
    }

    return fader;
}

inline
void init_fader(Fader *fader)
{
    fader->font = NULL;

    fader->time_started = 0.0;
    fader->pre_fade_t   = 0.0;
    fader->fade_in_t    = 2.0;
    fader->sustain_t    = 2.5;
    fader->fade_out_t   = 0.5;

    fader->y = 0.0;
}

Fader *do_report(String text, RArr<Fader*> *faders)
{
    // Offsets the older faders up:
    for (auto fader : *faders)
    {
        auto h = static_cast<f32>(render_target_height);
        if (h < 1) h = 1;

        auto dy = fader->font->default_line_spacing / h;
        fader->y += dy;
    }

    auto now = timez.ui_time;

    auto color = Vector4(1, 1, 0.8, 1);
    auto bg_color = Vector4(0, 0, 0, .5);

    auto fader = New<Fader>(false);
    init_fader(fader);

    fader->text         = copy_string(text); // @Fixme
    fader->y            = .75;
    fader->time_started = now;
    fader->font         = fader_font;
    fader->pre_fade_t   = 0.0;
    fader->fade_in_t    = 0.5;
    fader->color        = color;
    fader->bg_color     = bg_color;
    fader->sustain_t    = 2;

    array_add(faders, fader);

    return fader;
}

inline
bool draw_one_fader(Fader *fader)
{
    rendering_2d_right_handed_unit_scale();

    auto now = timez.ui_time;
    auto elapsed = now - fader->time_started;

    auto dx = fader->dx_dt.x * elapsed;
    auto dy = fader->dx_dt.y * elapsed;

    auto alpha = 0.0f;

    if (elapsed > fader->pre_fade_t)
    {
        elapsed -= fader->pre_fade_t;
        alpha = 1;

        if (elapsed > fader->fade_in_t)
        {
            elapsed -= fader->fade_in_t;

            if (elapsed >= fader->fade_out_t)
            {
                // This means we are done with the fader.
                return true;
            }
            else
            {
                if (fader->fade_out_t) alpha = 1 - elapsed / fader->fade_out_t;
                else                   alpha = 0;
            }
        }
    }

    if (alpha > 0)
    {
        alpha = std::clamp(alpha, 0.0f, 1.0f);

        auto color = fader->color;
        color.w = alpha;

        auto bg_color = fader->bg_color;
        bg_color.w = alpha * alpha;

        auto b = static_cast<i32>(fader->font->character_height / 20.0f);
        if (b < 1) b = 1;
        auto ox = -b;
        auto oy = -b;

        auto width = prepare_text(fader->font, fader->text);

        auto yy = render_target_height * fader->y + dy * render_target_width; // Width is intentional, it's so dx and dy are in the same units.

        auto font = fader->font;

        draw_text(font, static_cast<i32>((render_target_width - width)/2.0f + ox), static_cast<i32>(yy + oy), fader->text, bg_color);
        draw_text(font, static_cast<i32>((render_target_width - width)/2.0f), static_cast<i32>(yy), fader->text, color);
    }

    return false;
}

void draw_faders(RArr<Fader*> *faders)
{
    for (i64 it_index = 0; it_index < faders->count; )
    {
        auto fader = (*faders)[it_index];

        auto done = draw_one_fader(fader);

        if (done)
        {
            // Swap with the last element, and not incrementing the iterating index

            free_string(&fader->text);
            my_free(fader);
            (*faders)[it_index] = (*faders)[faders->count - 1];

            faders->count -= 1;

            continue;
        }

        it_index += 1;
    }
}

void draw_fps()
{
    auto text = tprint(String("%5.2f fps"), //  %f|%f|%f ms     need: last_min_dt, last_average_dt, last_max_dt
                       1.0 / timez.ui_dt);

    auto ox = 2;
    auto oy = -ox;

    auto color = Vector4(1, 1, 1, 1);
    auto bg_color = Vector4(0, 0, 0, 1);

    auto width = prepare_text(fps_font, text);
    auto extra_padding = 10;
    auto height = fps_font->character_height;

    rendering_2d_right_handed_unit_scale();

    immediate_begin();

    draw_text(fps_font, render_target_width - width - extra_padding + ox, render_target_height - height - extra_padding + oy, text, bg_color);
    draw_text(fps_font, render_target_width - width - extra_padding, render_target_height - height - extra_padding, text, color);

    immediate_flush();
}

void draw_hud(Entity_Manager *manager)
{
    rendering_2d_right_handed_unit_scale();

    // draw_checkpoint_flash();

    draw_faders(&game_faders);

#ifdef DEVELOPER_MODE
    draw_fps();
#endif
}

void hud_do_fader(String text, bool force_reset, f32 fade_in_time = -1.0f)
{
    if (force_reset)
    {
        clear_faders();
    }

    auto fader = do_report(text, &game_faders);

    if ((fade_in_time >= 0) && fader)
    {
        fader->fade_in_t = fade_in_time;
    }
}
