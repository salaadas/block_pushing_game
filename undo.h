#pragma once

#include "common.h"

#include "entity_manager.h"

extern bool doing_undo;

enum Undo_Action_Type : u8
{
    CHANGE      = 0,
    CREATION    = 1,
    DESTRUCTION = 2,
};

struct Undo_Record
{
    f64 gameplay_time;

    // Things that change in that frame.
    String transactions;

    // bool check_point;
};

// @Cleanup: Iron out the forward declarations
struct Entity_Manager;

struct Undo_Handler
{
    Entity_Manager *manager = NULL;
    RArr<Undo_Record*> undo_records;

    Table<Pid, Entity*> cached_entity_state;

    bool dirty = false;
    bool enabled = false;
};

/*
struct Undo_Editor_Info
{
    String     description;
    _type_Type entity_type_info;
    Pid        entity_id;
    i64        num_changed;
};
*/

void init_undo_handler(Undo_Handler *handler);
void reset_undo(Undo_Handler *handler);

void undo_mark_beginning(Entity_Manager *manager);
void undo_end_frame(Undo_Handler *handler);

void do_one_undo(Entity_Manager *manager);

bool save_game(Entity_Manager *manager);
bool load_game(Entity_Manager *manager);
