#include "fox_world.h"

#define TILE_CHUNK_SAFE_MARGIN (INT32_MAX / 64)
#define TILE_CHUNK_UNINITIALIZED INT32_MAX
// This does not mean anything but some kind of 'measure'
// because we are not operating in tiles anymore
#define TILES_PER_CHUNK 16

inline world_position
NullPosition()
{
    world_position result;
    result.chunkX = TILE_CHUNK_UNINITIALIZED;
    return result;
}

// We are only checking the chunkX,
// because that's our 'promise'
inline bool32
IsValid(world_position pos)
{
    bool32 result = pos.chunkX != TILE_CHUNK_UNINITIALIZED;
    return result;
}

// Is this value canonical to the CHUNK? (NOT THE SIMULATION REGION!!!!!)
inline bool32
IsCanonical(real32 chunkDim, real32 chunkRel)
{
    // TODO(casey): Fix floating point math so this can be exact?
    real32 Epsilon = 0.01f;
    bool32 Result = ((chunkRel >= -(0.5f*chunkDim + Epsilon)) &&
                     (chunkRel <= (0.5f*chunkDim + Epsilon)));

    return(Result);
}

inline bool32
IsCanonical(world *world, v3 chunkRelCoords)
{
    bool32 result = IsCanonical(world->chunkDimInMeters.x, chunkRelCoords.x) &&
                    IsCanonical(world->chunkDimInMeters.y, chunkRelCoords.y) && 
                    IsCanonical(world->chunkDimInMeters.z, chunkRelCoords.z);

    return result;
}

inline bool32
AreInSameChunk(world *world_, world_position *pos1, world_position *pos2)
{
    // These positions should had been canonicalized before coming here!
    Assert(IsCanonical(world_, pos1->offset_));
    Assert(IsCanonical(world_, pos2->offset_));

    bool32 isSame = (pos1->chunkX == pos2->chunkX) &&
                    (pos1->chunkY == pos2->chunkY) &&
                    (pos1->chunkZ == pos2->chunkZ);

    return isSame;
}

// TODO : should these coord functions be in seperate files?

//Coord basically means x or y
//because we are using this function for not to write one for x and one for y
inline void
RecanonicalizeCoord(real32 chunkDim, int32 *tileCoord, real32 *chunkRelCoord)
{

    // TODO : Need to do something that doesn't use the divide/multiply method
    // for recanonicalizing because this can end up rounding back on the tile
    // the player just came from if the value is too small

    int32 offset = RoundReal32ToInt32(*chunkRelCoord / chunkDim);

    // It doesn't matter even if we have one value that has two meanings(divided by 24bit and 8bit)
    // because in this case, the offset is for the tile value
    // and we have the tile value in the lower 8bit
    // Therefore, even if we just add the offset to it, it will be just added to the lower 8bit
    // and do not effect the upper 24bit
    *tileCoord += offset;
    *chunkRelCoord -= offset * chunkDim;

    Assert(IsCanonical(chunkDim, *chunkRelCoord));
}

//Recanonicalize both x AND y value based on the chunk
//which means just calling RecanonicalizeCoord twice... XD
inline world_position
MapIntoChunkSpace(world *world, world_position basePos, v3 offset)
{
    world_position result = basePos;

    result.offset_ += offset;

    RecanonicalizeCoord(world->chunkDimInMeters.x, &result.chunkX, &result.offset_.x);
    RecanonicalizeCoord(world->chunkDimInMeters.y, &result.chunkY, &result.offset_.y);
    RecanonicalizeCoord(world->chunkDimInMeters.z , &result.chunkZ, &result.offset_.z);
    

    return result;
}

