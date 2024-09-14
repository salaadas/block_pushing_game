/*
********************  TODO:  ********************
NEXT DO THE VISUAL/GRAPHICS OF THE GAME because it looks like shit currently:
- Mesh VBO's
- Editor UI

- Visual Interpolation of moves (not completely)
- Interpolation of rotation.

- Skeletal animations
- Text fader of level
- Animation playback
- Time controlled animation



- Shadow mapping
- HDR
- Bloom, Light Emitters
- Lightmaps

- Transitions between levels

WEEKEND:
- Fix the leaks.
- Save game on exit
- Save game on multi-levels levelsets
- Undo must handle entities destructions/creations.

- We should not lock the movement of the player when someone was dead by a gate! (Do something else)
- Handling noclip

THESE COULD BE DONE LATER WHEN I'M BORED
- Menu
- Sound

For now, we actually won't care about the buffered_moves, because we will update the position immediately after moving, so there is also no need for the visual interpolation of the move.
But that will be the next goal to do.

2024/07/31
I imagine the game loop is as folows:

- clear the temporary storage
- update time
- poll events for screen resize and inputs (keyboard/mouse)
- handle resizes
- read the input from the poll events

- simulation of sokoban:
    - reset the entities that were moved in the previous frame.
    - buffer the input (because we might not have done with the visual interpolation, so we don't want to skip out on any rapid inputs)
    - validate the input

    - enact a move if done processing the previous one
    - get the current active hero
    - check if the potential move is valid
    - if yes, proceed
    - get the associated entities that will be affected with the move
    - sync those entities with the mover

    - move the entities physically (by physically, we mean updating the logical grid so that their discrete positions are changed) if the center of the mover crossed the tile spacing.
    - update the orientation of the entities, so that the visual interpolation can interpolate between the starting theta and the target theta.
    - add the visual interpolation for the move.

- draw the game onto the framebuffer:
    
- display the framebuffer to the backbuffer

- swap buffers

- process hotload changes
*/

#include "sokoban.h"

#include "main.h"
#include "player_control.h"
#include "time_info.h"
#include "metadata.h"
#include "undo.h"
#include "vars.h"

Gameplay_Visuals gameplay_visuals;

Entity_Manager *sokoban_entity_manager = NULL;

RArr<Entity*> entities_moved_this_frame;

i32 current_level_index = -1;
bool noclip = false;

bool loading_first_level = true;

void post_move_reevaluate(Entity_Manager *manager);

// @Cleanup: This doesn't need to be this complicated
void init_level_general(bool reset)
{
    if (sokoban_entity_manager)
    {
        if (reset) reset_entity_manager(sokoban_entity_manager);
    }
    else
    {
        sokoban_entity_manager = make_entity_manager();
        reset = true;
        current_level_index = 0;
    }

    if (reset)
    {
        // Reset the undo system
        reset_undo(sokoban_entity_manager->undo_handler);

        auto set = global_context.current_level_set;
        assert((set != NULL));
        assert((set->level_names.count));

        String level_name = set->level_names[current_level_index];

#ifdef DEVELOPER_MODE
        // @Hack: Initially switch to this level on startup
        if (loading_first_level)
        {
            i64 probe_level_index = 0;
            for (auto it : set->level_names)
            {
                if (it == OVERRIDE_LEVEL_NAME)
                {
                    level_name = it;
                    current_level_index = probe_level_index;
                }

                probe_level_index += 1;
            }

            loading_first_level = false;
        }
#endif

        // @Temporary @Hardcoded: level_path as files in the data/levels directory
        String level_path = tprint(String("data/levels/%s.ascii_level"), temp_c_string(level_name));

        load_ascii_level(sokoban_entity_manager, level_path);

        // After load the level, mark the beginning of the undo system
        undo_mark_beginning(sokoban_entity_manager);
    }
}

void advance_level(i32 delta, bool due_to_victory)
{
    current_level_index += delta;

    auto set = global_context.current_level_set;
    assert((set != NULL));

    auto num_levels = set->level_names.count;
    assert(num_levels);

    // @Incomplete:
    // if (current_level_index == num_levels && due_to_victory)
    // {
    //     // Where highest distance is the longest distance from a guy to the door block.
    //     // Imagine a guy moved toward a door block and some other guys were already waiting
    //     // for him. So we make the transition slowly to fade out the screen.
    //     start_waiting_for_victory(highest_guy_distance);
    //     printf("You actually won!!!!\n");
    //     exit(1);
    // }

    current_level_index = std::clamp(current_level_index, 0, static_cast<i32>(num_levels - 1));

    init_level_general(true);
}

