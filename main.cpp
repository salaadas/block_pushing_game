// @Hack @Hack @Hack: This is a hack to force the ldr to not use multi-sampled texture.
// @Hack @Hack @Hack: This is a hack to force the shadow map to not use multi-sampled texture.

#include "common.h"

// OpenGL
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_X11
#include <GLFW/glfw3native.h>

// GLM
// @Note: GLM uses a column major ordering for matrices
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
// Linux
#include <unistd.h>
#include <X11/Xlib.h>
#include <limits.h>

#include <stb_image.h>

#include "main.h"
#include "file_utils.h"
#include "path_utils.h"
#include "opengl.h"
#include "time_info.h"
#include "draw.h"
#include "font.h"
#include "player_control.h"
#include "hotloader.h"
#include "sokoban.h"
#include "level_set.h"
#include "events.h"
#include "hud.h"
//#include "audio.h"

// #include "sound_catalog.h"
// #include "sound.h"

// #define NON_RESIZABLE_MODE

const String LEVEL_SET_NAME("heroes1");
const String OVERRIDE_LEVEL_NAME("heroes1_11");
// const String OVERRIDE_LEVEL_NAME("heroes1_8");

const f32    DT_MAX = 0.15f;
const i32    DESIRED_WIDTH  = 1600;
const i32    DESIRED_HEIGHT = 900;
const String PROGRAM_NAME("Heroes of sokoban");
const String FONT_FOLDER("data/fonts/");
i32          BIG_FONT_SIZE = 32; // @Note: This font size changes depending on the window's size

Program_Mode program_mode = Program_Mode::GAME;

// True if set from command-line args
bool         window_dimension_set = false; // @Fixme: unhandled
bool         should_quit = false;
f32          windowed_aspect_ratio_h_over_w;
Display     *x_global_display = NULL;
String       dir_of_running_exe;

bool         was_window_resized_this_frame = true; // Set to true to resize on first frame
i32          resized_width  = DESIRED_WIDTH;
i32          resized_height = DESIRED_HEIGHT;
// Sound_Player *sound_player;

RArr<Catalog_Base*>     all_catalogs;
Shader_Catalog          shader_catalog;
Texture_Catalog         texture_catalog;
Level_Set_Catalog       level_set_catalog;
Animation_Catalog       animation_catalog;
Animation_Names_Catalog animation_names_catalog;
Animation_Graph_Catalog animation_graph_catalog;

Animation_Graph *human_animation_graph = NULL;

Mesh_Catalog    mesh_catalog;
// Sound_Catalog   sound_catalog;

Texture_Map *white_texture = NULL;

// @Hardcode:
bool is_fullscreen = false;
#include <X11/Xatom.h>
void toggle_fullscreen()
{
    assert((x_global_display != NULL));

    auto window = glfwGetX11Window(glfw_window);

    Atom wm_state      = XInternAtom(x_global_display, "_NET_WM_STATE", true);
    Atom wm_fullscreen = XInternAtom(x_global_display, "_NET_WM_STATE_FULLSCREEN", true);

    XEvent x_event;
    memset(&x_event, 0, sizeof(XEvent));

    x_event.type = ClientMessage;
    x_event.xclient.window = window;
    x_event.xclient.message_type = wm_state;
    x_event.xclient.format = 32;

    if (is_fullscreen)
        x_event.xclient.data.l[0] = false;
    else
        x_event.xclient.data.l[0] = true;

    x_event.xclient.data.l[1] = wm_fullscreen;
    x_event.xclient.data.l[2] = 0;

    XSendEvent(x_global_display, DefaultRootWindow(x_global_display), False, SubstructureRedirectMask | SubstructureNotifyMask, &x_event);

    is_fullscreen = !is_fullscreen;
}

// Error callback for GLFW
void glfw_error_callback(i32 error, const char *description);

void glfw_window_size_callback(GLFWwindow *window, i32 new_width, i32 new_height);

