#pragma once

#include "common.h"

#include "gridlike.h"
#include "bucket_array.h"
#include "camera.h"
#include "texture.h"
#include "table.h"
#include "movement_visual.h"

#include "visit_struct.h"

typedef u32 Pid;
typedef u32 Transaction_Id;

// Eventually, we would want undo/activate and all of that stuff to be inside buffered move.
struct Buffered_Move
{
    enum Action_Type : u8
    {
        DELTA    = 0,
        UNDO     = 1,
        // ACTIVATE = 2,
        // MAGIC    = 3
    };

    Action_Type action_type;
    Pid     id;
    Vector3 delta;
    bool    is_repeat = false;
};

struct Triangle_Mesh;
struct Entity_Manager;

// enum Move_Type
// {
//     LINEAR_MOVE = 0,
//     TELEPORT    = 1,
//     STATIONARY  = 2,
//     STATIONARY_WITH_CHANGE  = 3,
//     TELEPORT_THEN_DISAPPEAR = 4,
// };

struct Animation_Player;

struct Entity
{
    // String     entity_name;

    Vector3    position;

    f32        scale; // 1.0
    Quaternion orientation;

    // These are in degrees
    f32        theta_current;
    f32        theta_target;

    f32        boundary_radius;
    Vector3    boundary_center;

    Triangle_Mesh *mesh;
    // f32 mesh_scale;
    // Vector3 mesh_offset;

    Entity_Manager *manager; // @NoSerialize
    Bucket_Locator  locator; // @NoSerialize

    Pid             entity_id;
    _type_Type      type = _make_Type(Entity);
    void           *derived_pointer;

    Texture_Map    *map; // @NoSerialize        // @Fixme: I think this should go away in the future because we will make everyone a mesh.
    // Texture_Map    *lightmap_texture; // @NoSerialize

    bool use_override_color; // false
    Vector4 override_color;

    bool dead; // false

    bool scheduled_for_destruction; // @NoSerialize

    Visual_Interpolation visual_interpolation;
    Vector3 visual_position;

    // SArr<Material*> materials;

    Animation_Player *animation_player = NULL;

    // @Important: we need to use this to do get_clones_of()
    // Pid turn_order_id;
};

VISITABLE_STRUCT(Entity,
                 // @Note: Not using entity name right now.
                 // entity_name,
                 position, scale, orientation,
                 // entity_flags,
                 // group_id,
                 // mount_parent_id,
                 theta_current, theta_target,
                 boundary_radius, boundary_center,
                 use_override_color, override_color,
                 dead,
                 visual_interpolation, visual_position);
                 // turn_order_id);

enum class Direction : u32
{
    EAST  = 0,
    NORTH = 1,
    WEST  = 2,
    SOUTH = 3
};

struct Guy
{
    Entity *base;

    bool can_push;
    bool can_pull;
    bool can_teleport;

    bool active; // false

    Direction facing_direction;

    Pid clone_of_id;

    // String death_sound; //  = "";
    // String move_sound; // = "";

    i32 turn_order_index; // -1
};

VISITABLE_STRUCT(Guy,
                 base,
                 can_push, can_pull, can_teleport,
                 active,
                 facing_direction,
                 clone_of_id,
                 turn_order_index);

struct Door
{
    Entity *base;

    bool open; // false
    bool already_did_switch; // false
};

VISITABLE_STRUCT(Door,
                 base,
                 open,
                 already_did_switch);

struct Switch;
struct Gate
{
    Entity *base;

    i32 flavor;
    bool open; // false
//     bool is_monster_gate; // false
};

VISITABLE_STRUCT(Gate,
                 base,
                 flavor,
                 open);

struct Switch
{
    Entity *base;

    i32 flavor;
    bool pressed; // false
};

VISITABLE_STRUCT(Switch,
                 base,
                 flavor,
                 pressed);

struct Rock
{
    Entity *base;
};

VISITABLE_STRUCT(Rock,
                 base);

struct Floor
{
    Entity *base;
};

VISITABLE_STRUCT(Floor,
                 base);

struct Wall
{
    Entity *base;
};

VISITABLE_STRUCT(Wall,
                 base);

struct Unknown_Entity
{
    Entity *base;
};

struct Proximity_Grid
{
    Gridlike gridlike;

    // // @Cleanup
    // // Array of pointers to entities, it has the size of gridlike.num_squares
    // Entity **data;
    using Grid_Index = i64;
    Table<Grid_Index, Entity*> entities_snapped_to_grid;
};

// @Cleanup: Iron out the forward declarations
struct Undo_Handler;

struct Entity_Manager
{
    // We store the base entity here!
    Bucket_Array<Entity, 100> entity_array;
    RArr<Entity*>       entities_to_clean_up;

    RArr<Entity*>       all_entities;
    RArr<Entity*>       moving_entities; // This is for visual interpolation

    RArr<Guy*>          _by_guys;
    RArr<Door*>         _by_doors;
    RArr<Gate*>         _by_gates;
    RArr<Switch*>       _by_switches;
    RArr<Rock*>         _by_rocks;
    RArr<Wall*>         _by_walls;
    RArr<Floor*>        _by_floors;

    Proximity_Grid     *proximity_grid;

    Camera              camera;

    Pid                 next_entity_id; // 0
    i32                 active_hero_index; // 0

    // bool for_editing = false;
    
    // bool needs_reevaluate;
    
    // f32 player_speed_target = 1.0f;
    // f32 player_speed = 1.0f;
    
    // f32 gameplay_dt = 0.0f;

    Transaction_Id      next_transaction_id_to_issue;  // 1
    Transaction_Id      next_transaction_id_to_retire; // 1
    Transaction_Id      waiting_on_player_transaction; // 0
    bool player_transaction_caused_changes; // false

    RArr<Pid>           turn_order;

    RArr<Buffered_Move> buffered_moves;

    Undo_Handler *undo_handler;
};

// cast down the type
// example: cast from Monster to Entity
//          cast from Door    to Entity
template <class Target_Type>
Target_Type *Down(Entity *e)
{
    bool b = cmp_var_type_to_type(e->type, Target_Type);
    assert(b);
    return static_cast<Target_Type*>(e->derived_pointer);
}

Entity *find_entity(Entity_Manager *manager, Pid id);
void find_at_position(Proximity_Grid *grid, Vector3 pos, RArr<Entity*> *results);

Entity_Manager *make_entity_manager();
void load_ascii_level(Entity_Manager *manager, String full_path);
void post_frame_cleanup(Entity_Manager *manager);
void reset_entity_manager(Entity_Manager *manager);

void add_to_grid(Proximity_Grid *grid, Entity *e);
my_pair<bool, Entity*> remove_from_grid(Proximity_Grid *grid, Entity *e);

// @Temporary: Remove texture_2d_name later, since only door needs it
// When we have triangle mesh for the entities, they will use the texture
// that lives inside the triangle list info instead of relying on this.
template <class Entity_Type>
Entity_Type *Make(Entity_Manager *manager, i32 x, i32 y, String texture_2d_name);
