#include "movement_visual.h"

#include "sokoban.h"
#include "entity_manager.h"
#include "time_info.h"
#include "player_control.h"

f32 get_accel_time(Guy *guy)
{
    auto accel_time = gameplay_visuals.move_accel_time_default;

    // @Incomplete:
    // auto s = &guy->animation_state;
    // auto as = &s->animation_state;
    // auto node = guy->animation_graph.nodes[as->node_index];
    // if (node->move_accel_time_override >= 0)
    // {
    //     accel_time = node->move_accel_time_override;
    // }

    return accel_time;
}

bool test_for_deceleration(Entity *e)
{
    auto manager = e->manager;

    if (manager->buffered_moves.count)
    {
        auto move = manager->buffered_moves[0];
        if (move.id == e->entity_id) return false;
    }

    auto hero = get_active_hero(manager); // @Speed:
    if (hero && (hero->base == e))
    {
        // If we are holding down a movement key, and it
        // goes in the same direction, there you go...

        auto v = &e->visual_interpolation;
        auto dx = v->visual_end - v->visual_start; // :Wrapping

        if      (dx.x > 0) {if (key_right.pressed) return false;}
        else if (dx.x < 0) {if (key_left.pressed)  return false;}
        else if (dx.y > 0) {if (key_up.pressed)    return false;}
        else if (dx.y < 0) {if (key_down.pressed)  return false;}
    }

    return true;
}

f32 get_decel_distance(Guy *guy)
{
    // @Cutnpaste from get_accel_time
    auto decel_distance = gameplay_visuals.move_decel_distance_default;

    // @Incomplete:
    // auto s = &guy->animation_state;
    // auto as = &s->animation_state;
    // auto node = guy->animation_graph.nodes[as->node_index];
    // if (node->move_dcel_distance_override >= 0)
    // {
    //     decel_distance = node->move_decel_distance_override;
    // }

    return decel_distance;
}

f32 get_move_speed_override(Guy *guy)
{
    // @Incomplete:
    // // @Cutnpaste from get_accel_time
    // if (!guy->animation_graph) return -1;

    // auto s = &guy->animation_state;
    // auto as = &s->animation_state;
    // auto node = guy->animation_graph->nodes[as->node_index];
    // if (node->move_speed_override >= 0)
    // {
    //     return node->move_speed_override;
    // }

    return -1;
}

f32 get_run_speed(Guy *guy)
{
    auto run_speed = gameplay_visuals.default_running_speed;
    auto speed_override = get_move_speed_override(guy);
    if (speed_override >= 0) run_speed = speed_override;

    return run_speed;
}

void reset(Visual_Interpolation *v, Source_Location loc)
{
    v->move_type = Move_Type::LINEAR;
    v->linear_move_type = Linear_Move_Type::NONE;

    v->start_time = -1;
    v->duration = 0;
    v->elapsed = 0;

    v->teleport_direction = Vector3(0, 0, 0);
    v->has_teleported = false;
    
    v->velocity = Vector3(0, 0, 0);

    v->sync_id = 0;
    // v->moving_onto_id = 0;

    v->done = false;

    // Don't clear transaction_id, because we chain this between movements.
}

void add_visual_interpolation(Entity *e, Vector3 old_position, Vector3 new_position, bool is_teleport, f32 duration, Transaction_Id transaction_id, Source_Location loc)
{
    Visual_Interpolation v;

    if (is_teleport) v.move_type = Move_Type::TELEPORT;
    else v.move_type = Move_Type::LINEAR;
    
    v.start_time = static_cast<f32>(timez.current_time);
    v.visual_end = new_position;
    // v.location = loc;

    auto visual_start = old_position;
    auto visual_end = new_position;

    auto manager = e->manager;
    // if (manager->wrapping) // If :Wrapping, then visual_start and visual_end will changed
    // {
    //     // Not caring about wrapping at the moment
    // }

    // We need to poke the visual_position now, in case visual_start changed,
    // otherwise our signed distance test will likely fail!
    e->visual_position = visual_start;

    v.visual_start = visual_start;
    v.visual_end = visual_end;

    if (transaction_id) v.transaction_id = transaction_id;
    else v.transaction_id = get_next_transaction_id(e->manager);

    if (duration < 0) v.duration = gameplay_visuals.visual_position_duration;
    else v.duration = duration;

    auto vel = e->visual_interpolation.velocity;
    // auto old_vel = e->visual_interpolation.old_velocity;

    e->visual_interpolation = v;

    e->visual_interpolation.velocity = vel;
    // e.visual_interpolation.old_velocity = old_vel;
}

