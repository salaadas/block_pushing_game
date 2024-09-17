// @Leak @Leak @Leak

#include "entity_manager.h"

#include "file_utils.h"
#include "undo.h"
#include "main.h"
#include "animation_player.h"

void init_proximity_grid(Proximity_Grid *grid)
{
    init_gridlike(&grid->gridlike);
    init(&grid->entities_snapped_to_grid);
}

constexpr Vector3 DEFAULT_CAMERA_POSITION = Vector3(0, -7, 22);

void init_default_camera(Camera *camera)
{
    camera->position = DEFAULT_CAMERA_POSITION;

    // Will be set by the user
    camera->forward = Vector3(0, 0, 0);

    camera->phi   = -M_PI / 5.0f;
    camera->theta =  M_PI / 2.0f;
    camera->orientation.w = 1;
    //    // get_ori_from_rot(&camera->orientation, Vector3(1, 0, 0), camera->phi);
    // get_orientation_from_angles(camera->theta, camera->phi);

    camera->fov_vertical = 35.0f;
    camera->z_near       = 0.2f;
    camera->z_far        = 200.0f;

    refresh_camera_matrices(camera);
}

Entity_Manager *make_entity_manager()
{
    auto manager = New<Entity_Manager>(false);
    bucket_array_init(&manager->entity_array);
    array_init(&manager->entities_to_clean_up);

    array_init(&manager->all_entities);
    array_init(&manager->moving_entities);

    array_init(&manager->_by_guys);
    array_init(&manager->_by_doors);
    array_init(&manager->_by_gates);
    array_init(&manager->_by_switches);
    array_init(&manager->_by_rocks);
    array_init(&manager->_by_walls);
    array_init(&manager->_by_floors);

    manager->proximity_grid = New<Proximity_Grid>(false);
    init_proximity_grid(manager->proximity_grid);

    init_default_camera(&manager->camera);

    manager->next_entity_id = 1; // Starts with ID of 1, in order to nullify or invalidate an id with 0
    manager->active_hero_index = 0;

    manager->next_transaction_id_to_issue = 1;
    manager->next_transaction_id_to_retire = 1;

    array_init(&manager->turn_order);

    array_init(&manager->buffered_moves);

    auto undo_handler = New<Undo_Handler>(false);
    init_undo_handler(undo_handler);
    manager->undo_handler = undo_handler;

    return manager;
}

inline
void init_general_entity(Entity *e)
{
    // e->position        = Vector3(0, 0, 0);
    // e->visual_position = Vector3(0, 0, 0);

    e->scale           = 1.0f;
    e->orientation     = Quaternion(1, 0, 0, 0);

    // No need to set these, we just do them for clarity.
    e->theta_current   = 0.0f;
    e->theta_target    = 0.0f;

    e->boundary_radius = 0.5f; // Default for a cube block
    e->boundary_center = Vector3(0.5, 0.5, 0.5);
    e->manager         = NULL;
    e->locator         = {};
    // e->entity_id       = 0;
    e->derived_pointer = NULL;

    e->use_override_color = false;
    e->dead = false;

    e->mesh = NULL;

    e->scheduled_for_destruction = false;

    reset(&e->visual_interpolation);
}

inline
void init_guy(Guy *guy)
{
    // Make sure to init the entity base class first!
    guy->can_push = false;
    guy->can_pull = false;
    guy->can_teleport = false;

    guy->active = false;
    guy->facing_direction = Direction::WEST;

    guy->turn_order_index = -1;
}

