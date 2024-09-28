#include "animation_graph.h"

#include "entity_manager.h" // For Human_Animation_State.
#include "animation_names.h"
#include "sokoban.h"        // For play_animation().

typedef Animation_Graph::Node Node;
typedef Animation_Graph::Arc  Arc;
typedef Arc::Condition        Condition;

Node *find_node(Animation_Graph *graph, String state_name);

Node *add_node(Animation_Graph *graph, String state_name)
{
    auto node = array_add(&graph->nodes);

    node->state_name = copy_string(state_name);
    node->node_index = graph->nodes.count - 1; // Subtracting one because the array_add above increments the count already.

    return node;
}

Arc *add_arc(Animation_Graph *graph, Node *source_node, String destination_name, i32 destination_line_number, String message = String(""))
{
    auto arc = array_add(&graph->arcs);
    arc->destination_name = destination_name; // The string is not allocated...
    arc->destination_line_number = destination_line_number;

    if (!message)
    {
        if (source_node->when_done)
        {
            logprint("add_arc", "Attempted to add an when_done arc to state '%s' but we already have one via calling the message '%s'!\n", temp_c_string(source_node->state_name), temp_c_string(source_node->when_done->message));
            return arc;
        }
        
        source_node->when_done = arc;
    }
    else
    {
        arc->message = copy_string(message);
        array_add(&source_node->message_arcs, arc);
    }

    return arc;
}

#include "main.h"
Animation_Graph *make_human_animation_graph()
{
    auto graph = catalog_find(&animation_graph_catalog, String("human"));
    return graph;
}

#include "main.h"
#include "sampled_animation.h"
void switch_to_node(Animation_Graph *graph, Human_Animation_State *state, Node *destination, f32 fade_time = -1.0f)
{
    state->node_index = destination->node_index;

#if defined(ANIM_DEVELOPER)
    if (state->node_name) free_string(&state->node_name);
    state->node_name = copy_string(destination->state_name);
#endif

    auto names = state->animation_names;
    auto [anim_name, success] = table_find(&names->name_to_anim_name, destination->tag);

    if (!success)
    {
        logprint("switch_to_node", "Animation Graph node '%s' told us to switch to animation with tag '%s', but we could not find a binding this tag in Animation_Names '%s'.\n", temp_c_string(destination->state_name), temp_c_string(destination->tag), temp_c_string(names->name));
        return;
    }

    auto e = state->entity;
    anim_name = join(3, e->mesh_name, String("_"), anim_name); // Joining the string to get consistent behavior with the other play_animation();
    auto anim = catalog_find(&animation_catalog, anim_name);

    if (!anim || !anim->num_samples)
    {
        logprint("switch_to_node", "Cannot find the sampled animation '%s' for tag '%s'.\n", temp_c_string(anim_name), temp_c_string(destination->tag));
        return;
    }

    if (fade_time < 0) play_animation(e, anim);
    else               play_animation(e, anim, 0, fade_time);
}

// @Incomplete: Generalize from Human_Animation_State to something that is more applicable
// to other entities (Gates, Switches, Inanimates).

#if defined(ANIM_DEVELOPER)
void recover_from_edits(Animation_Graph *graph, Human_Animation_State *state)
{
    if (!graph->nodes.count) return;
    if (!state->node_name)   return; // First time coming in, so we don't know what to recover to.

    if (state->last_graph_edit_index == graph->edit_index)
    {
        assert(state->node_index >= 0);
        assert(state->node_index < graph->nodes.count);
        return;
    }

    state->last_graph_edit_index = graph->edit_index;

    auto new_node = find_node(graph, state->node_name);
    if (new_node)
    {
        switch_to_node(graph, state, new_node);
    }
    else
    {
        new_node = &graph->nodes[0];

        logprint("recover_from_edits", "Animation graph '%s' changed and a character was in animation node '%s' but that node no longer exists in the modified graph. Resetting to state 0 ('%s').\n", temp_c_string(graph->name), temp_c_string(state->node_name), temp_c_string(new_node->state_name));

        // We are sure that we have at least one node here because otherwise we would have early outted.
        switch_to_node(graph, state, new_node);
        return;
    }

    assert(state->node_index >= 0);
    assert(state->node_index < graph->nodes.count);
}
#else
void recover_from_edits(Animation_Graph *graph, Human_Animation_State *state)
{
    if (!graph->nodes.count) return;

    if ((state->node_index < 0) || (state->node_index >= graph->nodes.count))
    {
        logprint("recover_from_edits", "Error: Node index out of range for graph '%s'.\n", temp_c_string(graph->name));

        state->node_index = 0;
    }
}
#endif