void update_synced_position(Entity *e)
{
    auto v = &e->visual_interpolation;
    auto other = find_entity(e->manager, v->sync_id);

    if (!other)
    {
        // @Incomplete: Log an error?
        v->sync_id = 0;
        return;
    }

    auto other_v = &other->visual_interpolation;

    auto other_length = glm::length(other_v->visual_end - other_v->visual_start);
    if (other_length)
    {
        auto other_dir = unit_vector(other_v->visual_end - other_v->visual_start);
        auto other_dist = glm::dot(other_dir, other->visual_position - other_v->visual_start);

        if (other_dist >= 0) // Don't let us go backward.
        {
            auto my_dir = unit_vector(v->visual_end - v->visual_start);
            e->visual_position = v->visual_start + my_dir * other_dist;
        }
    }
}

// https://www.youtube.com/watch?v=IHuFcNIFau0&list=PLOuzkDu2bbHOqdj1YrQOrXIDWPn-ELePr&index=234
void update_guy_position(Guy *guy, f32 dt)
{
    auto v = &guy->base->visual_interpolation;
    assert((v->start_time >= 0)); // Callers must make sure that this is true.
    
    // if (v->moving_onto_id)
    // {
    //     auto other = find_entity(guy->base->manager, v->moving_onto_id);
    //     if (other)
    //     {
    //         if (is_flat(other)) // Right now, floors are flat (until we make the editor and such)
    //             v->visual_end = other->visual_position;
    //         else
    //             v->visual_end = other->visual_position + v3(0, 0, 1);
    //     }
    // }

    auto decel_dist = get_decel_distance(guy);
    if (!decel_dist) decel_dist = 1;

    auto visual_range = v->visual_end - v->visual_start; // :Wrapping
    auto dir = unit_vector(visual_range);

    // Ignoring falling
    if (fabs(dir.z) > 0.1f)
    {
        assert(0);
        auto speed = 3.0f;
        auto dz = dir * speed * dt; // Using sign of dir.z here.
        guy->base->visual_position.z += dz.z * speed;

        auto done = false;
        if (v->visual_end.z < v->visual_start.z)
        {
            if (guy->base->visual_position.z <= v->visual_end.z) done = true;
        }
        else
        {
            if (guy->base->visual_position.z >= v->visual_end.z) done = true;
        }

        if (done)
        {
            guy->base->visual_position = v->visual_end;
            v->done = true;
        }

        return;
    }

    auto run_speed = get_run_speed(guy);

    // auto dt_total = dt;
    // while (dt_total > 0)
    // {
    //     auto dt = std::min(0.05f, dt_total); // For fast speeds, e.g. test palyback, or low frame rates, make sure our timestep is small so we don't go too wild.
    //     dt_total -= dt;
    // }

    auto signed_distance = glm::dot(v->visual_end - guy->base->visual_position, dir); // :Wrapping

    auto vel = &v->velocity;
    auto can_decel = test_for_deceleration(guy->base);
    // if (guy->falling) can_decel = false;
    if (guy->base->dead) can_decel = false;

    auto do_decel = false;
    if (can_decel && (fabs(signed_distance <= decel_dist)))
    {
        do_decel = true;

        // if (!guy->animation_state.sent_decel_message)
        // {
        //     // Send go_state_active early so that we start the animation blend.
        //     send_decel_message(guy);
        // }
    }

    auto visual_delta = v->visual_end - guy->base->visual_position; // :Wrapping

    const auto SPEED_SCALE = 1.0f; // @Hardcoded

    f32 dv;
    // auto desired_vel = unit_vector(visual_delta) * run_speed * v->speed_scale;
    auto desired_vel = unit_vector(visual_delta) * run_speed * SPEED_SCALE;
    if (do_decel)
    {
        const f32 SLOWDOWN_DISTANCE = 0.2;

        // dv = 160.0f * v->speed_scale * dt;
        dv = 160.0f * SPEED_SCALE * dt;

        auto slowdown_factor = glm::length(visual_delta) / SLOWDOWN_DISTANCE;
        Clamp(&slowdown_factor, 0.0f, 1.0f);

        desired_vel *= slowdown_factor;
    }
    else
    {
        auto accel_time = get_accel_time(guy);
        if (!accel_time) accel_time = 1.0f;

        dv = 2 * (run_speed / accel_time) * dt;
    }

    // Linearly interpolating in each coordinate is not strictly
    // correct since we move faster across diagonals, but we don't
    // in general move across diagonals in this game, so I am 
    // presuming it will be fine.

    vel->x = move_toward(vel->x, desired_vel.x, dv);
    vel->y = move_toward(vel->y, desired_vel.y, dv);

    auto dx = (*vel) * dt;
    dx.z = 0; // Esnure no z velocity snuck in.

    guy->base->visual_position += dx;

    assert((v->move_type == Move_Type::LINEAR));

    auto new_signed_distance = glm::dot(v->visual_end - guy->base->visual_position, dir); // :Wrapping
    const f32 EPSILON = 0.001f;
    if (new_signed_distance <= EPSILON) v->done = true;
    if (new_signed_distance <= 0)
    {
        // @Redundant with remaining_distance_now clause..
        guy->base->visual_position = v->visual_end; // Prevent stuff from going past squares if the frame rate is low or unpredictable.
    }

    auto remaining_distance_now = glm::length(guy->base->visual_position - v->visual_end); // :Wrapping
    if (remaining_distance_now < gameplay_visuals.distance_epsilon)
    {
        guy->base->visual_position = v->visual_end;
        v->done = true;
    }
}

