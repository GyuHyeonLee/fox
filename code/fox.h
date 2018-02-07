#ifndef FOX_H
#define FOX_H

/*
    TODO:
    
    ARCHITECTURE EXPLORATION
    - Collision detection?
        - Entry/exit?
        - What's the plan for robustness / shape definition?
    - Implement multiple sim regions per frame
        - Per-entity clocking
        - Sim region merging?  For multiple players?
    - Z!
        - Clean up things by using v3
        - Figure out how you go "up" and "down", and how is this rendered?

    - Debug code
        - Logging
        - Diagramming
        - (A LITTLE GUI, but only a little!) Switches / sliders / etc.
        - Draw tile chunks so we can verify that things are algiend /
        
    - Audio
        - Sound effect triggers
        - Ambient sounds
        - Music
    - Asset streaming
        
    - Metagame / save game?
        - How do you enter "save slot"?
        - Persistent unlocks/etc.
        - Do we allow saved games?  Probably yes, just only for "pausing",
        * Continuous save for crash recovery?
    - Rudimentary world gen (no quality, just "what sorts of things" we do)
        - Placement of background things
        - Connectivity?
        - Non-overlapping?
        - Map display
        - Magnets - how they work???
    - AI
        - Rudimentary monstar behavior example
        * Pathfinding
        - AI "storage"
        
    * Animation, should probably lead into rendering
        - Skeletal animation
        - Particle systems

    PRODUCTION
    - Rendering
    -> GAME
        - Entity system
        - World generation
*/
#include "fox_platform.h"

#define Minimum(a, b) ((a < b) ? a : b)
#define Maximum(a, b) ((a > b)? a : b)

//TODO: Implement sine ourselves because sine is pretty expensive because of precision purpose
#include <math.h>

struct memory_arena
{
    memory_index size;
    uint8 *base;
    memory_index used;

    int32 tempCount;
};

struct temporary_memory
{
    memory_index used;
    memory_arena *arena;
};

inline void
InitializeArena(memory_arena *arena, memory_index size, void *base)
{
    arena->size = size;
    arena->base = (uint8 *)base;
    arena->used = 0;
    arena->tempCount = 0;
}

inline temporary_memory
BeginTemporaryMemory(memory_arena *arena)
{
    temporary_memory result = {};

    result.arena = arena;
    result.used = arena->used;
    ++result.arena->tempCount;
    
    return result;
}

inline void
EndTemporaryMemory(temporary_memory memory)
{
    memory_arena *arena = memory.arena;
    Assert(arena->used >= memory.used);
    arena->used = memory.used;
    Assert(arena->tempCount > 0)
    --arena->tempCount;
}

inline void
CheckArena(memory_arena *arena)
{
    Assert(arena->tempCount == 0);
}

#define PushStruct(Arena, type) (type *)PushSize_(Arena, sizeof(type))
#define PushArray(Arena, count, type) (type *)PushSize_(Arena, count * sizeof(type))
#define PushSize(Arena, size) PushSize_(Arena, size)

inline void *
PushSize_(memory_arena *arena, memory_index size)
{
    Assert(arena->used + size <= arena->size);
    void *result = arena->base + arena->used;
    arena->used += size;

    return result;
}

#define ZeroStruct(instance) ZeroSize(sizeof(instance), &(instance))
// Zero memory
inline void
ZeroSize(memory_index size, void *ptr)
{
    // TODO : Check this guy for performance
    uint8 *byte = (uint8 *)ptr;
    while(size--)
    {
        *byte++ = 0;
    }
}

#include "fox_intrinsics.h"
#include "fox_math.h"
#include "fox_world.h"
#include "fox_sim_region.h"
#include "fox_render_group.h"

// This entity is being updated in low frequency(enemy that is far away from the player)    
// TODO : This is getting really huge. need to do something!
struct low_entity
{
    // TODO : It's kind of busted that pos can be invalid here,
    // but we store whether they would be invalid in the flags field,
    // so we have to check both.
    // Can we do something better here?
    world_position pos;
    sim_entity sim;
};

struct controlled_hero
{
    uint32 entityIndex;
    // Request for the hero ddp
    v2 ddPlayer;
    // Request for the sword dp
    v2 dSword;
    real32 dZ;
};


// This is an external hash!
struct pairwise_collision_rule
{
    // This is the only rule we currently have
    bool32 canCollide;
    
    uint32 storageIndexA;
    // This is the target entity index
    uint32 storageIndexB;

    // This is an external hash!    
    pairwise_collision_rule *nextInHash;
};

struct ground_buffer
{
    // NOTE : This is the center of the bitmap
    // NOTE : If the value of this is NULL, it means it's not ready
    world_position pos;
    loaded_bitmap bitmap;
};

struct game_state
{
    memory_arena worldArena;
    world *world;
    
    real32 typicalFloorHeight;

    // TODO : Should we allow split-sreen?
    uint32 cameraFollowingEntityIndex;
    world_position cameraPos;    
    
    controlled_hero controlledHeroes[ArrayCount(((game_input *)0)->controllers)];

    uint32 lowEntityCount;
    low_entity lowEntities[100000];

    loaded_bitmap grass[2];
    loaded_bitmap stone[4];
    loaded_bitmap tuft[3];
    
    // Bitmaps
    loaded_bitmap background;
    loaded_bitmap tree;
    loaded_bitmap shadow;
    loaded_bitmap sword;    
    loaded_bitmap stairwell;    

    hero_bitmaps heroBitmaps[4];

    // TODO : Must be power of two!
    // NOTE : This is external hash table as we store pointers, not entries!
    pairwise_collision_rule *collisionRules[256];
    pairwise_collision_rule *firstFreeCollisionRule;

    sim_entity_collision_volume_group *nullCollision;    
    sim_entity_collision_volume_group *swordCollision;
    sim_entity_collision_volume_group *stairCollision;
    sim_entity_collision_volume_group *playerCollision;
    sim_entity_collision_volume_group *monsterCollision;
    sim_entity_collision_volume_group *familiarCollision;    
    sim_entity_collision_volume_group *wallCollision;
    sim_entity_collision_volume_group *standardRoomCollision;    

    real32 time;

    loaded_bitmap diff;
    loaded_bitmap diffNormal;

};

struct transient_state
{
    bool32 isInitialized;
    memory_arena tranArena;
    
    uint32 groundBufferCount;
    ground_buffer *groundBuffers;

    int32 envMapWidth;
    int32 envMapHeight;
    // 1 : bottom ,2 : middle, 3 : top
    enviromnet_map envMaps[3];
};

#endif