inline
void register_entity(Entity_Manager *manager, Entity *e)
{
    e->entity_id = manager->next_entity_id;

    e->manager = manager;

    manager->next_entity_id += 1;

    array_add(&manager->all_entities, e);

#define add_entities_to_their_lists(entity_type, array_name)                   \
    if (cmp_var_type_to_type(e->type, entity_type)) array_add(array_name, Down<entity_type>(e));

    // Add the entity to its corresponding arrays
    add_entities_to_their_lists(Guy,    &manager->_by_guys);
    add_entities_to_their_lists(Door,   &manager->_by_doors);
    add_entities_to_their_lists(Gate,   &manager->_by_gates);
    add_entities_to_their_lists(Switch, &manager->_by_switches);
    add_entities_to_their_lists(Rock,   &manager->_by_rocks);
    add_entities_to_their_lists(Wall,   &manager->_by_walls);
    add_entities_to_their_lists(Floor,  &manager->_by_floors);

#undef add_entities_to_their_lists

#define add_moving_entities(entity_type, array_name)                   \
    if (cmp_var_type_to_type(e->type, entity_type)) array_add(array_name, e);

    auto moving_list = &manager->moving_entities;
    add_moving_entities(Guy,    moving_list);
    add_moving_entities(Rock,   moving_list);
    add_moving_entities(Gate,   moving_list);

#undef add_moving_entities
}

// @Temporary: Remove texture_2d_name later, since only door needs it
// When we have triangle mesh for the entities, they will use the texture
// that lives inside the triangle list info instead of relying on this.
template <class Entity_Type>
Entity_Type *Make(Entity_Manager *manager, i32 x, i32 y, String texture_2d_name)
{
    auto result = New<Entity_Type>(false);

    auto ret = find_and_occupy_empty_slot(&manager->entity_array);
    init_general_entity(ret.first);

    result->base = ret.first;
    result->base->locator = ret.second;

    result->base->derived_pointer = result;
    _set_type_Type(result->base->type, Entity_Type);

    result->base->position.x = static_cast<f32>(x);
    result->base->position.y = static_cast<f32>(y);
    result->base->position.z = 0;

    result->base->visual_position = result->base->position;

    result->base->map = catalog_find(&texture_catalog, texture_2d_name);

    register_entity(manager, result->base);

    return result;
}

// @Cutnpaste from Make() @Cleanup
template <class Entity_Type>
Entity_Type *Make3D(Entity_Manager *manager, i32 x, i32 y, String mesh_name)
{
    auto result = New<Entity_Type>(false);

    auto ret = find_and_occupy_empty_slot(&manager->entity_array);
    init_general_entity(ret.first);

    result->base = ret.first;
    result->base->locator = ret.second;

    result->base->derived_pointer = result;
    _set_type_Type(result->base->type, Entity_Type);

    result->base->position.x = static_cast<f32>(x);
    result->base->position.y = static_cast<f32>(y);
    result->base->position.z = 0;

    result->base->visual_position = result->base->position;

    result->base->map = NULL;
    result->base->mesh = catalog_find(&mesh_catalog, mesh_name);
    assert(result->base->mesh);

    register_entity(manager, result->base);

    return result;
}

void allocate_data_for_grid(Proximity_Grid *grid, i32 num_x, i32 num_y)
{
    auto gridlike = &grid->gridlike;
    gridlike->squares_x = num_x;
    gridlike->squares_y = num_y;

    auto num_squares = num_x * num_y;
    gridlike->num_squares = num_squares;
}

Switch *make_switch(Entity_Manager *manager, i32 x, i32 y, String texture_name, i32 flavor)
{
    auto swit = Make<Switch>(manager, x, y, texture_name);
    swit->flavor = flavor;
    swit->pressed = false;

    swit->base->use_override_color = true;

    Vector4 color;
    // @Hardcoded color for the swit:
    if (flavor == 1) color = Vector4(1, 1, 0, 1);
    else if (flavor == 2) color = Vector4(1, 0, 1, 1);
    else color = Vector4(1, 0, 0, 1);

    swit->base->override_color = color;

    return swit;
}

Gate *make_gate(Entity_Manager *manager, i32 x, i32 y, String texture_name, i32 flavor)
{
    auto gate = Make<Gate>(manager, x, y, texture_name);
    gate->flavor = flavor;
    gate->open = false;

    gate->base->use_override_color = true;

    Vector4 color;
    // @Hardcoded color for the gate:
    if (flavor == 1) color = Vector4(1, 1, 0, 1);
    else if (flavor == 2) color = Vector4(1, 0, 1, 1);
    else color = Vector4(1, 0, 0, 1);

    gate->base->override_color = color;

    return gate;
}

#define error(c_agent, handler, ...)                                  \
    printf("[%s] Error at line %d: ", c_agent, handler.line_number);  \
    printf(__VA_ARGS__);                                              

