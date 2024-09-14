#include "ui_states.h"

Table<u64, Button_State>            table_button;
Table<u64, Checkbox_State>          table_checkbox;
Table<u64, Text_Input_State>        table_text_input;
Table<u64, Slider_State>            table_slider;
Table<u64, Scrollable_Region_State> table_scrollable_region;
Table<u64, Dropdown_State>          table_dropdown;
Table<u64, Color_Picker_State>      table_color_picker;
Table<u64, Slidable_Region_State>   table_slidable_region;
Table<u64, Subwindow_State>         table_subwindow;

u64 combine_hashes(u64 a, u64 b)
{
    constexpr u64 KNUTH_GOLDEN_RATIO_64 = 1140071481932319848ULL;
    return a * KNUTH_GOLDEN_RATIO_64 + b;
}

u64 ui_get_hash(Source_Location loc, i64 identifier)
{
    auto hash = reinterpret_cast<u64>(loc.m_file_name) * static_cast<u64>(loc.m_line + 1) * static_cast<u64>(identifier + 1);
    return hash;
}
