#ifndef FOX_SIM_REGION_H

#define InvalidPos V3(10000.0f, 100000.0f, 10000.0f)

// Movement Spec
// Drag, acceleration..
struct move_spec
{
    bool32 unitMaxAccelVector;
    real32 speed;
    real32 drag;
};

enum entity_type
{
    EntityType_Null,

    EntityType_Space,

    EntityType_Hero,
    EntityType_Wall,
    EntityType_Familiar,
    EntityType_Monster,
    EntityType_Sword,    
    EntityType_Stairwell,    
};

#define HIT_POINT_SUB_COUNT 4
struct hit_point
{
    // TODO : Bake this down into one variable!
    uint8 flags;
    uint8 filledAmount;
};

struct sim_entity;
union entity_reference
{
    uint32 index;
    sim_entity *ptr;  
};

enum sim_entity_flags
{
    // TODO : Zsupported should be go away?
    EntityFlag_Nonspatial = (1 << 0),
    EntityFlag_Movable = (1 << 1),
    EntityFlag_CanCollide = (1 << 2),
    // Whether the entity is on the ground or not
    EntityFlag_ZSupported = (1 << 3),
    EntityFlag_Traversable = (1 << 4),    
};

struct sim_entity_collision_volume
{
    v3 offset;
    v3 dim;
};

struct sim_entity_collision_volume_group
{
    // This is the giant rectangle that contains every volumes
    // because usually we don't think about the volumes seperately
    // when we are trying to add to sim region
    sim_entity_collision_volume totalVolume;

    uint32 volumeCount;
    sim_entity_collision_volume *volumes;
};

// This takes place of high_entity
struct sim_entity
{
    uint32 storageIndex;
    bool32 updatable;

    // This is now relative to the simulation position
    // This is now the ground point, NOT the center of the entity! 
    v3 pos;

    v3 dPos;

    entity_type type;
    uint32 flags;
    
    sim_entity_collision_volume_group *collision;
    // What the hell is this var?
    int32 dAbsTileZ;

    // TODO : should hitpoints themsleves be entities?
    uint32 hitPointMax;
    hit_point hitPoints[16];

    entity_reference sword;
    real32 distanceLimit;

    real32 tBob;
    uint32 facingDirection;

    // TODO : Only for the stairs!
    real32 walkableHeight;

    v2 walkableDim;
};

struct sim_entity_hash
{
    sim_entity *ptr;
    uint32 index;
};

struct sim_region
{
    world *world;

    real32 maxEntityRadius;
    real32 maxEntityVelocity;

    // center of this sim_region
    world_position origin;
    // bounds of this sim_region
    rect3 bounds;
    rect3 updatableBounds;

    uint32 maxEntityCount;
    uint32 entityCount;
    sim_entity *entities;

    // This is to get the sim entity based on the storage index
    // because now, the storgae entity does not know about sim entity
    // So if someone want to get the sim entity with storageindex,
    // you have to come to hash using the torageindex, get the hash,
    // and then get the pointer to the sim entity
    sim_entity_hash hash[4096];
};

#define FOX_SIM_REGION_H
#endif