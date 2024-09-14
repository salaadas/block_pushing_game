#pragma once

#include "common.h"

#include "source_location.h"
#include "font.h"

extern Dynamic_Font *default_editor_font;
extern f32 mouse_x_float, mouse_y_float;
// extern i32 mouse_z; // Not used.

enum class Text_Alignment : u8
{
    Center = 0,
    Left   = 1,
    Right  = 2
};

enum class Contents_Mode
{
    Widgets = 0,
    Layouts,
    Inputs,

    Widget_Place_Labels,
    Widget_Place_Push_Buttons,
    Widget_Place_Check_Buttons,
    Widget_Place_Text_Inputs,
};

namespace Status_Flags // @Hack to get the bitwise operator to work nicely with enum.
{
    enum Status_Flags
    {
        OVER     = 0x01,
        FOCUS    = 0x02,
        DOWN     = 0x04,
        PRESSED  = 0x08,
        // Released = 0x10,
    };
}

namespace Margin // @Hack to get the bitwise operator to work nicely with enum.
{
    enum Margin : u8
    {
        HORIZONTAL = 0x1,
        VERTICAL   = 0x2
    };
}

struct Rect
{
    f32 x = 0, y = 0;
    f32 w = 0, h = 0;
};

#include "active_widgets.h"
#include "text_input.h"

//
// The states:
//
#include "ui_states.h"

//
// The themes:
//

//
// Size_Relativeness controls the way sizes of various widget aspects are computed. Wherever there
// is a declaration of type Size_Relativeness, there is also a similarly-named floating point
// parameter that acts as an argument. Rectangle_Shape, for example, has two fields 'relativeness',
// and 'roundedness'.
// ^ Setting 'relativeness' to RELATIVE_TO_HEIGHT causes the system to multiply 'roundedness' by
// the rect height to get the radius, in pixels, of the circle used to round the corner of the widget.
//
enum Size_Relativeness : u8
{
    RELATIVE_TO_HEIGHT = 0, // Computed by multiplying the relavant field by the height of the Rect passed in.
    RELATIVE_TO_WIDTH, // Computed by multiplying the relavant field by the width of the Rect passed in.
    ABSOLUTE_FROM_THEME_FIELD, // What you set in the theme field is the exact amount of pixels that you will get for the radius.
    ABSOLUTE_FROM_GLOBAL, // If you want all your themes to have matched sizes for some parameter, regardless of their rect sizes, but you don't want to manually adjust them all whenever the display resolution changes, you can use this type. This is @Incomplete. But there will be a set_global_rounding_radius() function, but if you don't call it, the sizes will automatically recomputed against the arbitrary value based on the render target height.
};

enum Rectangle_Rounding_Flags : u32
{
    NORTHWEST = 0x1,
    NORTHEAST = 0x2,
    SOUTHWEST = 0x4,
    SOUTHEAST = 0x8,

    NORTH = NORTHWEST | NORTHEAST,
    SOUTH = SOUTHWEST | SOUTHEAST,
};

struct Rectangle_Shape
{
    Size_Relativeness relativeness = Size_Relativeness::RELATIVE_TO_HEIGHT;
    f32 roundedness = 0.25f;
    u32 rounding_flags = (Rectangle_Rounding_Flags::NORTHWEST | Rectangle_Rounding_Flags::NORTHEAST | Rectangle_Rounding_Flags::SOUTHWEST | Rectangle_Rounding_Flags::SOUTHEAST);

    f32 frame_thickness = .03f; // Thickness of the frame relative to what ever is set in the 'relativeness';
    f32 pixels_per_edge_segment = 8.0f; // How many triangles we generate for the curves, based on the length of the arc.
};

struct Label_Theme
{
    Dynamic_Font *font = default_editor_font;
    Vector4 text_color = Vector4(1, 1, 1, 1);

    Text_Alignment alignment = Text_Alignment::Center;
    f32 alignment_pad_ems    = 1.0f;

    f32 text_baseline_vertical_position = FLT_MAX; // How high up the rectangular area of the text, relative to rect height. FLT_MAX means center the text vertically according to the rect.
};

struct Button_Theme
{
    Label_Theme label_theme;

    Rectangle_Shape rectangle_shape;