#include "animation_player.h"
#include "animation_channel.h"
void animation_graph_per_frame_update(Animation_Graph *graph, Human_Animation_State *state, Animation_Player *player)
{
    // Find the channel playing the currently active animation.
    // Maybe we should have a more-formal way to designate this.

    if (!graph->nodes.count) return;

    auto channel = get_primary_animation_channel(player);
    if (!channel) return;

    recover_from_edits(graph, state);

    // Get the node that we're in.
    auto current_node = &graph->nodes[state->node_index];

    auto remaining = channel->end_time - channel->current_time;

    auto arc = current_node->when_done;
    if (!arc || !arc->destination) return;

    auto outro_time = arc->outro_time;
    if (arc->outro_time_is_in_fraction)
    {
        outro_time = outro_time * channel->end_time;
    }

    if (remaining <= outro_time)
    {
        switch_to_node(graph, state, arc->destination, remaining);
    }
}

Node *find_node(Animation_Graph *graph, String state_name)
{
    for (auto &node : graph->nodes)
    {
        auto it = &node;
        if (it->state_name == state_name) return it;
    }

    return NULL;
}

my_pair<f32, bool> get_time_of_primary_animation(Entity *e)
{
    auto player = e->animation_player;

    if (!player)           return {0, false};
    if (!player->channels) return {0, false};

    auto primary = get_primary_animation_channel(e->animation_player);

    if (!primary)
    {
        return {0, false};
    }

    return {primary->current_time, true};
}

// Later, we should consider passing a sub-struct of Human_Animation_State to here,
// so that we can animate more things other than human.
void send_message(Animation_Graph *graph, Human_Animation_State *state, String message, String default_state) // The default_state is the name of the node to switch to if we could not find the message arc.
{
    if (!graph->nodes.count) return;

    recover_from_edits(graph, state);

    f32 fade_time = -1.0f; // Default value of 'fade_time' in switch_to_node().

    // Get the node that we're in.
    auto current_node = &graph->nodes[state->node_index];
    Node *destination = NULL;

    for (auto it : current_node->message_arcs)
    {
        if (it->message == message)
        {
            auto accept = true;

            auto primary = get_primary_animation_channel(state->entity->animation_player);

            if (it->condition == Condition::TIME_INTERVAL)
            {
                auto [t, success] = get_time_of_primary_animation(state->entity);

                auto t0 = it->time_0;
                auto t1 = it->time_1;
                if (it->condition_is_in_fraction)
                {
                    if (primary)
                    {
                        t0 = t0 * primary->end_time;
                        t1 = t1 * primary->end_time;
                    }
                    else
                    {
                        logprint("send_message", "Tried to use a fraction in terms of animation channel duration for the conditional start %f%% and end %f%% in message arc '%s' but we couldn't find any animation channel available?\n", t0 * 100, t1 * 100, temp_c_string(it->message));
                    }
                }

                if (!success)
                {
                    accept = false;
                }
                else if ((t < t0) || (t > t1))
                {
                    accept = false;
                }
            }
            else if (it->condition == Condition::NONE)
            {
                // Do nothing.
            }

            if (accept)
            {
                destination = it->destination;

                if (it->blend_duration != -1.0f)
                {
                    auto blend_duration = it->blend_duration;

                    if (it->blend_duration_is_in_fraction)
                    {
                        if (primary)
                        {
                            blend_duration = primary->end_time * blend_duration;
                        }
                        else
                        {
                            logprint("send_message", "Tried to use a fraction in terms of animation channel duration for the blend factor %f%% in message arc '%s' but we couldn't find any animation channel available?\n", blend_duration * 100, temp_c_string(it->message));
                        }
                    }

                    fade_time = blend_duration;
                }

                break;
            }
        }
    }

    if (!destination)
    {
        auto node = find_node(graph, default_state);
        if (!node)
        {
            logprint("send_message", "Could not find the destination node with name '%s' for the default state....\n", temp_c_string(default_state));
            return;
        }

        destination = node;
    }

    switch_to_node(graph, state, destination, fade_time);
}

