// References: https://github.com/focus-editor/focus/blob/main/modules/Linux_Display/ldx_display.jai
// https://github.com/focus-editor/focus/blob/main/modules/Clipboard/Clipboard.jai

// https://www.glfw.org/docs/3.0/group__clipboard.html#gaba1f022c5eb07dfac421df34cdcd31dd

// Eventually figure out how to do this in native X11

#include "clipboard.h"

#include "opengl.h"

void os_clipboard_set_text(String s)
{
    auto c_string = reinterpret_cast<char*>(temp_c_string(s));
    glfwSetClipboardString(glfw_window, c_string);
}

String os_clipboard_get_text()
{
    const char *clipboard_c_string = glfwGetClipboardString(glfw_window);

    auto result = String(clipboard_c_string);

    // Something went wrong, NULL was returned
    if (!clipboard_c_string)
    {
        result = String("");
        logprint("os_clipboard_get_text", "Was not able to get the content of clipboard.\n");
    }

    return result;
}