void init_sokoban()
{
    assert((LEVEL_SET_NAME.count));
    auto set = catalog_find(&level_set_catalog, LEVEL_SET_NAME);
    global_context.current_level_set = set;

    // Must init all the metadata before doing anying related to it.
    init_metadata_for_all_entities_type();
    init_variables();

    init_level_general(true);
}

Entity_Manager *get_entity_manager()
{
    return sokoban_entity_manager;
}

Transaction_Id get_next_transaction_id(Entity_Manager *manager)
{
    auto result = manager->next_transaction_id_to_issue;
    manager->next_transaction_id_to_issue += 1;

    return result;
}

bool maybe_retire_next_transaction(Entity_Manager *manager)
{
    auto id = manager->next_transaction_id_to_retire;
    if (id == manager->next_transaction_id_to_issue) return false;

    for (auto it : manager->moving_entities)
    {
        auto v = &it->visual_interpolation;

        if (v->start_time < 0) continue;
        if (v->transaction_id) return false;

        assert((v->transaction_id >= manager->next_transaction_id_to_retire));
    }

    if (id == manager->waiting_on_player_transaction)
    {
        manager->waiting_on_player_transaction = 0;
    }

    manager->next_transaction_id_to_retire += 1;
    return true;
}

template <class Target_Type>
Target_Type *find_type_at(Proximity_Grid *grid, Vector3 pos)
{
    RArr<Entity*> found;
    found.allocator = {global_context.temporary_storage, __temporary_allocator};
    find_at_position(grid, pos, &found);

    for (auto e : found)
    {
        if (cmp_var_type_to_type(e->type, Target_Type))
        {
            return Down<Target_Type>(e);
        }
    }

    return NULL;
}

Entity *find_pullable_at(Proximity_Grid *grid, Vector3 pos)
{
    RArr<Entity*> found;
    found.allocator = {global_context.temporary_storage, __temporary_allocator};
    find_at_position(grid, pos, &found);

    for (auto e : found)
    {
        if (e->dead) continue;

        if (cmp_var_type_to_type(e->type, Rock)) return e;
        // else if (cmp_var_type_to_type(e->type, Monster)) return e;
        else if (cmp_var_type_to_type(e->type, Guy)) return e;
    }

    return NULL;
}

// @Cleanup: remove the use of hit_obstance, instead, return NULL.
Entity *find_teleportable_at(Proximity_Grid *grid, Vector3 pos, bool *hit_obstacle)
{
    RArr<Entity*> found;
    found.allocator = {global_context.temporary_storage, __temporary_allocator};
    find_at_position(grid, pos, &found);
    
    for (auto e : found)
    {
        if (e->dead) continue;

        if (cmp_var_type_to_type(e->type, Rock))
        {
            return e;
        }
        // else if (cmp_var_type_to_type(e->type, Monster)) return e;
        else if (cmp_var_type_to_type(e->type, Guy))
        {
            return e;
        }
        else if (cmp_var_type_to_type(e->type, Wall))
        {
            *hit_obstacle = true;
        }
        else if (cmp_var_type_to_type(e->type, Gate))
        {
            auto gate = Down<Gate>(e);

            if (!gate->open) *hit_obstacle = true;
        }
    }

    return NULL;
}

bool find_heavy_thing_at(Proximity_Grid *grid, Vector3 pos)
{
    RArr<Entity*> found;
    found.allocator = {global_context.temporary_storage, __temporary_allocator};
    find_at_position(grid, pos, &found);

    for (auto e : found)
    {
        if (e->dead) continue;

        if (cmp_var_type_to_type(e->type, Wall)) return true;
        if (cmp_var_type_to_type(e->type, Rock)) return true;
        if (cmp_var_type_to_type(e->type, Guy)) return true;
        // if (cmp_var_type_to_type(e->type, Monster)) return true;
    }

    return false;
}

