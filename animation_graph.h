#pragma once

#include "common.h"

#define ANIM_DEVELOPER // Turn this off when we ship the game.

struct Animation_Graph
{
    String name;           // For catalog.
    String full_path;      // For catalog.
    bool   loaded = false; // For catalog.

    struct Arc;

    struct Node
    {
        String state_name; // Name of the state that this node represents.
        String tag;        // Tag is mapped to the 'preferred name' part in the *.animation_names file via the Animation_Names.

        RArr<Arc*> message_arcs;
        Arc *when_done = NULL; // If this is null, the animation will repeat / stay in this state.

        i32 node_index = -1; // This is the index of the node inside the 'nodes' array.
    };

    struct Arc
    {
        i32    destination_line_number = -1;
        String destination_name;         // This string is not allocated; rather, it is used in the postprocessing phase of loading the Animation Graph to get the 'Node *destination'.
        Node   *destination = NULL;

        String message; // If we send this message, we go to the destination. If this mesage is empty, this is the 'when_done' arc.

        bool outro_time_is_in_fraction = false;
        f32 outro_time = .2f; // The amount of time before the end of the previous animation that we switch nodes and begin the crossfade.

        enum Condition
        {
            NONE = 0,
            TIME_INTERVAL
        };

        Condition condition = NONE;
        // If conditioned on time:
        bool condition_is_in_fraction = false; // If this is true, the below is a factor between [0..1].
        f32 time_0 = 0.0f;
        f32 time_1 = 0.0f;

        bool blend_duration_is_in_fraction = false; // If this is true, blend_duration is a factor between [0..1].
        f32 blend_duration = -1.0f; // This is only meaningful if blend_duration == -1.0f.
    };

    i32 edit_index = 0; // Increments by 1 each time we change the graph (by hotloading).

    RArr<Node> nodes;
    RArr<Arc>  arcs;
};

Animation_Graph::Node *add_node(Animation_Graph *graph, String tag);
Animation_Graph *make_human_animation_graph();

struct Human_Animation_State;
void send_message(Animation_Graph *graph, Human_Animation_State *state, String message, String default_state);

struct Animation_Player;
void animation_graph_per_frame_update(Animation_Graph *graph, Human_Animation_State *state, Animation_Player *player);

void load_animation_graph(Animation_Graph *graph);

// Catalog stuff below here:
template <typename V>
struct Catalog;

using Animation_Graph_Catalog = Catalog<Animation_Graph>;

void init_animation_graph_catalog(Animation_Graph_Catalog *catalog);
void reload_asset(Animation_Graph_Catalog *catalog, Animation_Graph *graph);