void init_window()
{
    String exe = get_executable_path();

    i32 last_slash = find_index_from_right(exe, '/');
    exe.count = last_slash; // Upto but not including the last slash

    dir_of_running_exe = copy_string(exe);
    setcwd(dir_of_running_exe);

    // Reset temporary storage here because we use a lot of memory in get_executable_path
    // This is due to allocating PATH_MAX amount for the buffer storing the exe path
    reset_temporary_storage();
}

void init_gl(i32 render_target_width, i32 render_target_height, bool vsync = true, bool windowed = true)
{
    // Set the error callback first before doing anything
    glfwSetErrorCallback(glfw_error_callback);

    // Handle error
    assert(glfwInit() == GLFW_TRUE);

    // Hints the about-to-created window's properties using:
    // glfwWindowHint(i32 hint, i32 value);
    // to reset all the hints to their defaults:
    // glfwDefaultWindowHints();
    // ^ good idea to call this BEFORE setting any hints BEFORE creating any window

#ifdef NON_RESIZABLE_MODE
    glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
#endif

    glfwWindowHint(GLFW_SCALE_TO_MONITOR, 1); // Make window DPI aware (scaling the game accordingly to the monitor).

    {
        i32 width, height;
        width  = render_target_width;
        height = render_target_height;
        if (height < 1) height = 1;
        windowed_aspect_ratio_h_over_w = height / (f32)width;

        if (!window_dimension_set)
        {
            i32 limit_w, limit_h;

            // @Note: X11 way to get the dimension of the screen
            {
                x_global_display = glfwGetX11Display();
                assert((x_global_display != NULL));

                auto display = x_global_display;
                auto snum    = DefaultScreen(display);

                i32 desktop_height = DisplayHeight(display, snum);
                i32 desktop_width  = DisplayWidth(display, snum);

                // @Fixme: The screen query here is actually wrong because it merges both monitors into one.
                printf("              -----> Desktop width %d, height %d\n", desktop_width, desktop_height);

                limit_h = (i32)desktop_height;
                limit_w = (i32)desktop_width;
            }

            i32 other_limit_h = (i32)(limit_w * windowed_aspect_ratio_h_over_w);
            i32 limit = limit_h < other_limit_h ? limit_h : other_limit_h; // std::min(limit_h, other_limit_h);

            if (height > limit)
            {
                f32 ratio = limit / (f32)height;
                height    = (i32)(height * ratio);
                width     = (i32)(width  * ratio);
            }

            render_target_height = height;
            render_target_width  = width;
        }
    }

    // Creates both the window and context with which to render into
    if (windowed) glfw_window = glfwCreateWindow(render_target_width, render_target_height, (char*)PROGRAM_NAME.data, NULL, NULL);
    else          glfw_window = glfwCreateWindow(render_target_width, render_target_height, (char*)PROGRAM_NAME.data,
                                                 glfwGetPrimaryMonitor(), NULL);

    // Before we can use the context, we need to make it current
    glfwMakeContextCurrent(glfw_window);

    if (!vsync) glfwSwapInterval(0);
    else glfwSwapInterval(1);

    hook_our_input_event_system_to_glfw(glfw_window);

    // Properties that can be controlled after creating the window:
    // - glfwSetWindowSize(GLFWwindow *window, i32 width, i32 height);
    // - glfwSetWindowPos(GLFWwindow *window, i32 x_pos, i32 y_pos);
    // similarly, we can:
    // - glfwGetWindowSize(GLFWwindow *window, i32 *width, i32 *height);
    // - glfwGetWindowPos(GLFWwindow *window, i32 *x_pos, i32 *y_pos);
    // or if you want to set a callback to the size and position of the window when it is changed, do:
    // - glfwSetWindowSizeCallback(...);
    // - glfwSetWindowPosCallback(...);
    glfwSetWindowSizeCallback(glfw_window, glfw_window_size_callback);

    // glfwGetWindowSize() returns the size of the window in pixels, which is skewed if the window system
    // uses scaling.
    // To retrieve the actual size of the framebuffer, use
    // glfwGetFrambuffersize(GLFWwindow *window, i32 *width, i32 *height);
    // you can also do
    // glfwSetFramebuffersizeCallback(...);


    // GLFW provides a mean to associate your own data with a window:
    // void *glfwGetWindowUserPointer(GLFWwindow *window);
    // glfwSetWindowUserPointer(GLFWwindow *window, void *pointer);


    // @Important: NOW COMES THE OPENGL GLUE THAT ALLOWS THE USE OF OPENGL FUNCTIONS
    // this is where we use the "glad.h" lib
    // we must set this up before using any OpenGL functions
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    // check for DSA support
    if (!GLAD_GL_ARB_direct_state_access)
    {
        fprintf(stderr, "GLAD: DSA is not supported\n");
        exit(1);
    }

    {
        auto result = create_texture_rendertarget(XXX_the_offscreen_buffer_width, XXX_the_offscreen_buffer_height, true, true);
        the_offscreen_buffer = result.first;
        the_depth_buffer     = result.second;
        assert((the_depth_buffer != NULL));

        {
            // @Hack @Hack @Hack: This is a hack to force the ldr to not use multi-sampled texture.
            // @Hack @Hack @Hack: This is a hack to force the ldr to not use multi-sampled texture.
            // @Hack @Hack @Hack: This is a hack to force the ldr to not use multi-sampled texture.

            auto old_multisampling = multisampling;
            multisampling = false;

            auto result = create_texture_rendertarget(XXX_the_offscreen_buffer_width, XXX_the_offscreen_buffer_height, false, false); // This is only used to blit the multi-sampled FBO. It is useful for tone mapping from HDR to LDR.

            the_ldr_buffer = result.first;
            multisampling  = old_multisampling;
        }

        // @Fixme: So many similar variable names for sizes
        auto back_buffer_width  = render_target_width;
        auto back_buffer_height = render_target_height;
        
        the_back_buffer = New<Texture_Map>(false);
        init_texture_map(the_back_buffer);
        the_back_buffer->width  = back_buffer_width;
        the_back_buffer->height = back_buffer_height;

        // Creating shadow map color buffer and its depth buffer. We want the color buffer
        // of the shadow map in order to debug its output.
        {
            // @Hack @Hack @Hack: This is a hack to force the shadow map to not use multi-sampled texture.
            // @Hack @Hack @Hack: This is a hack to force the shadow map to not use multi-sampled texture.
            // @Hack @Hack @Hack: This is a hack to force the shadow map to not use multi-sampled texture.

            auto old_multisampling = multisampling;
            multisampling = false;

            shadow_map_width  = 2048;
            shadow_map_height = 2048; // Make this settable later.

            auto shadow = create_texture_rendertarget(shadow_map_width, shadow_map_height, true);
            shadow_map_buffer = shadow.first;
            shadow_map_depth  = shadow.second;

            multisampling  = old_multisampling;
        }
    }

    object_to_world_matrix = Matrix4(1.0);
    world_to_view_matrix   = Matrix4(1.0);
    view_to_proj_matrix    = Matrix4(1.0);
    object_to_proj_matrix  = Matrix4(1.0);

    num_immediate_vertices = 0;
    should_vsync           = vsync;
    in_windowed_mode       = windowed;

    glGenVertexArrays(1, &opengl_is_stupid_vao);
    glGenBuffers(1, &immediate_vbo);
    glGenBuffers(1, &immediate_vbo_indices);

    // @Temporary: Clearing the initial color of the game when loading
    glClearColor(.03, .10, .13, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glfwSwapBuffers(glfw_window);
}

void init_window_and_gl(i32 width, i32 height)
{
    render_target_width  = width;
    render_target_height = height;

    init_window(); // This modifies the render_target_width and render_target_height accordingly
    init_gl(render_target_width, render_target_height);
}

void init_context()
{
    global_context.allocator.proc    = __default_allocator;
    global_context.allocator.data    = NULL; // since regular malloc doesn't have a pointer to memory

    global_context.temporary_storage = &__default_temporary_storage;

    global_context.current_level_set = NULL;
}

void resize_offscreen_buffer_size(i32 new_width, i32 new_height)
{
    the_back_buffer->width  = new_width;
    the_back_buffer->height = new_height;

    f32  back_buffer_width  = new_width;
    f32  back_buffer_height = new_height;
    f32  back_buffer_denom  = std::max(back_buffer_height, 1.0f);
    auto window_aspect      = back_buffer_width / back_buffer_denom;

    // @Hardcode:
    constexpr auto desired_aspect_x = 16.0f;
    constexpr auto desired_aspect_y =  9.0f;
    auto desired_denom    = std::max((f32)desired_aspect_y, 1.0f);
    auto desired_aspect   = desired_aspect_x / desired_denom;

    f32 w, h;
    if (window_aspect > desired_aspect)
    {
        // Using the height of the back buffer for the offscreen buffer
        h = back_buffer_height;
        w = h * desired_aspect;
    }
    else
    {
        // Using the width of the back buffer for the offscreen buffer
        w = back_buffer_width;
        h = w / desired_aspect;
    }

    if (w < 1) w = 1;
    if (h < 1) h = 1;

    the_offscreen_buffer->width  = (i32)(floorf(w));
    the_offscreen_buffer->height = (i32)(floorf(h));
    size_color_target(the_offscreen_buffer, true);

    {
        // @Hack @Hack @Hack: This is a hack to force the ldr to not use multi-sampled texture.
        // @Hack @Hack @Hack: This is a hack to force the ldr to not use multi-sampled texture.
        // @Hack @Hack @Hack: This is a hack to force the ldr to not use multi-sampled texture.

        auto old_multisampling = multisampling;
        multisampling = false;

        the_ldr_buffer->width  = (i32)(floorf(w));
        the_ldr_buffer->height = (i32)(floorf(h));

        size_color_target(the_ldr_buffer, false);

        multisampling = old_multisampling;
    }

    // @Fixme @Fixme @Fixme: Game view is skewed when resize.
    // @Fixme @Fixme @Fixme: Game view is skewed when resize.
    // @Fixme @Fixme @Fixme: Game view is skewed when resize.
    the_depth_buffer->width  = (i32)(floorf(w));
    the_depth_buffer->height = (i32)(floorf(h));
    size_depth_target(the_depth_buffer);
}

void update_audio() // @Incomplete:
{
}

i64 highest_water = -1;
i64 frame_index = 0;

void do_one_frame()
{
    frame_index += 1;
    i64 hw = std::max(highest_water, global_context.temporary_storage->high_water_mark);
    // printf("[frame no.%ld] Highest water mark is %ld\n", frame_index, hw);

    if (hw > highest_water)
    {
        highest_water = hw;
        log_ts_usage();
    }

    reset_temporary_storage();

    update_time(DT_MAX);

    update_linux_events();
    glfwPollEvents();

    // This must happend after the glfwPollEvents
    // @Note: was_window_resized_this_frame is set in glfw_window_size_callback function
    // This flag will be resetted at the end of the frame.
    if (was_window_resized_this_frame)
    {
        resize_offscreen_buffer_size(resized_width, resized_height);
        
        deinit_all_font_stuff_on_resize(); // Because it they depends on the height of the render target

        BIG_FONT_SIZE = render_target_height * .08f;

        // @Fixme: Setting fader font and fps font here, because the font stuff will get deinit.
        fader_font = get_font_at_size(FONT_FOLDER, String("KarminaBold.otf"), BIG_FONT_SIZE * 1.0);
        fps_font   = get_font_at_size(FONT_FOLDER, String("AnonymousProRegular.ttf"), BIG_FONT_SIZE * .3);
    }

    read_input();

    // Update/simulation
    if (program_mode == Program_Mode::GAME)
    {
        hide_os_cursor(glfw_window); // @Cleanup: Move this into the draw phase.
        simulate_sokoban(); // @Fixme: Turn me back on when finish the material system.

        // auto manager = get_entity_manager();
        // update_transition(manager);
    }
    else
    {
        // @Temporary: Currently, we disable the cursor for EDITOR mode.
        // However, we would like to show the cursor when the editor has
        // some ui elements (buttons, toggles, ...)
        disable_os_cursor(glfw_window); // @Cleanup: Move this into the draw phase below.
    }

    update_audio();

    // editor_frame_start();
    // update_editor();

    // Draw
    // if (program_mode == Program_Mode::GAME)
    {
        // @Cleanup:
        auto manager = get_entity_manager();

        // decals_update(manager);
        update_game_camera(manager);
        // update_signs(manager);
        draw_game_view_3d();

        // set_depth_target(0, NULL);
        // glDisable(GL_DEPTH_TEST);

        // resolve_to_ldr(manager);

        // glEnable(GL_BLEND); // For alpha blending.
        draw_hud(manager);

        // draw_transition();
        // draw_debug_view(); // This is in draw.cpp

        post_frame_cleanup(manager); // @Cleanup: This should be inside simulate(), after we fix the entity destruction thing we will move it there.
    }
/*
    else if (program_mode == Program_Mode::MENU)
    {
        draw_menu();
    }
    else
    {
        auto manager = open_entity_manager();
        auto world = get_open_world();

        auto camera = &manager->camera;
        auto camera->postion     = world->camera_position;
        auto camera->orientation = world->camera_orientation;

        draw_editor_view();
        draw_debug_view();
    }
*/

    // draw_console(); // Only if developer;

    // editor_frame_end();

    glfwSwapBuffers(glfw_window);

    while (hotloader_process_change()) {}
    // @Incomplete: currently we do perform_reloads inside hotloader_process_change,
    // so there is no need to do it outside here.
    // for (auto it : all_catalogs) perform_reloads(it);

    // @Speed:
    if (was_window_resized_this_frame) was_window_resized_this_frame = false;
}

#include <sys/socket.h>    // For socket().
#include <sys/ioctl.h>     // For ioctl().
#include <net/if.h>        // For ifconf.
#include <netinet/in.h>    // For IPPROTO_IP.

my_pair<RArr<String>, bool /*success*/> os_get_mac_addresses()
{
    constexpr auto BUFFER_SIZE = 2000;
    auto buffer = reinterpret_cast<char*>(my_alloc(BUFFER_SIZE));
    defer { my_free(buffer); };

    RArr<String> addresses;

    auto sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock == -1) return {addresses, false};

    ifconf ifc = {};
    ifc.ifc_len = BUFFER_SIZE;
    ifc.ifc_buf = buffer;

    auto status = ioctl(sock, SIOCGIFCONF, &ifc);
    if (status == -1) return {addresses, false};

    auto nitems = ifc.ifc_len / sizeof(struct ifreq);
    for (i64 i = 0; i < nitems; ++i)
    {
        auto it = ifc.ifc_req + i;

        ifreq req = {};
        memcpy(req.ifr_name, it->ifr_name, IFNAMSIZ);

        status = ioctl(sock, SIOCGIFFLAGS, &req);
        if (status != 0)                  continue; // Should we bail?
        if (req.ifr_flags & IFF_LOOPBACK) continue; // Ignore loopback.

        status = ioctl(sock, SIOCGIFHWADDR, &req);
        if (status != 0) continue;

        auto s = req.ifr_hwaddr.sa_data;
        auto address = sprint(String("%02x-%02x-%02x-%02x-%02x-%02x"),
                              (u8)s[0], (u8)s[1], (u8)s[2], (u8)s[3], (u8)s[4], (u8)s[5]);

        array_add(&addresses, address);
        // printf("Address is '%s'\n", temp_c_string(address));
    }

    return {addresses, true};
}