Guy *get_active_hero(Entity_Manager *manager)
{
    auto index = manager->active_hero_index;

    if (0 <= index && (index < manager->turn_order.count))
    {
        auto id = manager->turn_order[index];
        auto e = find_entity(manager, id);

        if (e && (cmp_var_type_to_type(e->type, Guy)))
        {
            auto guy = Down<Guy>(e);
            return guy;
        }
    }

    return NULL;
}

RArr<Guy*> get_clones_of_active_hero(Entity_Manager *manager)
{
    RArr<Guy*> clones;
    clones.allocator = {global_context.temporary_storage, __temporary_allocator};

    auto active_hero = get_active_hero(manager);

    if (!active_hero) return clones;

    Pid id_of_host = active_hero->base->entity_id;

    for (auto it : manager->_by_guys)
    {
        if (it == active_hero) continue;

        Pid clone_of = it->clone_of_id;

        // @Fixme: FIX this, because clones entities should not have the same entity_id as one another.
        // Instead, use turn_order_id to find the clones
        if ((clone_of != it->base->entity_id) &&
            (clone_of == id_of_host))
        {
            array_add(&clones, it);
        }
    }

    return clones;
}

Vector3 vector_to_dot_with;
bool sort_by_dot_product(Entity *a, Entity *b)
{
    auto a_dot = glm::dot(a->position, vector_to_dot_with);
    auto b_dot = glm::dot(b->position, vector_to_dot_with);

    return a_dot < b_dot;
}

Direction reverse_direction(Direction dir)
{
    auto new_dir = static_cast<Direction>((static_cast<u32>(dir) + 2) & 3);

    return new_dir;
}

f32 wrap_degrees(f32 dtheta)
{
    if (dtheta < -180) dtheta += 360;
    if (dtheta >  180) dtheta -= 360;

    return dtheta;
}

void turn_toward(Guy *guy, Vector3 delta)
{
    if (!(delta.x || delta.y)) return;

    auto theta = static_cast<f32>(atan2(delta.y, delta.x));
    auto desired_theta_target = theta * (360 / TAU);

    auto e = guy->base;

    e->theta_current = wrap_degrees(e->theta_current);
    auto magnitude = desired_theta_target - e->theta_current;

    if (magnitude >  180) desired_theta_target -= 360;
    if (magnitude < -180) desired_theta_target += 360;

    e->theta_target = desired_theta_target;
}

inline
Door *is_on_door(Entity *e) // Called in check_for_victory
{
    auto grid = e->manager->proximity_grid;

    auto result = find_type_at<Door>(grid, e->position);
    return result;
}

void check_for_victory(Entity_Manager *manager)
{
    auto guys = manager->_by_guys;
    if (guys.count == 0) return;

    RArr<Door*> unoccupied_doors;
    unoccupied_doors.allocator = {global_context.temporary_storage, __temporary_allocator};

    for (auto it : manager->_by_doors) array_add(&unoccupied_doors, it);

    bool all_on_doors = true;

    for (auto it : guys)
    {
        auto door = is_on_door(it->base);

        if (door && !it->base->dead)
        {
            array_unordered_remove_by_value(&unoccupied_doors, door);
        }
        else
        {
            all_on_doors = false;
            continue;
        }
    }

    // We can't return in the loop because we might do some animation for all the guys!
    if (!all_on_doors) return;

    // @Incomplete: Handle level transitions.
    advance_level(+1, true);
}

void update_physical_position(Entity *e, Vector3 new_position)
{
    auto manager = e->manager;
    auto grid = manager->proximity_grid;
    auto delta = new_position - e->position;

    auto [success, removed_entity] = remove_from_grid(grid, e);

    if (!success)
    {
        printf("[update_physical_position] Was not able to move guy with id %d\n", e->entity_id);
        return;
    }

    assert((removed_entity == e));
    e->position = new_position;

    // SuperDumb @Hack: Remove this immediately. @Fixme
    {
        f32 theta_target = 0.0f;
        if (delta.x > 0) theta_target = 0;
        if (delta.x < 0) theta_target = 180;
        if (delta.y > 0) theta_target = 90;
        if (delta.y < 0) theta_target = 270;

        e->theta_target = theta_target;
    }

    add_to_grid(grid, e);

    if (!doing_undo)
    {
        array_add_if_unique(&entities_moved_this_frame, e);
    }
}