    Vector4 surface_color       = Vector4(.08, .08, .08, 1.0);
    Vector4 surface_color_over  = Vector4(.15, .15, .20, 1.0);
    Vector4 surface_color_flash = Vector4(.60, .77, .62, 1.0);
    Vector4 surface_color_down  = Vector4(.28, .28, .38, 1.0); // When held down.

    // 'text_color' is inside label_theme.
    Vector4 text_color_over    = Vector4(.95, .95, .95, 1.0);
    Vector4 text_color_pressed = Vector4(.99, .99, .99, 1.0);

    Vector2 text_offset_when_down = Vector2(.12, -.12); // In ems.

    f32 frame_thickness = 0.06f; // In terms of fraction of button's height.
    f32 press_duration  = 0.7f; // The duration of the flash when you press the button.

    f32 over_fade_in_speed  = 14.0f; // Higher is faster; 14 == 1/14 second for full fade in.
    f32 over_fade_out_speed = 6.0f; // e.g. 4 == 1/4 second for full fade out.

    f32 down_fade_in_speed  = 12.0f;
    f32 down_fade_out_speed = 6.0f;

    f32 alpha_scale = 1.0f; // @Incomplete: Not used right now.

    f32 highlight_lighten_parameter = .20f; // For lighter themes, you migth want more lighten and less darken.
    f32 highlight_darken_parameter  = .60f;
};

struct Checkbox_Theme
{
    Button_Theme button_theme;

    bool is_radio_button = false;

    // There aren't separate 'over' and 'pressed' colors when selected,
    // though maybe we should add those?
    Vector4 button_color_selected = Vector4(.90, .80, .90, 1.0);
    Vector4 text_color_selected   = Vector4(.90, .60, .90, 1.0);

    // @Note: I think this is a valid use case for constructors
    // since we can't easily default initialize fields of button_theme.
    Checkbox_Theme()
    {
        button_theme.label_theme.alignment = Text_Alignment::Left;
        button_theme.label_theme.alignment_pad_ems = 0.12;

        // Button theme's background colors are used for the bitmap.
        button_theme.surface_color       = Vector4(.55, .55, .55, 1);
        button_theme.surface_color_over  = Vector4(.80, .80, .80, 1);
        button_theme.surface_color_flash = Vector4(.90, .70, .90, 1);
        button_theme.surface_color_down  = Vector4(.90, .70, .90, 1); // When held down.

        button_theme.label_theme.text_color = Vector4(.55, .55, .55, 1.0);
        button_theme.text_color_over        = Vector4(.80, .80, .80, 1.0);
        button_theme.text_color_pressed     = Vector4(.90, .70, .90, 1.0);
    }
};

struct Text_Input_Theme
{
    // @Cleanup: Use Label_Theme here so that we can replace the font, text_color, alignment* stuff with those from the label theme.
    Dynamic_Font *font = default_editor_font;

    // These factors (being similar to those in Button_Theme), are in here because we
    // are not including a Button_Theme inside us since we don't need all the fields
    // there. So we make another set of them on our own.
    f32 press_duration        = 0.35f; // The duration of the flash when you press the input.
    f32 over_fade_in_speed    = 14.0f; // Higher is faster; 14 == 1/14 second for full fade in.
    f32 over_fade_out_speed   = 6.0f;  // e.g. 4 == 1/4 second for full fade out.
    f32 active_fade_in_speed  = 12.0f;
    f32 active_fade_out_speed = 6.0f;

    Vector4 text_color        = Vector4(.30, .70, .70, 1.0);
    Vector4 text_color_over   = Vector4(.99, .70, .70, 1.0);
    Vector4 text_color_active = Vector4(.55, .85, .85, 1.0);

    Vector4 text_color_auto_complete        = Vector4(.20, .79, .90, .40);
    Vector4 text_color_auto_complete_failed = Vector4(.99, .23, .10, .40);

    Vector4 background_color        = Vector4(.09, .07, .25, 1.0);
    Vector4 background_color_over   = Vector4(.12, .12, .40, 1.0);
    Vector4 background_color_active = Vector4(.40, .15, .65, 1.0);
    Vector4 background_color_flash  = Vector4(.60, .77, .62, 1.0);

    Vector4 selection_color = Vector4(.15, .09, .75, 1.0);
    Vector4 cursor_color    = Vector4(.99, .80, .73, 1.0);

    f32 text_baseline_vertical_position = 0.225f; // How high up the rectangular area of the text, relative to rect height.