my_pair<String, bool /*success*/> os_get_username()
{
    constexpr auto BUFFER_SIZE = 256 + 1;

    char buf[BUFFER_SIZE];

    auto not_ok = getlogin_r(buf, BUFFER_SIZE);
    if (not_ok) return {String(""), false};

    auto name = copy_string(String(buf));
    return {name, true};
}

void my_hotloader_callback(Asset_Change *change, bool handled); // @ForwardDeclare

int main()
{
    init_context();

    // MAC address and username.
    // @Incomplete: Hook this up to the editor when we change the entity format.
    {
        auto [mac_addresses, mac_success] = os_get_mac_addresses();
        if (!mac_success)
        {
            logprint("main.cpp", "Failed to get MAC address for device (required for editor to work)!\n");
        }

        auto [username, username_success] = os_get_username();    
        if (!username_success)
        {
            logprint("main.cpp", "Failed to get the username for device (required for editor to work)!\n");
        }
    }

    // @Note: Init all the catalogs
    init_shader_catalog(&shader_catalog);
    init_texture_catalog(&texture_catalog);
    init_level_set_catalog(&level_set_catalog);

    init_mesh_catalog(&mesh_catalog);
    init_animation_catalog(&animation_catalog);
    init_animation_names_catalog(&animation_names_catalog);
    init_animation_graph_catalog(&animation_graph_catalog);

    // @Note: Then, add catalogs into the catalog table
    array_add(&all_catalogs, &shader_catalog.base);
    array_add(&all_catalogs, &texture_catalog.base);
    array_add(&all_catalogs, &level_set_catalog.base);
    array_add(&all_catalogs, &mesh_catalog.base);
    array_add(&all_catalogs, &animation_catalog.base);
    array_add(&all_catalogs, &animation_names_catalog.base);
    array_add(&all_catalogs, &animation_graph_catalog.base);

    init_window_and_gl(DESIRED_WIDTH, DESIRED_HEIGHT);

    catalog_loose_files(String("data"), &all_catalogs);

    newline(); // New line here because we want to differentiate the catalogs' logs with the game-loop ones.

    reset_temporary_storage();

    // @Important: Cannot init these after before is initted!!!
    white_texture = catalog_find(&texture_catalog, String("white"));
    // the_missing_mesh = catalog_find(&texture_catalog, String("missing_asset")); // Should be a question mark @Incomplete:

    // We init the shaders (after the catalog_loose_files). For draw.cpp
    init_shaders();

    hotloader_init(); 
    hotloader_register_callback(my_hotloader_callback);

    human_animation_graph = make_human_animation_graph();

    init_sokoban(); // @Cleanup: Collapse this with the above stuff into init_game.

    while (!glfwWindowShouldClose(glfw_window))
    {
        if (should_quit) break;

        do_one_frame();
    }

    newline();
    printf("Exiting..\n");

    glfwDestroyWindow(glfw_window);
    glfwTerminate();

    hotloader_shutdown();

    newline();
    printf("Giving the OS all the allocated memory....\n");

    return(0);
}