void move_entity(Entity *e, Vector3 delta, bool is_teleport, Pid sync_id = 0, f32 duration = -1.0f, Transaction_Id transaction_id = 0, Source_Location loc = Source_Location::current())
{
    auto manager = e->manager;

    auto new_position = e->position + delta;

    if (!doing_undo)
    {
        auto _duration = duration;
        if ((_duration < 0) && (cmp_var_type_to_type(e->type, Guy)))
        {
            // auto override_speed = get_move_speed_override(Down<Guy>(e));
            auto override_speed = -1.0;
            if (override_speed > 0) _duration = 1 / override_speed;
        }

        // Pid moving_onto_id; // @Note: Used this with mobile_supporter (I think it is related to falling).
        add_visual_interpolation(e, e->position, new_position, is_teleport, _duration, transaction_id, loc);

        // if (moving_onto_id) e->visual_interpolation.moving_onto_id = moving_onto_id;
        if (sync_id != e->entity_id) e->visual_interpolation.sync_id = sync_id; // Callers can be sloppy and pass their own id; in those cases we igonre it. This simpilifies some 'if' statements.
    }

    update_physical_position(e, new_position);
}

bool can_go(Entity *mover, Proximity_Grid *grid, Vector3 desired_pos, Vector3 delta, bool can_push, RArr<Entity*> *pushes, bool voluntarily = true) // Pid id_to_ignore = 0
{
    auto index = position_to_index(grid->gridlike, desired_pos);

    if (index < 0) return false; // Out of bounds!

    // If anyone is moving into this position, we can't go.
    auto manager = mover->manager;
    for (auto it : manager->moving_entities)
    {
        // auto v = &it->visual_interpolation;

        if (it == mover) continue;
        // if (v->start_time < 0) continue;

        // if (glm::distance(v->physical_end, desired_pos) < 0.5f)
        // {
        //     printf("BLOCKING V IS: %p (Open debugger for more info)\n", v);
        //     return false;
        // }
    }

    RArr<Entity*> found;
    found.allocator = {global_context.temporary_storage, __temporary_allocator};
    find_at_position(grid, desired_pos, &found);

    for (auto e : found)
    {
        // if (e->entity_id == id_to_ignore) continue;
        // if (e->entity->scheduled_for_destructoin) continue;
        if (cmp_var_type_to_type(e->type, Guy))
        {
            auto guy = Down<Guy>(e);
            if (guy->base->dead) continue;
        }

        if (e->entity_id == mover->entity_id &&
            (delta != Vector3(0, 0, 0)))
        {
            printf("[can_go]: Next position of should not have the current mover there!!!!\n");
            assert(0);
            return true;
        }

        if (cmp_var_type_to_type(e->type, Gate))
        {
            auto gate = Down<Gate>(e);
            if (!gate->open) return false;
        }

        if (cmp_var_type_to_type(e->type, Wall))
        {
            return false;
        }

        if (cmp_var_type_to_type(e->type, Rock) ||
            cmp_var_type_to_type(e->type, Guy))
        {
            if (!can_push) return false;

            auto new_desired_pos = e->position + delta;
            if (can_go(mover, grid, new_desired_pos, delta, can_push, pushes, false))
            {
                array_add(pushes, e);
            }
            else
            {
                return false;
            }
        }
    }

    // Return true because the target position is empty.
    return true;
}