void update_visual_position(Entity *e, f32 dt)
{
    auto v = &e->visual_interpolation;
    if (v->start_time < 0) return;

    auto old_t = v->elapsed / v->duration;
    if (cmp_var_type_to_type(e->type, Guy))
    {
        const auto PLAYER_SPEED = 1.0f; // 1 block for second @Hardcoded
        // dt *= e->manager->player_speed; // Move faster if stuff is buffered up!
        dt *= PLAYER_SPEED; // Move at a constant
    }

    v->elapsed += dt;

    if (v->move_type == Move_Type::TELEPORT)
    {
        auto t = v->elapsed / v->duration;
        Clamp(&t, 0.0f, 1.0f);

        // if (v->elapsed >= v->teleport_pre_time)
        {
            if (!v->has_teleported)
            {
                e->visual_interpolation.visual_start = v->visual_end;
                v->has_teleported = true;
                return;
            }

            // if (v->elapsed >= v->teleport_post_time)
            {
                v->done = true;
                return;
            }

            return;
        }

        // Linear-interpolate at the start of the teleport.
        // Note that in the new system this may actually trigger a physical move
        // in rare cases!
        // Maybe we don't want that, and would rather just do an effect that doesn't involve
        // moving the visual position!
        if (glm::length(v->teleport_direction))
        {
            e->visual_position = v->visual_start + t * 0.4f * v->teleport_direction;
        }
        else
        {
            // With no teleport_direction, we just interpolate between the endpoints.
            e->visual_position = lerp(v->visual_start, v->visual_end, t * 0.2f);
        }

        return;
    }

    if (cmp_var_type_to_type(e->type, Guy))
    {
        update_guy_position(Down<Guy>(e), dt);
    }
    else
    {
        auto t = v->elapsed / v->duration;
        Clamp(&t, 0.0f, 1.0f);

        e->visual_position = lerp(v->visual_start, v->visual_end, t);

        if (v->elapsed >= v->duration)
        {
            v->done = true;
        }
    }
}

void update_entities_visual_interpolation(Entity_Manager *manager, f32 dt)
{
    // First do the non-synced guys, then the synced guys if they need to move
    RArr<Entity*> interps_completed;
    interps_completed.allocator = {global_context.temporary_storage, __temporary_allocator};

    RArr<Entity*> considered;
    considered.allocator = {global_context.temporary_storage, __temporary_allocator};

    for (auto it : manager->moving_entities)
    {
        auto v = &it->visual_interpolation;
        if (v->start_time < 0) continue;

        array_add(&considered, it);
    }

    // Also, validate any sync_ids. If they are synced to someone who is
    // on a different transaction now, kill the sync!
    for (auto it : considered)
    {
        auto v = &it->visual_interpolation;
        if (!v->sync_id) continue;

        auto other = find_entity(it->manager, v->sync_id);
        if (!other ||
            (other->visual_interpolation.transaction_id != v->transaction_id) ||
            (other->visual_interpolation.start_time < 0))
        {
            v->sync_id = 0; // @Redundant with update_synced_position, but hey, this is just a temporary.

            auto t = 0.0f;
            auto denom = glm::length(v->visual_start - v->visual_end);
            if (denom) t = glm::distance(v->visual_start, it->visual_position) / denom;

            Clamp(&t, 0.0f, 1.0f);

            v->elapsed = v->duration * t;
            continue;
        }
    }

    //
    // First we do things without sync_id, then we do things with sync_id,
    // to ensure that we dont' use the previous frame's dat for sync.
    // Maybe we should have put them into two separate arrays!
    //

    for (auto it : considered)
    {
        auto v = &it->visual_interpolation;
        if (v->sync_id) continue;

        if (v->start_time < 0) continue; // Should we even check this?

        auto old_v = it->visual_interpolation;
        update_visual_position(it, dt);
        // has_moved(it, old_v, it->orientation, it->scale);
        // auto should_physically_move = needs_physical_move(it);

        if (v->done)
        {
            for (auto it2 : manager->moving_entities)
            {
                if ((it2->visual_interpolation.sync_id == it->entity_id) &&
                    (it2->visual_interpolation.start_time >= 0))
                {
                    array_add_if_unique(&interps_completed, it2);
                }
            }

            array_add(&interps_completed, it);
            v->start_time = -1;
        }
    }

    for (auto it : considered)
    {
        auto v = &it->visual_interpolation;
        if (v->sync_id)
        {
            auto v = &it->visual_interpolation;
            if (v->sync_id)
            {
                auto old_v = it->visual_interpolation;
                update_synced_position(it);
                // has_moved(it, old_v, it->orientation, it->scale);
                continue;
            }
        }
    }

    auto should_do_player_move = maybe_retire_next_transaction(manager);
    if (should_do_player_move)
    {
        enact_next_buffered_move(manager);
    }

    // for (auto e : interps_completed)
    // {
    //     // If this entity restarted a new animation due to enact_next_buffered_move,
    //     // don't animate him to neutral.
    //     if (e->visual_interpolation.start_time >= 0)
    //     {
    //         e->visual_interpolation.velocity = e->visual_interpolation.old_velocity;
    //         continue;
    //     }

    //     if (cmp_var_type_to_type(e->type, Guy))
    //     {
    //         auto guy = Down<Guy>(e);
    //         if (guy->base->dead) continue;

    //         // guy->animation_state.sent_going_far = false;
    //         e->visual_interpolation.velocity = Vector3(0, 0, 0);

    //         // // If we are ending a known move type, issue a known
    //         // // handler message. Otherwise, go to StateDefault.
    //         // if (guy->active) send_message(guy, "go_state_active", "StateDefault");
    //         // else send_message(guy, "go_state_inactive", "StateDefault");
    //     }
    // }
}