    Text_Alignment alignment = Text_Alignment::Left;
    f32 alignment_pad_ems    = 1.0f; // How much space on the left of the rect before text begins (or right if alignment == ::Right, etc.), in ems.

    f32 text_insert_margin = 3.7f; // How much you want to be able to see when typing near the edge of a text input, in ems. The resulting pixel value is clamped at 49% of the width of the text input region at render at (otherwise it will be undefined when you want the insert point to be, unless we had two different margin).

    f32 cursor_width_inside_text  = 0.1f; // In ems.
    f32 cursor_width_outside_text = 0.6f; // In ems.

    Rectangle_Shape background_shape;
};

struct Slider_Theme
{
    Button_Theme button_theme;

    Vector4 background_color         = Vector4(.08, .15, .11, 1);
    Vector4 background_color_over    = Vector4(.13, .17, .13, 1);
    Vector4 background_color_pressed = Vector4(.15, .20, .20, 1);
    Vector4 background_color_sliding = Vector4(.17, .26, .23, 1);

    Vector4 button_color_sliding = Vector4(.20, .70, .54, 1);
    Vector4 text_color_sliding   = Vector4(.85, .92, .98, 1);

    f32 surface_margin = 0.09f; // As a fraction of the rect height.

    i32 decimals = 3; // If sliding a float, how many digits of precision to display after the decimal.

    bool text_editable = true; // Right-click or left-double-click to edit, if this is set. (@Incomplete: not handling double-click right now)

    bool clamp_text_input_low  = true;
    bool clamp_text_input_high = true;

    Text_Input_Theme text_input_theme; // If you decide to edit the slider's value. Note that this has an entirely different alignment and alignment_pad_ems, etc. so you may edit those or keep them in sync with the slider.

    bool use_spinboxes = true;
    f32 spinbox_width = 1.0f; // As a fraction of the slider rect's height. 1.0 means each spinbox button will be squared, and 2*height will be cut from the main rect to make space for the spinbox.
    Button_Theme spinbox_theme;

    Slider_Theme()
    {
        button_theme.surface_color      = Vector4(.13, .31, .21, 1);
        button_theme.surface_color_over = Vector4(.18, .48, .34, 1);

        button_theme.label_theme.alignment_pad_ems = .4f;

        // @Cleanup probably want a different variable since text_color usually mean something else.
        spinbox_theme.label_theme.text_color = Vector4(.40, .73, .75, 1.0);
        spinbox_theme.text_color_over        = Vector4(.50, .88, .91, 1.0);
        spinbox_theme.text_color_pressed     = Vector4(.70, .95, .99, 1.0);
    }
};

struct Scrollable_Region_Theme
{
    f32 horizontal_margin_size = .009f; // Relative to render target height.
    f32 vertical_margin_size   = .009f; // Relative to render target height. It's intentional that this is width and not height, because by default you want the sizes of the two margins to be controllable relative to each other, not messed up by random aspect ratio changes.

    f32 scrollbar_size = .03f; // Relative to render target height. This indicates the width for a vertical scrollbar, or height for a horizontal scrollbar.
    f32 minimum_nib_height = 1.0f; // Relative to scrollbar size. 1.0 means it will not get shorter than the width of the scrollbar.

    u32 margin_flags = (Margin::HORIZONTAL | Margin::VERTICAL);
    f32 nib_margin = .1f; // Mqargin between trench and nib, relative to scrollbar size.

    Button_Theme scrollbar_nib_theme;

    Vector4 scrollbar_trench_color = Vector4(.30, .05, .40, 1);
    Vector4 background_color = Vector4(.06, .01, .08, 1);

    f32 mouse_wheel_increment = .062f; // Distance to scrol per tick of the mouse hwell, relative to render target height.

    Rectangle_Shape scrollbar_shape;
    Rectangle_Shape content_area_shape;