void move_individual_guy(Guy *guy, Vector3 delta, Transaction_Id transaction_id = 0)
{
    // turn_toward(guy, delta);

    auto manager = guy->base->manager;
    auto grid = manager->proximity_grid;
    auto original_pos = guy->base->position;

    if (!noclip)
    {
        auto wall = find_type_at<Wall>(grid, original_pos);
        if (wall) delta = Vector3(0, 0, 0);
    }

    // @Cleanup: merge with the movement code for fighter/thief
    if (guy->can_teleport)
    {
        Vector3 teleport_delta = delta;

        // The other entity to swap with.
        Entity *other = NULL;

        auto index = position_to_index(grid->gridlike, original_pos + teleport_delta);
        // Stop if out of bound
        while (index != -1)
        {
            bool found_obstacle = false;

            other = find_teleportable_at(grid, original_pos + teleport_delta, &found_obstacle);
            if (other)
            {
                delta = teleport_delta;
                break;
            }

            if (found_obstacle)
            {
                // Only move by one block because there is nothing to swap between.
                teleport_delta = delta;
                break;
            }

            teleport_delta += delta;

            index = position_to_index(grid->gridlike, original_pos + teleport_delta);            
        }

        if (index == -1)
        {
            // Should not get here
            assert(0);
        }

        if (other)
        {
            move_entity(other, teleport_delta * -1.0f, false);
        }
        else
        {
            // Which means there are nothing to swap with.
            // Then we check if we can go regularly to the desired position.
            bool can = can_go(guy->base, grid, original_pos + teleport_delta, delta, guy->can_push, NULL, true);
            if (!can) return;
        }
        
        move_entity(guy->base, teleport_delta, guy->can_teleport, 0, -1, transaction_id);
    }
    else
    {
        auto target_position = original_pos + delta;

        RArr<Entity*> pushed;
        pushed.allocator = {global_context.temporary_storage, __temporary_allocator};

        bool voluntarily = true;
        bool can = can_go(guy->base, grid, target_position, delta, guy->can_push, &pushed, voluntarily);

        if (!can) return;

        move_entity(guy->base, delta, guy->can_teleport, 0, -1, transaction_id);
        auto sync_id = guy->base->entity_id;

        for (auto it : pushed)
        {
            // Setting teleport to false because you don't want the wizard to teleport when pushed.
            move_entity(it, delta, false, sync_id, -1, transaction_id);
        }

        if (guy->can_pull)
        {
            auto pulled_entity = find_pullable_at(grid, original_pos - delta);

            if (pulled_entity)
            {
                // We assume that the original position of the Thief is not occupied
                // Right after it has moved.
                // @Todo: Should we should do a can_go check here?
                move_entity(pulled_entity, delta, false, sync_id, -1, transaction_id);
            }
        }
    }

    // We know that we have moved if we reach here, so we call undo handler to cache to differences.

    auto undo_handler = manager->undo_handler;
    undo_end_frame(undo_handler);

    post_move_reevaluate(manager);
    check_for_victory(manager); // @Incomplete: Wait for all movement visuals to end before transitioning levels.
}

void evaluate_gates(Entity_Manager *manager)
{
    // We only open gate(s) of a certain flavor if all the switches of that
    // flavor is pressed.
    u32 cached_maybe_gates_should_open = 0;
    u32 should_switch_not_be_evaluated = 0;

    //
    // First we evaluate all the switches and press it if there is something heavy on it.
    //
    auto grid = manager->proximity_grid;
    for (auto it : manager->_by_switches)
    {
        auto pos = it->base->position;
        auto flavor = it->flavor;

        auto unpress = (1 << flavor) & should_switch_not_be_evaluated;

        if (!unpress && find_heavy_thing_at(grid, pos))
        {
            it->pressed = true;
            cached_maybe_gates_should_open |= (1 << flavor);
        }
        else
        {
            it->pressed = false;
            cached_maybe_gates_should_open &= ~(1 << flavor);
            should_switch_not_be_evaluated |= (1 << flavor);
        }
    }

    //
    // Next we active the gates accordingly
    //

    for (auto it : manager->_by_gates)
    {
        auto bit_mask = 1 << it->flavor;

        bool should_open = (bit_mask & cached_maybe_gates_should_open);
        it->open = should_open;

        // Any entity that is under the closing gate will be destroyed!
        if (!should_open)
        {
            RArr<Entity*> found;
            found.allocator = {global_context.temporary_storage, __temporary_allocator};
            find_at_position(grid, it->base->position, &found);

            for (auto to_destroy : found)
            {
                if (to_destroy == it->base) continue;

                to_destroy->dead = true;

                // @Fixme: we should not cleanup this guy, instead, make him dead and then render him differently.
                array_add(&manager->entities_to_clean_up, to_destroy);
            }
        }
    }
}

void post_move_reevaluate(Entity_Manager *manager)
{
    evaluate_gates(manager);
    // evaluate_doors(manager);
}