void load_ascii_level(Entity_Manager *manager, String full_path)
{
    Text_File_Handler handler;
    String agent("load_ascii_level");

    start_file(&handler, full_path, agent);
    if (handler.failed) return;

    auto c_agent = reinterpret_cast<char*>(temp_c_string(agent));

    auto grid = manager->proximity_grid;

    i32 num_x = 0;
    i32 num_y = 0;
    i32 processed_rows = 0;
    bool allocated_data_for_grid = false;

    Vector3 camera_pos = DEFAULT_CAMERA_POSITION;

    while (true)
    {
        auto [line, found] = consume_next_line(&handler);

        if (!found) break;

        if (line[0] == ':')
        {
            advance(&line, 1);
            eat_spaces(&line);

            auto [command, remainder] = break_by_spaces(line);

            if (command == String("width"))
            {
                if (num_x)
                {
                    error(c_agent, handler, "Redefinition of number of squares in x for level '%s', skipping...\n",
                          temp_c_string(full_path));
                    continue;
                }

                bool success = false;
                auto ret = string_to_int(remainder, &success);

                if (!success)
                {
                    error(c_agent, handler, "Unable to number of squares in x for level '%s'.\n",
                          temp_c_string(full_path));
                    continue;
                }

                num_x = ret.first;

                if (ret.second)
                {
                    error(c_agent, handler, "Junk at the end of line '%s'.\n", temp_c_string(ret.second));
                }
            }
            else if (command == String("height"))
            {
                if (num_y)
                {
                    error(c_agent, handler, "Redefinition of number of squares in x for level '%s', skipping...\n",
                          temp_c_string(full_path));
                    continue;
                }

                bool success = false;
                auto ret = string_to_int(remainder, &success);

                if (!success)
                {
                    error(c_agent, handler, "Unable to read number of squares in y for level '%s'.\n",
                          temp_c_string(full_path));
                    continue;
                }

                num_y = ret.first;

                if (ret.second)
                {
                    error(c_agent, handler, "Junk at the end of line '%s'.\n", temp_c_string(ret.second));
                }
            }
            else if (command == String("map"))
            {
                if (remainder)
                {
                    error(c_agent, handler, "Junk at the end of line '%s'.\n", temp_c_string(remainder));
                }

                if ((num_x <= 0) || (num_y <= 0))
                {
                    error(c_agent, handler, "Cannot allocate data for grid when size of it is (%d, %d).\n",
                          num_x, num_y);
                    continue;
                }

                allocate_data_for_grid(grid, num_x, num_y);
                allocated_data_for_grid = true;
            }
            else if (command == String("camera"))
            {
                while (remainder)
                {
                    auto [command, args] = break_by_spaces(remainder);

                    if (command == String("south"))
                    {
                        bool success = false;
                        auto [distance, rhs] = string_to_float(args, &success);

                        if (!success)
                        {
                            error(c_agent, handler, "Unable to parse the south distance of the camera.\n");
                            break;
                        }

                        // The negative sign here means that we are switching to a y-forward axis,
                        // because south in our engine means negative in y.
                        camera_pos.y = -distance;

                        args = rhs;
                    }
                    else if (command == String("up"))
                    {
                        bool success = false;
                        auto [distance, rhs] = string_to_float(args, &success);

                        if (!success)
                        {
                            error(c_agent, handler, "Unable to parse the up distance of the camera.\n");
                            break;
                        }

                        camera_pos.z = distance;

                        args = rhs;
                    }
                    else
                    {
                        error(c_agent, handler, "Unknown camera property '%s'.\n", temp_c_string(command));
                        break;
                    }

                    remainder = args;
                    eat_spaces(&remainder);
                }
            }
            else
            {
                printf("************************************ COMMAND: '%s', REMAINDER: '%s'\n",
                       temp_c_string(command), temp_c_string(remainder));
            }
        }
        else
        {
            assert(line.count > 0);

            if (!allocated_data_for_grid)
            {
                error(c_agent, handler, "Must call command 'map' before specifying the level map.\n");
                continue;
            }

            if (num_x != line.count)
            {
                error(c_agent, handler, "Mismatch dimensions in squares_x! Gridlike's squares_x = %d while line count is %ld.\n", num_x, line.count);
            }

            if (processed_rows >= num_y)
            {
                error(c_agent, handler, "The number of rows (%d) is more than squares_y (%d) in proximity grid, skipping!!!!\n", processed_rows, num_y);
                continue;
            }

            i32 column_index = 0;

            //
            // For now, some tile does not 
            for (auto c : line)
            {
                i32 x = column_index;
                i32 y = num_y - processed_rows - 1;

                Entity *e = NULL;
                Entity *e2 = NULL;

                if (c == '*')
                {
                    e  = Make<Wall>(manager, x, y, String("chisled_stone"))->base;
                    e2 = Make<Floor>(manager, x, y, String("quartz"))->base;
                }
                else if (c == '%')
                {
                    e  = Make<Rock>(manager, x, y, String("wood"))->base;
                    e2 = Make<Floor>(manager, x, y, String("quartz"))->base;
                }
                else if (c == '{')
                {
                    auto swit = make_switch(manager, x, y, String("candy"), 1);

                    e = swit->base;
                    e2 = Make<Floor>(manager, x, y, String("quartz"))->base;
                }
                else if (c == '}')
                {
                    auto swit = make_switch(manager, x, y, String("candy"), 2);

                    e = swit->base;
                    e2 = Make<Floor>(manager, x, y, String("quartz"))->base;
                }
                else if (c == '[')
                {
                    auto gate = make_gate(manager, x, y, String("candy"), 1);

                    e = gate->base;
                    e2 = Make<Floor>(manager, x, y, String("quartz"))->base;
                }
                else if (c == ']')
                {
                    auto gate = make_gate(manager, x, y, String("candy"), 2);

                    e = gate->base;
                    e2 = Make<Floor>(manager, x, y, String("quartz"))->base;
                }
                else if (c == '.')
                {
                    // This is the floor.
                    e2 = Make<Floor>(manager, x, y, String("quartz"))->base;
                }
                else if (c == '$')
                {
                    auto door = Make<Door>(manager, x, y, String("door"));
                    door->open = true;

                    e  = door->base;
                    e2 = Make<Floor>(manager, x, y, String("quartz"))->base;
                }
                // @Cleanup: once we start doing meshes, consolidate the
                // making of the guys.
                else if (c == 'F' || c == '1')
                {
                    // @Cleanup: Create a Make_guy3D procedure.
                    auto guy = Make3D<Guy>(manager, x, y, String("only-red")); // @Hardcoded: @Temporary:
                    init_guy(guy);
                    guy->can_push = true;

                    // @Incomplete: Handle animation playing for the guys...
                    guy->base->animation_player = New<Animation_Player>(); // @Leak:
                    init_player(guy->base->animation_player);
                    // set_mesh(guy->base->mesh);

                    bool active = false;
                    if (c == 'F') active = true;
                    guy->active = active;

                    // On loading level, we make the clone id of everyone themselves,
                    // However, we will correct these id when we make the turn order.
                    guy->clone_of_id = guy->base->entity_id;

                    e  = guy->base;
                    e2 = Make<Floor>(manager, x, y, String("quartz"))->base;
                }
                else if (c == 'T' || c == '2')
                {
                    auto guy = Make<Guy>(manager, x, y, String("thief"));
                    init_guy(guy);
                    guy->can_pull = true;

                    bool active = false;
                    if (c == 'T') active = true;
                    guy->active = active;

                    // On loading level, we make the clone id of everyone themselves,
                    // However, we will correct these id when we make the turn order.
                    guy->clone_of_id = guy->base->entity_id;

                    e  = guy->base;
                    e2 = Make<Floor>(manager, x, y, String("quartz"))->base;
                }
                else if (c == 'W' || c == '3')
                {
                    auto guy = Make<Guy>(manager, x, y, String("wizard"));
                    init_guy(guy);
                    guy->can_teleport = true;

                    bool active = false;
                    if (c == 'W') active = true;
                    guy->active = active;

                    // On loading level, we make the clone id of everyone themselves,
                    // However, we will correct these id when we make the turn order.
                    guy->clone_of_id = guy->base->entity_id;

                    e  = guy->base;
                    e2 = Make<Floor>(manager, x, y, String("quartz"))->base;
                }
                else
                {
                    error(c_agent, handler, "Unknown ascii entity '%c', making it a wall!\n", c);
                    e = Make<Wall>(manager, x, y, String("purple"))->base;
                }

                if (e)
                {
                    add_to_grid(grid, e);
                }

                column_index += 1;
            }

            processed_rows += 1;
        }
    }

    deinit(&handler);

    // printf("sx %d, sy %d\n", num_x, num_y);
    // printf("floors   count: %ld\n", manager->_by_floors.count);
    // printf("walls    count: %ld\n", manager->_by_walls.count);
    // printf("doors    count: %ld\n", manager->_by_doors.count);
    // printf("guys     count: %ld\n", manager->_by_guys.count);
    // printf("gates    count: %ld\n", manager->_by_gates.count);
    // printf("switches count: %ld\n", manager->_by_switches.count);
    // printf("rocks    count: %ld\n", manager->_by_rocks.count);

    // Making the turn order for the manager
    {
        array_reserve(&manager->turn_order, manager->_by_guys.count);

        i32 turn_index = 0;
        Guy *first_active_guy = NULL;
        // @Fixme: I think all guys should be inside the turn order, however,
        // for clones, we can use turn_order_id to really get which turn we are in.
        for (auto guy : manager->_by_guys)
        {
            // If we have multiple active dudes in the
            // level, we make all of the dudes be clones
            // of the first active dude.
            if (first_active_guy && guy->active)
            {
                Pid id_of_host = first_active_guy->base->entity_id;

                // Correct the id of the parasite.
                guy->clone_of_id = id_of_host;

                // We don't add them to the turn order because
                // they are controlled by their host.
                continue;
            }

            Pid id = guy->base->entity_id;
            array_add(&manager->turn_order, id);

            guy->turn_order_index = turn_index;

            // Only the first active dude will get to here, because the rest
            // of the active dudes are handled in the above if clause.
            if (guy->active)
            {
                manager->active_hero_index = turn_index;
                first_active_guy = guy;
            }

            turn_index += 1;
        }
    }

    // @Hardcoded @Temporary: Fix the camera control so that each level contains a different camera position and such.
    {
        auto camera = &manager->camera;

        // The north distance of the camera, in the *.ascii_level file, we choose to specify
        // the south distance, in part because it is more common.
        camera->position.y = camera_pos.y;
        // The up distance of the camera (how high is the camera compared to the map).
        camera->position.z = camera_pos.z;

        // @Hack: @Hardcoded: Generally, we want our camera to be at the center of the map width,
        // but we may want to customize this later.
        camera->position.x = (num_x - 1) * 0.5;
        camera->theta = M_PI / 2.0f;

        Vector3 point_of_interest;
        point_of_interest.x = manager->camera.position.x;
        point_of_interest.y = (num_y - 1) * 0.5;
        point_of_interest.z = 0;

        camera->fov_vertical = 40; // @Hardcoded:
        camera->forward = point_of_interest - camera->position;

        // @Cleanup: Is there a faster way to do this?
        auto dot = glm::dot(camera->forward, Vector3(0, 1, 0));
        auto len = glm::length(camera->forward);
        camera->phi = -acos(dot / len);

        refresh_camera_matrices(camera);
    }
}