void postprocess_arc(Animation_Graph *graph, Arc *arc)
{
    assert(arc->destination_name);
    arc->destination = find_node(graph, arc->destination_name);

    if (!arc->destination)
    {
        logprint("Animation_Graph postprocess", "Error on line %d: Could not find the destination node with name '%s' for the arc message '%s'!\n", arc->destination_line_number, temp_c_string(arc->destination_name), temp_c_string(arc->message));
    }
}

void postprocess(Animation_Graph *graph)
{
    for (auto &node : graph->nodes)
    {
        auto it = &node;
        
        if (!it->tag)
        {
            logprint("Animation_Graph postprocess", "Node '%s' does not have an animation tag!\n", temp_c_string(it->state_name));
        }

        for (auto arc : it->message_arcs)
        {
            postprocess_arc(graph, arc);
        }

        auto when_done = it->when_done;
        if (when_done)
        {
            postprocess_arc(graph, when_done);
        }
    }
}

void init_animation_graph_catalog(Animation_Graph_Catalog *catalog)
{
    catalog->base.my_name = String("Animation Graph");
    array_add(&catalog->base.extensions, String("animation_graph"));
    do_polymorphic_catalog_init(catalog);
}

Animation_Graph *make_placeholder(Animation_Graph_Catalog *catalog, String short_name, String full_name)
{
    auto graph       = New<Animation_Graph>();
    graph->name      = copy_string(short_name);
    graph->full_path = copy_string(full_name);

    return graph;
}

void reload_asset(Animation_Graph_Catalog *catalog, Animation_Graph *graph)
{
    if (graph->loaded)
    {
        for (auto &node : graph->nodes)
        {
            free_string(&node.state_name);
            free_string(&node.tag);
        }
        array_reset(&graph->nodes);

        for (auto &arc : graph->arcs)
        {
            free_string(&arc.message);
        }
        array_reset(&graph->arcs);
    }

    graph->edit_index += 1;

    load_animation_graph(graph);
}