void post_undo_reevaluate(Entity_Manager *manager)
{
    evaluate_gates(manager);

    // This is a @Hack, but we need to change the active_hero_index before updating all the active
    // guys' flags, this is because when we apply the diff's, the manager's active_hero_index doesn't
    // change. So here, we need to manually set it back.
    //
    // See entity_manager.cpp, the part where we set the turn_index
    {
        i32 active_index = manager->active_hero_index;
        bool has_set_active_index = false;

        for (auto guy : manager->_by_guys)
        {
            if (guy->active)
            {
                assert(!has_set_active_index);
                active_index = guy->turn_order_index;
            }
        }

        manager->active_hero_index = active_index;
    }

    update_guy_active_flags(manager);
}

void enact_move(Entity_Manager *manager, Buffered_Move move, bool is_interrupt_move = false)
{
    // if ((transition_mode == Transition::WAITING_FOR_VICTORY) || (transition_mode == Transition::VICTORY)) return;

    assert(!is_interrupt_move);
    if (is_interrupt_move) return;

    auto guy_id = move.id;
    auto delta  = move.delta;
    auto caused_changes = false;
    auto some_not_dead  = false;

    RArr<Entity*> clones_to_move;
    clones_to_move.allocator = {global_context.temporary_storage, __temporary_allocator};

    // @Cleanup: Factor out to a get_clones_of function (remove the get_clones_of_active_hero_function)
    auto guys = manager->_by_guys;
    for (auto other : guys)
    {
        if (other->base->scheduled_for_destruction) continue;
        if ((other->clone_of_id == guy_id) || other->base->entity_id == guy_id)
        {
            array_add(&clones_to_move, other->base);
        }
    }

    auto e = find_entity(manager, guy_id);
    Guy *guy = NULL;
    if (cmp_var_type_to_type(e->type, Guy)) guy = Down<Guy>(e);

    // Is this too specific? I don't think it is right anymore...
    if (guy && guy->base->dead) return; // No move for the dead

    auto transaction_id = get_next_transaction_id(manager);
    manager->waiting_on_player_transaction = transaction_id;

    if (clones_to_move.count)
    {
        vector_to_dot_with = delta;
        array_qsort(&clones_to_move, sort_by_dot_product);

        for (auto it : clones_to_move)
        {
            if (it->dead) continue;
            some_not_dead = true;

            auto other = Down<Guy>(it);
            move_individual_guy(other, delta, transaction_id);
        }

        // If anyone can't go, we can't go.
        // Repeat until no further changes.
        auto num_reverted = 0;
        RArr<Entity*> movers;
        movers.allocator = {global_context.temporary_storage, __temporary_allocator};

        for (auto it : manager->moving_entities)
        {
            if (it->visual_interpolation.transaction_id == transaction_id)
            {
                array_add(&movers, it);
            }
        }

        for (auto it : clones_to_move)
        {
            if (!it->dead) array_add_if_unique(&movers, it);
        }

        auto anyone_moved = false;
        for (auto it : clones_to_move)
        {
            if (it->dead) continue;
            if (it->visual_interpolation.start_time >= 0)
            {
                anyone_moved = true;
                if (it->visual_interpolation.move_type != Move_Type::STATIONARY)
                {
                    caused_changes = true;
                }
                break;
            }
        }

        // @Incomplete: Uncomment the below block:
        // if (!anyone_moved)
        // {
        //     for (auto it : clones_to_move)
        //     {
        //         if (some_not_dead)
        //         {
        //             // Take time... @Note: Should we even do this?
        //             // move_entity(it, it->position, false, 0, 0.4f, transaction_id);
        //         }
        //         else
        //         {
        //             auto guy = Down<Guy>(it);
        //             if (guy) wiggle(guy);
        //         }
        //     }
        // }
    }

    manager->player_transaction_caused_changes = caused_changes;
}