Entity *find_entity(Entity_Manager *manager, Pid id)
{
    for (auto e : manager->all_entities)
    {
        if (e->entity_id == id)
        {
            return e;
        }
    }

    return NULL;
}

// @Cutnpaste from table_find_multiple
void find_at_position(Proximity_Grid *grid, Vector3 pos, RArr<Entity*> *results)
{
    auto position_index = position_to_index(grid->gridlike, pos);
    auto table = &grid->entities_snapped_to_grid;

    if (!table->allocated) return;
    if (!results->allocator) results->allocator = table->allocator;

    // walk code
    auto mask           = (u32)(table->allocated - 1);
    auto hash           = table->hash_function(position_index);
    if (hash < FIRST_VALID_HASH) hash += FIRST_VALID_HASH;
    auto index          = hash & mask;
    u32 probe_increment = 1;

    while (table->entries[index].hash)
    {
        auto entry = &table->entries[index];
        if (entry->hash == hash)
        {
            if (table->cmp_function(entry->key, position_index))
            {
                array_add(results, entry->value);
            }
        }

        index = (index + probe_increment) & mask;
        probe_increment += 1;
    }
}

void release_entity(Entity *e)
{
    assert((e->manager != NULL));
    bucket_array_remove(&e->manager->entity_array, e->locator);
}