#include "file_utils.h"
my_pair<String /*remainder*/, bool /*success*/> add_information_to_arc(Arc *arc, Text_File_Handler *handler, String content)
{
    auto c_agent = temp_c_string(handler->log_agent);
    auto [potential_command, rhs] = break_by_spaces(content);

    my_pair<String, bool> failed_to_parse_return = {String(""), false};

    if (potential_command == String("time"))
    {
        if (!rhs)
        {
            logprint(c_agent, "Error on line %d: Expected 2 floating-point values after the 'time' command, but got nothing (Example is: time 0.5 100).\n", handler->line_number);
            return failed_to_parse_return;
        }

        //
        // The start time.
        //
        bool success = false;
        auto [start_time, start_remainder] = string_to_float(rhs, &success);

        if (!success)
        {
            logprint(c_agent, "Error on line %d: Failed to parse the start time of the 'time' command, got '%s'!\n", handler->line_number, temp_c_string(rhs));
            return failed_to_parse_return;
        }
        else if (!start_remainder)
        {
            logprint(c_agent, "Error on line %d: Missing the end time value for the 'time' command!\n", handler->line_number);
            return failed_to_parse_return;
        }
        else if (start_time < 0)
        {
            logprint(c_agent, "Error on line %d: Start time for the 'time' command should not be negative, got %f.\n", handler->line_number, start_time);
            return failed_to_parse_return;
        }

        bool is_start_a_fraction = false;
        // Checking whether the start time is in fraction/percentage:
        if (start_remainder[0] == '%')
        {
            start_time /= 100.0f;
            advance(&start_remainder, 1);

            is_start_a_fraction = true;
        }

        //
        // The end time.
        //
        auto [end_time, the_rest] = string_to_float(start_remainder, &success);

        if (!success)
        {
            logprint(c_agent, "Error on line %d: Failed to parse the end time of the 'time' command, got '%s'!\n", handler->line_number, temp_c_string(start_remainder));
            return failed_to_parse_return;
        }
        else if (end_time < 0)
        {
            logprint(c_agent, "Error on line %d: End time for the 'time' command should not be negative, got %f.\n", handler->line_number, end_time);
            return failed_to_parse_return;
        }

        // Checking whether the end time is in fraction/percentage:
        bool is_end_a_fraction = false;

        if (the_rest.count && the_rest[0] == '%')
        {
            // If the start time was not a fraction but the end time is, we do error.
            if (!is_start_a_fraction && (start_time != 0))
            {
                logprint(c_agent, "Error on line %d: Start time (%f) and end time (%f%%) must be either both fraction/percentage or floating-points in terms of seconds!\n", handler->line_number, start_time, end_time);
                return failed_to_parse_return;
            }

            end_time /= 100.0f;
            advance(&the_rest, 1);

            is_end_a_fraction = true;
        }

        // If the start time was a fraction and the end time is not, we do error.
        if (is_start_a_fraction && !is_end_a_fraction)
        {
            logprint(c_agent, "Error on line %d: Start time (%f%%) and end time (%f) must be either both fraction/percentage or floating-points in terms of seconds!\n", handler->line_number, start_time * 100, end_time);
            return failed_to_parse_return;
        }

        //
        // Adding start time and end time to the arc.
        //
        if (arc->condition != Condition::NONE)
        {
            logprint(c_agent, "Error on line %d: Attempted to re-assigned the condition of the arc '%s'!\n", handler->line_number, temp_c_string(arc->message));
            return failed_to_parse_return;
        }

        arc->condition = Condition::TIME_INTERVAL;
        arc->time_0 = start_time;
        arc->time_1 = end_time;
        arc->condition_is_in_fraction = is_end_a_fraction;

        return {the_rest, true};
    }
    else if (potential_command == String("blend"))
    {
        auto success = false;
        auto [blend_duration, rhs2] = string_to_float(rhs, &success);

        if (!success)
        {
            logprint(c_agent, "Error on line %d: Command 'blend' was not able to parse the blend duration from '%s'!\n", handler->line_number, temp_c_string(rhs));
            return failed_to_parse_return;
        }

        if (arc->blend_duration != -1.0f)
        {
            logprint(c_agent, "Error on line %d: Blend duration was set more than once!\n", handler->line_number);
        }

        if (rhs2.count && rhs2[0] == '%')
        {
            blend_duration /= 100.0f;
            advance(&rhs2, 1);

            arc->blend_duration_is_in_fraction = true;
        }

        arc->blend_duration = blend_duration;

        return {rhs2, true};
    }

    // Not a valid command that we know of.
    logprint(c_agent, "Error on line %d: Command is not one of the type we know of '%s', the potential ones are 'time', 'blend'.\n", handler->line_number, temp_c_string(potential_command));

    return failed_to_parse_return;
}