void queue_game_control(Entity_Manager *manager, Buffered_Move::Action_Type action_type, Vector3 delta, bool is_repeat, bool actually_perform_immediately = false)
{
    // maybe_revert_map_zoom_to_local();
    // if ((transition_mode == Transition::WAITING_FOR_VICTORY) || (transition_mode == Transition::VICTORY)) return;

    auto guy = get_active_hero(manager);
    auto guy_id = guy->base->entity_id;

    if (!guy_id) return;

    Buffered_Move move;
    move.id = guy_id;
    move.action_type = action_type;
    move.delta = delta;
    move.is_repeat = is_repeat;

    if (actually_perform_immediately)
    {
        enact_move(manager, move);
        return;
    }

    // // Just test this as an early out... hopefully this has no negative ramifications for animation.
    // if (manager->waiting_on_player_transaction)
    // {
    //     for (auto it : manager->moving_entities)
    //     {
    //         if (it->turn_order_id != guy_id) continue;
    //         if (it->visual_interpolation.start_time < 0) continue;

    //         if (!cmp_var_type_to_type(it->type, Guy)) continue;

    //         auto guy = Down<Guy>(it);
    //         maybe_send_going_far(guy);
    //     }
    // }

    array_add(&manager->buffered_moves, move);
}

void queue_game_control(Vector3 delta, bool is_repeat = false)
{
    auto manager = get_entity_manager();
    queue_game_control(manager, Buffered_Move::DELTA, delta, is_repeat);
}

inline
bool XXX_should_queue_input(Key_Info *info)
{
    bool result = false;

    if (info->pressed)
    {
        if (info->signalled)
        {
            info->signalled = false;
            result = true;
        }
        else
        {
            auto dt = timez.current_dt;
            info->time_accumulated += dt;

            if (info->time_accumulated >= TIME_PER_MOVE_REPEAT)
            {
                info->time_accumulated -= TIME_PER_MOVE_REPEAT;
                result = true;
            }
        }
    }

    return result;
}

void generate_buffered_moves_from_held_keys(Entity_Manager *manager)
{
    // @Temporary: :PlayerControl
    if (XXX_should_queue_input(&key_left))  queue_game_control(Vector3(-1,  0, 0), key_left.repeat);
    if (XXX_should_queue_input(&key_right)) queue_game_control(Vector3( 1,  0, 0), key_right.repeat);
    if (XXX_should_queue_input(&key_down))  queue_game_control(Vector3( 0, -1, 0), key_down.repeat);
    if (XXX_should_queue_input(&key_up))    queue_game_control(Vector3( 0,  1, 0), key_up.repeat);

    /*
      @Note: Eventually, we want to do this:
    if (key_left.pressed)  queue_game_control(Vector3(-1,  0, 0));
    if (key_right.pressed) queue_game_control(Vector3( 1,  0, 0));
    if (key_down.pressed)  queue_game_control(Vector3( 0, -1, 0));
    if (key_up.pressed)    queue_game_control(Vector3( 0,  1, 0));
    */
}

void enact_next_buffered_move(Entity_Manager *manager, bool first_call)
{
    // if (transition_mode == Transition::WAITING_FOR_VICTORY) return;

    if (!manager->buffered_moves.count)
    {
        if (first_call)
        {
            generate_buffered_moves_from_held_keys(manager);
            enact_next_buffered_move(manager, false);
        }

        return;
    }

    if (manager->waiting_on_player_transaction) return;

    auto move = manager->buffered_moves[0];
    array_ordered_remove_by_index(&manager->buffered_moves, 0);

    enact_move(manager, move);
}

void do_input(Entity_Manager *manager)
{
    enact_next_buffered_move(manager);
}

void simulate(Entity_Manager *manager)
{
    auto buffered = 0;
    for (auto it : manager->buffered_moves)
    {
        if (!it.is_repeat) buffered += 1;
    }

    if (buffered > 1)
    {
        printf("!!!!!!!!!!! More than one buffered = %d\n", buffered);
        // @Incomplete: Speed up the animation dt here.
    }

    update_entities_visual_interpolation(manager, timez.current_dt);

    // // @Incomplete:

    // auto active_guy = get_active_hero(manager);

    // if (!active_guy)
    // {
    //     printf("[simulate_sokoban]: Tried to move guy but couldn't find an active one.\n");
    //     return;
    // }

    // move_individual_guy(active_guy, delta);

    // // After we move the active hero, move the associated clones.
    // auto clones = get_clones_of_active_hero(manager);
    // for (auto it : clones)
    // {
    //     move_individual_guy(it, delta);
    // }

    // post_move_reevaluate(manager);
    // check_for_victory(manager);
}

void simulate_sokoban()
{
    array_reset(&entities_moved_this_frame);

    auto manager = get_entity_manager();

    do_input(manager);
    simulate(manager);
}