    Scrollable_Region_Theme()
    {
        scrollbar_nib_theme.surface_color                    = Vector4(.55, .05, .55, 1);
        scrollbar_nib_theme.surface_color_over               = Vector4(.70, .05, .70, 1);
        scrollbar_nib_theme.surface_color_flash = Vector4(.90, .50, .90, 1);
        scrollbar_nib_theme.surface_color_down               = Vector4(.85, .15, .85, 1);
        scrollbar_nib_theme.rectangle_shape.frame_thickness  = .025f;

        scrollbar_shape.rounding_flags  = Rectangle_Rounding_Flags::NORTHEAST | Rectangle_Rounding_Flags::SOUTHEAST;
        scrollbar_shape.relativeness    = Size_Relativeness::RELATIVE_TO_WIDTH;
        scrollbar_shape.frame_thickness = 0;

        content_area_shape.rounding_flags  = Rectangle_Rounding_Flags::NORTHWEST | Rectangle_Rounding_Flags::SOUTHWEST;
        content_area_shape.relativeness    = Size_Relativeness::RELATIVE_TO_WIDTH;
        content_area_shape.roundedness     = .05f;
        content_area_shape.frame_thickness = .008f;
    }
};

struct Dropdown_Theme
{
    Button_Theme theme_for_current_value;
    Button_Theme theme_for_other_choices;
    Button_Theme theme_for_current_choice; // Used when drawing all the choices, this specify the theme for the currently picked choice.

    f32 dropdown_indicator_aspect_ratio = 1.0f; // This is for the little arrow on the right that moves up and down when you collapse or expand the dropdown. This relative to the rect height.

    f32 arrow_flip_up_speed   = 3.9f;
    f32 arrow_flip_down_speed = 2.8f;

    Dropdown_Theme()
    {
        theme_for_current_value.surface_color       = Vector4(.20, .10, .05, 1);
        theme_for_current_value.surface_color_over  = Vector4(.30, .20, .15, 1);
        theme_for_current_value.surface_color_flash = Vector4(.75, .55, .55, 1);
        theme_for_current_value.surface_color_down  = Vector4(.40, .20, .25, 1);

        theme_for_current_choice.surface_color       = Vector4(.80, .20, .05, 1);
        theme_for_current_choice.surface_color_over  = Vector4(.90, .30, .15, 1);
        theme_for_current_choice.surface_color_flash = Vector4(.95, .75, .55, 1);
        theme_for_current_choice.surface_color_down  = Vector4(.90, .40, .25, 1);
    }
};

struct Color_Picker_Theme
{
    Button_Theme apply_and_revert_button_theme;
    Button_Theme stash_button_theme;
    
    f32 horizontal_padding = .06f; // Relative to the rect width.
    f32 vertical_padding   = .06f; // Relative to the rect width.

    f32 stashes_color_width = .10f; // Relative to the rect width.
    f32 margin_between_color_input_and_stashed_colors = .06f;  // Relative to the rect width.

    f32 outer_circle_diameter = .99f; // Relative to the available circle area's width (which is computed by cutting off the margins).
    f32 inner_circle_diameter = .73f; // Same

    f32 inner_disc_radius = .95f; // Relative to inner_circle_diameter. 1.0 means it will meet the inner circle (but the rendering is not set up to do this seamlessly so you would get cracks, etc.)

    Rectangle_Shape background_shape;
    Vector4 background_color = Vector4(.03, .12, .12, 1);

    Label_Theme text_theme;

    f32 slider_hue_min = -30; // In degrees.
    f32 slider_hue_max = 345; // In degrees.

    String revert_string = String("Revert");
    String apply_string  = String("Apply");

    f32 hue_range_in_zoomed_slider = 60.0f;

    Rectangle_Shape stashed_colors_shape;

    RArr<Vector3> initial_stashed_colors;

    Color_Picker_Theme()
    {
        stashed_colors_shape.relativeness            = Size_Relativeness::RELATIVE_TO_WIDTH;
        stashed_colors_shape.pixels_per_edge_segment = 2.0f;
        stashed_colors_shape.frame_thickness         = 0.03f;

        background_shape.relativeness    = Size_Relativeness::RELATIVE_TO_WIDTH;
        background_shape.roundedness     = .08f;
        background_shape.frame_thickness = .02f;
    }
};

struct Slidable_Region_Theme
{
    enum Orientation : u8
    {
        HORIZONTAL = 0,
        VERTICAL   = 1
    };

    Orientation orientation = Orientation::HORIZONTAL;

    Button_Theme divider_theme;
    Size_Relativeness divider_thickness_type = Size_Relativeness::ABSOLUTE_FROM_GLOBAL;
    f32 divider_thickness = .20f;

    Rectangle_Shape background_shape;
    Vector4 background_color = Vector4(.01f, .06f, .08f, 1);

    Size_Relativeness initial_slider_position_type = Size_Relativeness::RELATIVE_TO_HEIGHT;
    f32 initial_slider_position = .5f;