//Get the chunk
internal world_chunk *
GetWorldChunk(world *world_, int32 chunkX, int32 chunkY, int32 chunkZ,
              memory_arena *arena = 0)
{
    // We are not allowing wrapping!
    //
    Assert(chunkX > -TILE_CHUNK_SAFE_MARGIN);
    Assert(chunkY > -TILE_CHUNK_SAFE_MARGIN);
    Assert(chunkZ > -TILE_CHUNK_SAFE_MARGIN);
    Assert(chunkX < TILE_CHUNK_SAFE_MARGIN);
    Assert(chunkY < TILE_CHUNK_SAFE_MARGIN);
    Assert(chunkZ < TILE_CHUNK_SAFE_MARGIN);

    // This is our hash function which is totally crappy XD
    // TODO : Better Hash Function!
    uint32 hashValue = 19 * chunkX + 7 * chunkY + 3 * chunkZ;
    // We have to mask it with arraycount so that we can always get number within arraycount boundary
    uint32 hashSlot = hashValue & (ArrayCount(world_->chunkHash) - 1);
    Assert(hashSlot < ArrayCount(world_->chunkHash));

    world_chunk *chunk = world_->chunkHash + hashSlot;
    do
    {
        if (chunkX == chunk->chunkX &&
            chunkY == chunk->chunkY &&
            chunkZ == chunk->chunkZ)
        {
            break;
        }

        // This means the hash slot is not initiallized
        // because rememeber : the game doesn't allow position that is
        // smaller than TILE_CHUNK_SAFE_MARGIN

        // The slot is occupied, but the nextinhash is empty
        // So we need to create new one
        if (arena && chunk->chunkX != TILE_CHUNK_UNINITIALIZED && !chunk->nextInHash)
        {
            chunk->nextInHash = PushStruct(arena, world_chunk);
            chunk = chunk->nextInHash;
            // The tile chunk that we newly created should be 0
            // So that we can know that it's not initialized
            chunk->chunkX = TILE_CHUNK_UNINITIALIZED;
        }

        // If we have created a new chunk on the upper side,
        // this if statement must be excuted
        // This if statements means we have new tile chunk
        // but it's not initialized
        if (arena && chunk->chunkX == TILE_CHUNK_UNINITIALIZED)
        {
            // Store the coordinates
            chunk->chunkX = chunkX;
            chunk->chunkY = chunkY;
            chunk->chunkZ = chunkZ;

            chunk->nextInHash = 0;

            break;
        }

        // If all of those if fails, just check the next one in hash
        chunk = chunk->nextInHash;
    } while (chunk);

    return chunk;
}

// Substract two world_position and get the real32 x and y
// Therefore, in this case, x and y can be bigger than tileSideInMeters
// For example, if the two positions were 5 tiles away, dXY should be a lot bigger than one tile!
internal v3
SubstractTwoWMP(world *world, world_position *pos1, world_position *pos2)
{
    v3 dTile = V3((real32)pos1->chunkX - (real32)pos2->chunkX,
                    (real32)pos1->chunkY - (real32)pos2->chunkY,
                    (real32)pos1->chunkZ - (real32)pos2->chunkZ);

    v3 result = Hadamard(dTile, world->chunkDimInMeters) + pos1->offset_ - pos2->offset_; 

    return result;
}

internal world_position
CenteredChunkPoint(int32 chunkX, int32 chunkY, int32 chunkZ)
{
    world_position result = {};
    result.chunkX = chunkX;
    result.chunkY = chunkY;
    result.chunkZ = chunkZ;

    return result;
}

internal world_position
CenteredChunkPoint(world_chunk *chunk)
{
    world_position result = CenteredChunkPoint(chunk->chunkX, chunk->chunkY, chunk->chunkZ);
    return result;
}


internal void
InitializeWorld(world *world, v3 chunkDimInMeters)
{
    world->chunkDimInMeters = chunkDimInMeters;
    // Because we don't have any entity block
    world->firstFree = 0;

    for (uint32 chunkIndex = 0;
         chunkIndex < ArrayCount(world->chunkHash);
         ++chunkIndex)
    {
        world_chunk *chunk = world->chunkHash + chunkIndex;
        chunk->chunkX = TILE_CHUNK_UNINITIALIZED;
        chunk->firstBlock.entityCount = 0;
    }
}

// This function will not be called that frequently(except while initializing)
// because not many entities gonna change their chunk

// For now, this function does not actually change the location
// it just relocate the entity to the correct entity block
// it can also allocate new entity block