// @Note: Should we really add only one main layer to the proximity grid?
// To me, this sounds like a reasonable thing to do for a while because all
// the game logic happens within one layer of the game.
void add_to_grid(Proximity_Grid *grid, Entity *e)
{
    auto index = position_to_index(grid->gridlike, e->position);

    if (index == -1)
    {
        logprint("add_to_grid", "Entity's position (%g, %g, %g) is out of bounds for the grid (%ld, %ld)!\n",
                 e->position.x, e->position.y, e->position.z,
                 grid->gridlike.squares_x, grid->gridlike.squares_y);
        return;
    }

    auto data = &grid->entities_snapped_to_grid;
    table_add(data, index, e);
}

// @Cutnpaste from table_remove
my_pair<bool, Entity*> remove_from_grid(Proximity_Grid *grid, Entity *e)
{
    auto entity_index = position_to_index(grid->gridlike, e->position);
    auto id = e->entity_id;

    auto table = &grid->entities_snapped_to_grid;

    if (!table->allocated)
    {
        return {false, NULL};
    }

    // walk code
    auto mask           = (u32)(table->allocated - 1);
    auto hash           = table->hash_function(entity_index);
    if (hash < FIRST_VALID_HASH) hash += FIRST_VALID_HASH;
    auto index          = hash & mask; // same as hash % table->allocated [less clock-cycle]
    u32 probe_increment = 1;

    while (table->entries[index].hash)
    {
        auto entry = &table->entries[index];

        if ((entry->hash == hash) && table->cmp_function(entry->key, entity_index))
        {
            auto entry_value = entry->value;
            if (entry_value && entry_value->entity_id == id)
            {
                entry->hash = REMOVED_HASH;
                table->count -= 1;
                return {true, entry_value};
            }
        }

        index = (index + probe_increment) & mask;
        probe_increment += 1;
    }

    return {false, NULL};
}