    Slidable_Region_Theme()
    {
        background_shape.relativeness = Size_Relativeness::ABSOLUTE_FROM_GLOBAL;

        divider_theme.rectangle_shape.relativeness    = Size_Relativeness::ABSOLUTE_FROM_GLOBAL;
        divider_theme.rectangle_shape.frame_thickness = .02f;
        divider_theme.rectangle_shape.roundedness     = 0;
    }
};

struct Subwindow_Theme
{
    f32          title_bar_height = .03; // Relative to render target height.
    Button_Theme title_bar_theme;

    Rectangle_Shape content_area_shape; // Shape for the content area (area that excludes the title bar).
    Vector4         content_area_background_color = Vector4(.08, .08, .15, 1.);

    Subwindow_Theme()
    {
        title_bar_theme.rectangle_shape.rounding_flags = Rectangle_Rounding_Flags::NORTH;

        content_area_shape.relativeness = Size_Relativeness::ABSOLUTE_FROM_GLOBAL;
        content_area_shape.roundedness = 0.15f;
        content_area_shape.rounding_flags = Rectangle_Rounding_Flags::SOUTH;
    }
};

struct Overall_Theme
{
    Label_Theme             label_theme;
    Button_Theme            button_theme;
    Slider_Theme            slider_theme;
    Checkbox_Theme          checkbox_theme;
    Dropdown_Theme          dropdown_theme;
    Text_Input_Theme        text_input_theme;
    Color_Picker_Theme      color_picker_theme;
    Slidable_Region_Theme   slidable_region_theme;
    Scrollable_Region_Theme scrollable_region_theme;
    Subwindow_Theme         subwindow_theme;

    f32 alpha_scale = 1.0f; // @Incomplete: Not used....
};

f32 em(Dynamic_Font *font, f32 x);
u64 ui_get_hash(Source_Location loc, i64 identifier);

bool is_inside(f32 x, f32 y, Rect r); // Check if point is inside rect.

Rect get_rect(f32 x, f32 y, f32 w, f32 h);
void get_quad(Rect r, Vector2 *p0, Vector2 *p1, Vector2 *p2, Vector2 *p3);
u32 get_status_flags(Rect r);
f32 get_float_parameter(Rect r, Size_Relativeness relativeness, f32 local_value, f32 fixed_value);

void draw_ui();

void push_scissor(Rect r);
void pop_scissor();

struct Button_State;
Vector4 get_color_for_button(Button_Theme *theme, Button_State *state, f32 over_factor, f32 pressed_factor); // @Cleanup: Move over_factor and pressed_factor into state!!!

my_pair<f32 /*over_factor*/, f32 /*pressed_flash_factor*/> update_production_value_button(Rect r, bool changed, Button_State *state, u32 status_flags, Button_Theme *theme);

my_pair<bool /*pressed*/, Button_State*> button(Rect r, String text, Button_Theme *theme = NULL, Texture_Map *map = NULL, i64 identifier = 0, Source_Location loc = Source_Location::current());

template <typename T>
bool slider(Rect _r, T *value, T min_value, T max_value, String prefix, String suffix, Slider_Theme *theme = NULL, T spinbox_step = 1, bool drag = true, i64 identifier = 0, Source_Location loc = Source_Location::current());

void label(Rect r, String text, Label_Theme *theme);
void draw_arrow(Button_State *state, Button_Theme *theme, Vector2 p4, Vector2 p5, Vector2 p6);

bool has_focus(Rect r);

Vector4 darken(Vector4 color, f32 t);
Vector4 whiten(Vector4 color, f32 t);

my_pair<Rect /*right*/,  Rect /*remainder*/> cut_right(Rect r, f32 amount);
my_pair<Rect /*left*/,   Rect /*remainder*/> cut_left(Rect r, f32 amount);
my_pair<Rect /*top*/,    Rect /*remainder*/> cut_top(Rect r, f32 amount);
my_pair<Rect /*bottom*/, Rect /*remainder*/> cut_bottom(Rect r, f32 amount);

void rounded_rectangle(Rect r, Rectangle_Shape shape, Vector4 color);

extern f32 ui_current_dt;

extern Overall_Theme default_overall_theme;

enum Key_Current_State : u32;
extern Key_Current_State mouse_button_left_state;
extern Key_Current_State mouse_button_right_state;

void ui_handle_event(Event event);