void load_animation_graph(Animation_Graph *graph)
{
    auto highest_mark = get_temporary_storage_mark();
    defer { set_temporary_storage_mark(highest_mark); };

    Text_File_Handler handler;
    String agent("Animation Names");

    auto full_path = graph->full_path;
    start_file(&handler, full_path, agent);
    if (handler.failed) return;
    defer { deinit(&handler); };

    auto c_agent = temp_c_string(agent);
    Node *current_node = NULL;

    while (true)
    {
        auto [line, found] = consume_next_line(&handler);

        if (!found) break;

        if (line[0] == ':') // Start of a new state
        {
            advance(&line, 1);
            eat_spaces(&line);
            
            auto [state_name, rhs] = break_by_spaces(line);
            if (rhs)
            {
                logprint(c_agent, "Junk on line %d after command '%s', junk is '%s'!\n", handler.line_number, temp_c_string(state_name), temp_c_string(rhs));
            }

            current_node = add_node(graph, state_name);
        }
        else
        {
            auto [command, rhs] = break_by_spaces(line);

            if (!current_node)
            {
                logprint(c_agent, "Error on line %d: Tried to set command '%s' before declaring any nodes!\n", handler.line_number, temp_c_string(command));
                return;
            }

            if (command == String("animation"))
            {
                auto [tag, rhs2] = break_by_spaces(rhs);
                if (rhs2)
                {
                    logprint(c_agent, "Found junk on line %d after setting the animation tag to '%s'.\n", handler.line_number, temp_c_string(tag));
                }

                if (!tag)
                {
                    logprint(c_agent, "Error on line %d: Attempted to assign an animation tag to state '%s' but the tag name is missing. Skipping...\n", handler.line_number, temp_c_string(current_node->state_name));
                    continue;
                }

                if (current_node->tag)
                {
                    logprint(c_agent, "Error on line %d: Attempted to override the existing animation tag '%s' for state '%s' with tag '%s'! Skipping...\n", handler.line_number, temp_c_string(current_node->tag), temp_c_string(current_node->state_name), temp_c_string(tag));
                    continue;
                }
                
                current_node->tag = copy_string(tag);
            }
            else if (command == String("message"))
            {
                // Example: message go_state_active StateItoa
                auto [message_name, message_rhs]  = break_by_spaces(rhs);
                auto [destination_name, dest_rhs] = break_by_spaces(message_rhs);

                if (!destination_name)
                {
                    logprint(c_agent, "Error on line %d: Attempted to make a message arc '%s', but it is missing the destination state name. Skipping...\n", handler.line_number, temp_c_string(message_name));
                    continue;
                }

                if (!message_name)
                {
                    logprint(c_agent, "Error on line %d: Attempted to make a message arc for state '%s', but it is missing the message name to intercept. Skipping...\n", handler.line_number, temp_c_string(current_node->state_name));
                    continue;
                }

                auto arc = add_arc(graph, current_node, destination_name, handler.line_number, message_name);

                while (dest_rhs)
                {
                    auto [new_remainder, success] = add_information_to_arc(arc, &handler, dest_rhs);

                    if (success)
                    {
                        dest_rhs = new_remainder;
                    }
                    else
                    {
                        // logprint(c_agent, "Error on line %d: Junk on line after node name, junk is '%s'!\n", handler.line_number, temp_c_string(dest_rhs));
                        break;
                    }
                }
            }
            else if (command == String("when_done"))
            {
                auto [destination_name, dest_rhs] = break_by_spaces(rhs);
                f32 outro_time = -1;
                bool outro_time_is_in_fraction = false;

                if (dest_rhs)
                {
                    bool success = false;
                    auto [time_in_float, junk] = string_to_float(dest_rhs, &success);

                    if (junk && junk[0] == '%')
                    {
                        time_in_float /= 100.0f;
                        outro_time_is_in_fraction = true;

                        advance(&junk, 1);
                    }

                    if (!success || junk)
                    {
                        logprint(c_agent, "Found junk on line %d after setting the when_done arc to '%s', junk is '%s'.\n", handler.line_number, temp_c_string(destination_name), temp_c_string(dest_rhs));
                    }
                    else if (time_in_float < 0)
                    {
                        logprint(c_agent, "Error on line %d: Time value for the crossfade between arcs cannot be a negative found (got %f)!\n", handler.line_number, outro_time_is_in_fraction ? (time_in_float * 100) : time_in_float);
                    }
                    else
                    {
                        outro_time = time_in_float;
                    }
                }

                if (!destination_name)
                {
                    logprint(c_agent, "Error on line %d: Attempted to make a when_done arc for state '%s', but it is missing the node name. Skipping...\n", handler.line_number, temp_c_string(current_node->state_name));
                    continue;
                }

                auto arc = add_arc(graph, current_node, destination_name, handler.line_number);

                if (outro_time > 0)
                {
                    arc->outro_time = outro_time;
                    arc->outro_time_is_in_fraction = outro_time_is_in_fraction;
                }
            }
            else
            {
                logprint(c_agent, "Error on line %d, found unknown command '%s'!\n", handler.line_number, temp_c_string(command));
            }
        }
    }

    postprocess(graph);
}
