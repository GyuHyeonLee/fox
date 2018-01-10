#ifndef FOX_TILE_H
#define FOX_TILE_H

struct world_position
{
    // TODO : It seems like we have to store chunkX / Y / Z with each
    // entity because even though the sim region gather doesn't need it at first,
    // and we could get by without it, entity references pull
    // in entities WITHOUT going through their world_chunk, and thus
    // still need to know the chunkX / Y / Z
    
    int32 chunkX;
    int32 chunkY;
    int32 chunkZ;

    // These are the offsets from the chunk center
    v3 offset_;
};

// TODO : Could make this just tile_chunk and then allow multiple
// tile chunks per x/y/z
// This is just a unit that contains lowEntityIndexes.
// There is nothing to do with the order
struct world_entity_block
{
    uint32 entityCount;
    uint32 lowEntityIndexes[16];
    world_entity_block *next;
};

struct world_chunk
{
    int32 chunkX;
    int32 chunkY;
    int32 chunkZ;

    world_entity_block firstBlock;
    
    world_chunk *nextInHash;
};

struct world
{
    v3 chunkDimInMeters;

    world_entity_block *firstFree;

    world_chunk chunkHash[4096];
};

#endif