void post_frame_cleanup(Entity_Manager *manager)
{
    // for (auto e : manager->entities_to_clean_up)
    // {
    //     my_free(e->derived_pointer); // This is not calling the entity's destructor, so its leaking
    //     release_entity(e);
    // }
    // array_reset(&manager->entities_to_clean_up);

    auto grid = manager->proximity_grid;
    for (auto e : manager->entities_to_clean_up)
    {
        auto [success, removed_entity] = remove_from_grid(grid, e);
        if (!success)
        {
            printf("[post_frame_cleanup]: Was not able to clean up entity with id %d\n", e->entity_id);
        }
    }

    array_reset(&manager->entities_to_clean_up);
}

void reset_entity_manager(Entity_Manager *manager)
{
    auto proximity_grid = manager->proximity_grid;
    table_reset(&proximity_grid->entities_snapped_to_grid);

    bucket_array_reset(&manager->entity_array);

    array_reset(&manager->entities_to_clean_up);

    array_reset(&manager->all_entities);
    array_reset(&manager->moving_entities);

    array_reset(&manager->_by_guys);
    array_reset(&manager->_by_doors);
    array_reset(&manager->_by_gates);
    array_reset(&manager->_by_switches);
    array_reset(&manager->_by_rocks);
    array_reset(&manager->_by_walls);
    array_reset(&manager->_by_floors);

    array_reset(&manager->turn_order);

    array_reset(&manager->buffered_moves);

    manager->next_entity_id = 0;
    manager->active_hero_index = 0;

    manager->next_transaction_id_to_issue  = 1;
    manager->next_transaction_id_to_retire = 1;
    manager->waiting_on_player_transaction = 0;
    manager->player_transaction_caused_changes = false;
}