// Error callback for GLFW
void glfw_error_callback(i32 error, const char *description)
{
    fprintf(stderr, "GLFW ERROR [%d]: %s", error, description);
    exit(1);
}

void glfw_window_size_callback(GLFWwindow *window, i32 new_width, i32 new_height)
{
    glViewport(0, 0, new_width, new_height);

    resized_width  = new_width;
    resized_height = new_height;
    was_window_resized_this_frame = true;
}

#include "vars.h"
void my_hotloader_callback(Asset_Change *change, bool handled)
{
    if (handled) return;

    auto full_name = change->full_name;
    auto [short_name, ext] = chop_and_lowercase_extension(change->short_name);

    if (change->extension == String("ascii_level"))
    {
        // Check if the the changed level is in our current level set.
        auto set = global_context.current_level_set;
        assert((set != NULL));
        assert((set->level_names.count));

        auto found = array_find(&set->level_names, short_name);

        if (!found) return;

        // Only reload if the editted file is the current level
        i64 probe_level_index = 0;
        for (auto it : set->level_names)
        {
            if ((it == short_name) && probe_level_index == current_level_index)
            {
                // Restart the current level
                init_level_general(true);
            }

            probe_level_index += 1;
        }
    }
    else if (change->extension == String("variables"))
    {
        reload_variables(short_name, full_name);
    }
    else
    {
        logprint("hotloader_callback", "Unhandled non-catalog asset change: %s\n", temp_c_string(full_name));
    }
}