/*


    if (interps_completed)
    {
        // If there are clones, and they are basically almost done,
        // just complete them as well, so that we all stay in sync and don't break
        // animations. (This sounds like a @Hack tho).

        for (auto it : interps_completed)
        {
            it->visual_interpolation.old_velocity = it->visual_interpolation.velocity;

            if (!cmp_var_type_to_type(it->type, Guy)) continue;

            for (auto other_guy : manager->_by_guys)
            {
                auto other_e = other_guy->base;
                if (other_e == it) continue;
                if (array_find(interps_completed, other_e)) continue;

                if (other_e->visual_interpolation.start_time < 0) continue;
                if (other_e->visual_interpolation.transaction_id != it->visual_interpolation.transaction_id) continue;
                if (other_e->visual_interpolation.move_type != it->visual_interpolation.move_type) continue;

                array_add(&interps_completed, other_e);
            }
        }

        // For anyone who is synced, if we stopped the guy, we stop the thing too.
        for (auto it : considered)
        {
            if (!it->visual_interpolation.sync_id) continue;
            if (array_find(&interps_completed, it)) continue;

            for (auto comp : interps_completed)
            {
                if ((comp->entity_id == it->visual_interpolation->sync_id) &&
                    (comp->visual_interpolation.transaction_id == it->visual_interpolation.transaction_id))
                {
                    array_add(&interps_completed, it);
                    break;
                }
            }
        }
    }


void add_visual_interpolation(Entity *e, Vector3 old_position, Vector3 new_position, Move_Type move_type = Move_Type::LINEAR, f32 duration = -1.0f, Transaction_Id transaction_id = 0, Source_Location loc = Source_Location::current())
{
}

void update_entity_visual_interpolation(Entity_Manager *manager, f32 dt)
{
    RArr<Entity*> interps_completed;
    interps_completed.allocator = {global_context.temporary_storage, __temporary_allocator};

    RArr<Entity*> considered;
    considered.allocator = {global_context.temporary_storage, __temporary_allocator};

    for (auto it : manager->moving_entities)
    {
        auto v = &it->visual_interpolation;
        if (v->start_time < 0) continue;

        array_add(&considered, it);
    }

    // Valid any sync_ids. If they are synced to someone who is on
    // a different transaction now, kill the sync!
    for (auto it : considered)
    {
        auto v = &it->visual_interpolation;

        if (!v->sync_id) continue;

        auto other = find_entity(it->manager, v->sync_id);
        if (!other || (other->visual_interpolation.transaction_id != v->transaction_id) ||
            (other->visual_interpolation.start_time < 0))
        {
            v->sync_id = 0;

            // @Incomplete:
        }
    }
}

// update_synced_position(Entity *e)


void be_stationary(Entity *e, Move_Type move_type = Move_Type::STATIONARY, f32 duration = -1.0, Transaction_Id transaction_id = 0, Source_Location loc = Source_Location::current())
{
    add_visual_interpolation(e->visual_interpolation, e->visual_position, move_type, duration, transaction_id, loc);
}
*/