// TODO : If this moves an entity into the camera bounds, 
// should it automatically go into the high set immdeiately?
// because for now, the changed entity will go into the high set in next frame, not this frame
inline void
ChangeEntityLocationRaw(memory_arena *arena, world *world_, uint32 lowEntityIndex,
                     world_position *oldPos, world_position *newPos)
{
    Assert(!oldPos || IsValid(*oldPos));
    Assert(!newPos || IsValid(*newPos));    

    // 
    if (oldPos && newPos && AreInSameChunk(world_, oldPos, newPos))
    {
        // Leave entity where it is
        // because they are in same chunk! no need to change it
    }
    else
    {
        // If there was a oldpos, we need to get this entity out of it's old entityblock
        if (oldPos)
        {
            // NOTE : pull the entity out of its entity block
            // and make the firstblock to have the free space
            // because we don't want to hunt down every time to find the empty space

            // Get the chunk that has this entity
            world_chunk *oldChunk =
                GetWorldChunk(world_, oldPos->chunkX, oldPos->chunkY, oldPos->chunkZ);
            // We always need to get the chunk
            // because unless, we shouldn't be here!
            Assert(oldChunk);
            
            if (oldChunk)
            {
                // This variable is for double loop break
                bool32 notFound = true;

                // NOTE : Start finding the entity, starting from the firstBlock of the chunk

                world_entity_block *firstBlock = &oldChunk->firstBlock;

                for (world_entity_block *block = &oldChunk->firstBlock;
                     block && notFound;
                     block = block->next)
                {
                    for (uint32 index = 0;
                         index < block->entityCount && notFound;
                         ++index)
                    {
                        // If we find it,
                        if (block->lowEntityIndexes[index] == lowEntityIndex)
                        {
                            // copy the last entity of the first block to the location
                            // where we are about to delete
                            // so that the firstblock has free space
                            block->lowEntityIndexes[index] =
                                firstBlock->lowEntityIndexes[--firstBlock->entityCount];

                            // If there is no entity left in the first block,
                            if (firstBlock->entityCount == 0)
                            {
                                // if there was nextBlock, copy that to the firstBlock
                                // and mark the nextBlock to be free
                                if (firstBlock->next)
                                {
                                    world_entity_block *nextBlock = firstBlock->next;
                                    *firstBlock = *nextBlock;

                                    nextBlock->next = world_->firstFree;
                                    world_->firstFree = nextBlock;
                                }
                            }

                            notFound = false;
                        }

                        // If we couldn't find it, move to the next entity block
                    }
                }
            }
        }

        // NOTE : Insert the entity into its new entity block
        if(newPos)
        {
            world_chunk *newChunk =
                GetWorldChunk(world_, newPos->chunkX, newPos->chunkY, newPos->chunkZ, arena);
            world_entity_block *firstBlock = &newChunk->firstBlock;

            // Check whether the firstBlock of the chunk has room for new entity
            if (firstBlock->entityCount == ArrayCount(firstBlock->lowEntityIndexes))
            {
                // NOTE : We're out of room, get a new block!

                world_entity_block *oldBlock = world_->firstFree;
                if (oldBlock)
                {
                    // If there was a firstfree block,
                    //  make the firstfree to the next because we're about to use this block
                    world_->firstFree = oldBlock->next;
                }
                else
                {
                    // If there was not firstfree block,
                    // make a new one
                    oldBlock = PushStruct(arena, world_entity_block);
                }

                // Make a copy of the first block,
                // point to it(that's why it's name is oldBlock, not newblock)
                // and then make the entityCount to 0 so that the firstBlock can accept new entites
                *oldBlock = *firstBlock;
                firstBlock->next = oldBlock;
                firstBlock->entityCount = 0;
            }

            Assert(firstBlock->entityCount < ArrayCount(firstBlock->lowEntityIndexes));
            firstBlock->lowEntityIndexes[firstBlock->entityCount++] = lowEntityIndex;
        }
    }
}

// This change the block location AND
// it also moves the position, too.
internal void
ChangeEntityLocation(memory_arena *arena, world *world_, low_entity *lowEntity, uint32 lowEntityIndex,
                    world_position newPosInit)
{
    world_position *oldPos = 0; 
    world_position *newPos = 0;
    
    if(!IsSet(&lowEntity->sim, EntityFlag_Nonspatial) && IsValid(lowEntity->pos))
    {
        oldPos = &lowEntity->pos;
    }

    if(IsValid(newPosInit))
    {
        newPos = &newPosInit;
    }

    ChangeEntityLocationRaw(arena, world_, lowEntityIndex, oldPos, newPos);

    if(newPos)
    {
        lowEntity->pos = *newPos;
        ClearFlags(&lowEntity->sim, EntityFlag_Nonspatial);
    }
    else
    {
        lowEntity->pos = NullPosition();
        AddFlags(&lowEntity->sim, EntityFlag_Nonspatial);        
    }
}