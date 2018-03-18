#include "fox.h"
#include "fox_render_group.h"
#include "fox_render_group.cpp"
#include "fox_random.h"
#include "fox_sim_region.h"
#include "fox_entity.h"
#include "fox_world.cpp"

// #include "fox_etity.cpp"
#include "fox_sim_region.cpp"

// NOTE : OpenGL includes
// #include "include/glad/glad.h"
#include "include/GLFW/glfw3.h"
namespace
{
    #define TONE_VOLUME 3000
    #define PLAYER_ACC 50.0f
}

internal void
GameOutputSound(game_state *gameState, game_sound_output_buffer *soundBuffer)
{
#if 0
    int16 toneVolume = TONE_VOLUME;
    int wavePeriod = soundBuffer->samplesPerSecond / gameState->toneHz;//how many samples are in one cycle

    int16 *locToWrite = soundBuffer->samples;
    for(int sampleIndex = 0;
        sampleIndex < soundBuffer->sampleCount;
        ++sampleIndex)
    {
        real32 sineValue = sinf(gameState->tSine);
        int16 sampleValue = (int16)(sineValue * toneVolume);
        *locToWrite++ = sampleValue;//left
        *locToWrite++ = sampleValue;//right

        gameState->tSine += 2.0f * Pi32 * 1.0f / (real32)wavePeriod;
        if(gameState->tSine > 2.0f * Pi32)
        {
            gameState->tSine -= 2.0f * Pi32;             
        }
    }
#endif
}

// This is the memory template for the bmp files
// When we read the bmp file, the memory block should be EXACTLY like this(including the ORDER!)
// Start from here, start exact fitting because we should read this memory in exact order
#pragma pack(push, 1)
struct bitmap_header
{
    uint16 fileType;
    uint32 fileSize;
    uint16 reserved1;
    uint16 reserved2;
    uint32 bitmapOffset;
    uint32 size;
    int32 width;
    int32 height;
    uint16 planes;
    uint16 bitsPerPixel;
    uint32 compression;
    uint32 sizeOfBitmap;
    int32 horzResolution;
    int32 vertResolution;
    uint32 colorsUsed;
    uint32 colorsImportant;

    uint32 redMask;
    uint32 greenMask;
    uint32 blueMask;
};
#pragma pack(pop)
// End the exact fitting and back to the regular packing

inline v2 
SetTopDownAlign(loaded_bitmap *bitmap, v2 align)
{
    align.y = bitmap->height - align.y;

    align.x = SafeRatio0((real32)align.x, (real32)bitmap->width);
    align.y = SafeRatio0((real32)align.y, (real32)bitmap->height);

    return align;
}

// NOTE : Align is based on left bottom corner as Y is up -> which means top-down
internal loaded_bitmap
DEBUGLoadBMP(thread_context *thread, debug_platform_read_entire_file *readEntireFile, char *fileName,
            int32 alignX, int32 topDownAlignY)
{
    loaded_bitmap result = {};

    debug_read_file_result readResult = readEntireFile(thread, fileName);

    if(readResult.contentSize != 0)
    {
        bitmap_header *bitmapHeader = (bitmap_header *)readResult.content;
        uint32 *pixels = (uint32 *)((uint8 *)readResult.content + bitmapHeader->bitmapOffset);

        result.memory = pixels;

        result.width = bitmapHeader->width;
        result.height = bitmapHeader->height;
        result.widthOverHeight = (real32)result.width / (real32)result.height;

        result.alignPercentage = SetTopDownAlign(&result, V2i(alignX, topDownAlignY));
                
        // NOTE : This function is only for bottom up bmps for now.
        Assert(result.height >= 0);
        // TODO : Do this for each compression type?
        Assert(bitmapHeader->compression == 3);

        // If you are using this generically,
        // the height will be negative for top-down
        // please remember that BMP files can go in either direction and
        // (there can be compression, etc... don't think this 
        // is complete BMP loading function)

        // NOTE : Byte Order in memory is determined by the header itself,
        // so we have to read out the masks and convert our piexels ourselves.
        uint32 redMask = bitmapHeader->redMask;
        uint32 greenMask = bitmapHeader->greenMask;
        uint32 blueMask = bitmapHeader->blueMask;
        uint32 alphaMask = ~(redMask | greenMask | blueMask);

        // Find out how many bits we must rightshift for each colormask
        bit_scan_result redScan = FindLeastSignificantSetBit(redMask);
        bit_scan_result greenScan = FindLeastSignificantSetBit(greenMask);
        bit_scan_result blueScan = FindLeastSignificantSetBit(blueMask);
        bit_scan_result alphaScan = FindLeastSignificantSetBit(alphaMask);

        Assert(redScan.found && greenScan.found && blueScan.found && alphaScan.found);

        int32 alphaShift = (int32)alphaScan.index;
        int32 redShift = (int32)redScan.index;
        int32 greenShift = (int32)greenScan.index;
        int32 blueShift = (int32)blueScan.index;
        
        uint32 *sourceDest = pixels;

        for(int32 y = 0;
            y < bitmapHeader->height;
            ++y)
        {
            for(int32 x = 0;
                x < bitmapHeader->width;
                ++x)
            {
                uint32 color = *sourceDest;

                v4 texel = {(real32)((color & redMask) >> redShift),
                            (real32)((color & greenMask) >> greenShift),
                            (real32)((color & blueMask) >> blueShift),
                            (real32)((color & alphaMask) >> alphaShift)};

                texel = SRGB255ToLinear1(texel);
                
                // NOTE : Premultiplied Alpha!
                texel.rgb *= texel.a;

                texel = Linear1ToSRGB255(texel);
                
                *sourceDest++ = (((uint32)(texel.a + 0.5f) << 24) |
                                 ((uint32)(texel.r + 0.5f) << 16) |
                                 ((uint32)(texel.g + 0.5f) << 8) |
                                 ((uint32)(texel.b + 0.5f) << 0));
            
            }
        }        
    }
    
        
    // NOTE : Because the usual bmp format is already bottom up, don't need a prestep anymore!
    result.pitch = result.width*BITMAP_BYTES_PER_PIXEL;
    
    // There might be top down bitmaps. Use this method for them
#if 0
    // Because we want to go to negative, set it to negative value
    result.pitch = -result.width*BITMAP_BYTES_PER_PIXEL;
    
    // Readjust the memory to be the start of the last row
    // because the bitmap that was read is upside down. 
    // Therefore, we need to read backward
    result.memory = (uint8 *)result.memory - result.pitch*(result.height - 1);
#endif
    return result;
}

// By default, the align it by the center!
internal loaded_bitmap
DEBUGLoadBMP(thread_context *thread, debug_platform_read_entire_file *readEntireFile, char *fileName)
{
    loaded_bitmap result = DEBUGLoadBMP(thread, readEntireFile, fileName, 0, 0);
    // align it by center
    result.alignPercentage = V2(0.5f, 0.5f);
    return result;
}

// TODO : This is only for the world building! Remove it completely later.
inline world_position
TilePositionToChunkPosition(world *world, int32 absTileX, int32 absTileY, int32 absTileZ, 
                            v3 additionalOffset = V3(0, 0, 0))
{
    world_position basePos = {};

    real32 tileSideInMeters = 1.4f;
    real32 tileDeptInMeters = 3.0f;

    v3 tileDim = V3(tileSideInMeters, tileSideInMeters, tileDeptInMeters);
    v3 offset = Hadamard(tileDim, V3((real32)absTileX, (real32)absTileY, (real32)absTileZ));

    // Recanonicalize this value
    world_position result = MapIntoChunkSpace(world, basePos, offset + additionalOffset);

    Assert(IsCanonical(world, result.offset_));

    return result;
}

struct add_low_entity_result
{
    uint32 lowIndex;
    low_entity *low;
};

// This function adds new low entity AND put it to the entity block based on the chunk
// where the entity is.
internal add_low_entity_result
AddLowEntity(game_state *gameState, entity_type type, world_position worldPosition)
{
    Assert(gameState->lowEntityCount < ArrayCount(gameState->lowEntities));
    uint32 lowIndex = gameState->lowEntityCount++;

    low_entity *low = gameState->lowEntities + lowIndex;
    *low = {};
    low->sim.type = type;

    low->pos = NullPosition();

    ChangeEntityLocation(&gameState->worldArena, gameState->world, low, lowIndex, worldPosition);
    
    add_low_entity_result result = {};
    result.low = low;
    result.lowIndex = lowIndex;

    return result;
}

// Draw grounded low entities
internal add_low_entity_result
AddGroundedLowEntity(game_state *gameState, entity_type type, world_position worldPosition,
                    sim_entity_collision_volume_group *collisionGroup)
{
    add_low_entity_result result = AddLowEntity(gameState, type, worldPosition);
    result.low->sim.collision = collisionGroup;

    return result;
}

internal void
InitHitPoints(low_entity *low, uint32 hitPointCount)
{
    Assert(hitPointCount < ArrayCount(low->sim.hitPoints));
    low->sim.hitPointMax = hitPointCount;
    for(uint32 hitPointIndex = 0;
        hitPointIndex < low->sim.hitPointMax;
        ++hitPointIndex)
    {
        hit_point *hitPoint = low->sim.hitPoints + hitPointIndex;
        hitPoint->flags = 0;
        hitPoint->filledAmount = HIT_POINT_SUB_COUNT;
    }
}

internal add_low_entity_result
AddSword(game_state *gameState)
{
    add_low_entity_result entity = AddLowEntity(gameState, EntityType_Sword, NullPosition());
    entity.low->sim.collision = gameState->swordCollision;
    
    return entity;
}

internal add_low_entity_result
AddPlayer(game_state *gameState)
{
    world_position pos = gameState->cameraPos;
    add_low_entity_result entity = AddGroundedLowEntity(gameState, EntityType_Hero, pos, gameState->playerCollision);
    
    AddFlags(&entity.low->sim, EntityFlag_Movable|EntityFlag_ZSupported|EntityFlag_CanCollide);

    InitHitPoints(entity.low, 3);
    // MakeEntityHighFrequency(gameState, entity.lowIndex);

    // Add sword for the player
    add_low_entity_result sword = AddSword(gameState);
    entity.low->sim.sword.index = sword.lowIndex;

    // If there is no entity that the camera is following,
    // Make this new entity followed by the camera
    if(gameState->cameraFollowingEntityIndex == 0)
    {
        gameState->cameraFollowingEntityIndex = entity.lowIndex;
    }

    return entity;
}

internal add_low_entity_result
AddMonster(game_state *gameState, uint32 absTileX, uint32 absTileY, uint32 absTileZ)
{
    world_position worldPositionOfTile = 
        TilePositionToChunkPosition(gameState->world, absTileX, absTileY, absTileZ);
    
    add_low_entity_result entity = 
        AddGroundedLowEntity(gameState, EntityType_Monster, worldPositionOfTile, gameState->monsterCollision);

    AddFlags(&entity.low->sim, EntityFlag_CanCollide);

    InitHitPoints(entity.low, 2);
    
    return entity;
}

internal add_low_entity_result
AddFamiliar(game_state *gameState, uint32 absTileX, uint32 absTileY, uint32 absTileZ)
{
    world_position worldPositionOfTile = 
        TilePositionToChunkPosition(gameState->world, absTileX, absTileY, absTileZ);
    
    add_low_entity_result entity = AddLowEntity(gameState, EntityType_Familiar, worldPositionOfTile);

    entity.low->sim.collision = gameState->familiarCollision;

    AddFlags(&entity.low->sim, EntityFlag_Movable);

    return entity;
}

internal add_low_entity_result
AddStair(game_state *gameState, uint32 absTileX, uint32 absTileY, uint32 absTileZ)
{
    // For the stairs, we want to offset a little be higher
    // so that the maximum Z value is the ground of next floor
    world_position worldPositionOfTile = 
        TilePositionToChunkPosition(gameState->world, absTileX, absTileY, absTileZ);

    add_low_entity_result entity = 
        AddGroundedLowEntity(gameState, EntityType_Stairwell, worldPositionOfTile, gameState->stairCollision);

    AddFlags(&entity.low->sim, EntityFlag_ZSupported);    

    entity.low->sim.walkableDim = entity.low->sim.collision->totalVolume.dim.xy;    
    entity.low->sim.walkableHeight = gameState->typicalFloorHeight;

    return entity;
}

// Unlike other addentity calls, this is from the center!
// Which means, absTileX,Y,Z are or centertiles
internal add_low_entity_result
AddStandardSpace(game_state *gameState, uint32 absTileX, uint32 absTileY, uint32 absTileZ)
{
    world_position worldPositionOfTile = 
        TilePositionToChunkPosition(gameState->world, absTileX, absTileY, absTileZ);

    add_low_entity_result entity = 
        AddGroundedLowEntity(gameState, EntityType_Space, worldPositionOfTile, gameState->standardRoomCollision);

    AddFlags(&entity.low->sim, EntityFlag_Traversable);

    return entity;
}

// Add wall based on the tilemap
// Of course because we don't have tilemap anymore, 
// we have to map this tilemap to the chunkXY and offset
internal add_low_entity_result
AddWall(game_state *gameState, uint32 absTileX, uint32 absTileY, uint32 absTileZ)
{
    world_position worldPositionOfTile = 
        TilePositionToChunkPosition(gameState->world, absTileX, absTileY, absTileZ);

    add_low_entity_result entity = 
        AddGroundedLowEntity(gameState, EntityType_Wall, worldPositionOfTile, gameState->wallCollision);

    AddFlags(&entity.low->sim, EntityFlag_CanCollide);
    
    return entity;
}

// DrawHitPoints
internal void
DrawHitpoints(sim_entity *entity, render_group *pieceGroup)
{
    if(entity->hitPointMax >= 1)
    {
        // size of each health square
        v2 healthDim = {0.2f, 0.2f};
        // space between each heatlh square
        real32 spacingX = 1.5f * healthDim.x;
        v2 hitP = {-0.5f * (entity->hitPointMax - 1) * spacingX, -0.1f};
        v2 dHitP = {spacingX, 0.0f};

        for(uint32 healthIndex = 0;
            healthIndex < entity->hitPointMax;
            ++healthIndex)
        {
            hit_point *hitPoint = entity->hitPoints + healthIndex;
            v4 color = {1.0f, 0.0f, 0.0f, 1.0f};
            if(hitPoint->filledAmount == 0)
            {
                color = V4(0.2f, 0.2f, 0.2f, 1.0f);
            }
            PushRect(pieceGroup, V3(hitP, 0), healthDim, color);
            hitP += dHitP;
        }
    }
}

sim_entity_collision_volume_group *
MakeSimpleGroundedCollision(game_state *gameState, real32 dimX, real32 dimY, real32 dimZ)
{
    // TODO : Not world arena! Change to using the fundamental entity arena
    sim_entity_collision_volume_group *result = 
        PushStruct(&gameState->worldArena, sim_entity_collision_volume_group);
    result->volumeCount = 1;
    result->volumes = PushArray(&gameState->worldArena, result->volumeCount, sim_entity_collision_volume);
    result->totalVolume.offset = V3(0, 0, 0.5f*dimZ);
    result->totalVolume.dim = V3(dimX, dimY, dimZ);
    result->volumes[0] = result->totalVolume;

    return result;
}

// TODO : MultiThreaded?
internal void
FillGroundChunks(transient_state *tranState, game_state *gameState, 
                ground_buffer *groundBuffer, world_position *chunkPos)
{
    temporary_memory groundRenderMemory = BeginTemporaryMemory(&tranState->tranArena);
    
    // TODO : How do we want to control our ground chunk resolution?
    // TODO : Find out what is the precies maxPushBufferSize is!
    render_group *groundRenderGroup = 
        AllocateRenderGroup(&tranState->tranArena, Megabytes(4), groundBuffer->bitmap.width, groundBuffer->bitmap.height);

    Clear(groundRenderGroup, V4(0.2f, 0.2f, 0.2f, 1.0f));

    loaded_bitmap *buffer = &groundBuffer->bitmap;
    buffer->alignPercentage = V2(0.5f, 0.5f);
    buffer->widthOverHeight = 1.0f;

    groundBuffer->pos = *chunkPos; 

#if 0
    real32 width = gameState->world->chunkDimInMeters.x;
    real32 height = gameState->world->chunkDimInMeters.y;

    for(int32 chunkOffsetY = -1;
        chunkOffsetY <= 1;
        ++chunkOffsetY)
    {
        for(int32 chunkOffsetX = -1;
            chunkOffsetX <= 1;
            ++chunkOffsetX)
        {
            int32 chunkX = chunkPos->chunkX + chunkOffsetX;
            int32 chunkY = chunkPos->chunkY + chunkOffsetY;
            int32 chunkZ = chunkPos->chunkZ;

            // TODO : Make random number generation more systemic
            // TODO : Look into wang hashing or some other spatial seed generation
            random_series series = Seed(132*chunkX + 217*chunkY + 532*chunkZ);

            v2 center = V2(chunkOffsetX*width, chunkOffsetY*height);
    
            for(uint32 grassIndex = 0;
                grassIndex < 100;
                ++grassIndex)
            {
                loaded_bitmap *stamp;
                if(RandomChoice(&series, 2))
                {
                    stamp = gameState->grass + RandomChoice(&series, ArrayCount(gameState->grass));
                }
                else
                {
                    stamp = gameState->stone + RandomChoice(&series, ArrayCount(gameState->stone));            
                }

                v2 pos = center + Hadamard(V2(width, height), V2(RandomUnilateral(&series), RandomUnilateral(&series)));

                PushBitmap(groundRenderGroup, stamp, 1.0f, V3(pos, 0.0f));
            }
        }
    }    
#endif

    RenderGroupToOutputBuffer(groundRenderGroup, buffer);
    EndTemporaryMemory(groundRenderMemory);
}

internal void
ClearBitmap(loaded_bitmap *bitmap)
{
    if(bitmap)
    {
        int32 totalBitmapSize = bitmap->width * bitmap->height * BITMAP_BYTES_PER_PIXEL;
        ZeroSize(totalBitmapSize, bitmap->memory);
    }
}

internal loaded_bitmap
MakeEmptyBitmap(memory_arena *arena, int32 width, int32 height, bool32 shouldBeCleared = true)
{
    loaded_bitmap result = {};

    result.width = width;
    result.height = height;
    result.pitch = result.width * BITMAP_BYTES_PER_PIXEL;
    int32 totalBitmapSize = result.width * result.height * BITMAP_BYTES_PER_PIXEL;
    result.memory = PushSize_(arena, totalBitmapSize);

    if(shouldBeCleared)
    {
        ClearBitmap(&result);
    }

    return result;
}


internal void
MakeSphereDiffuseMap(loaded_bitmap *bitmap, real32 Cx = 1.0f, real32 Cy = 1.0f)
{
    real32 InvWidth = 1.0f / (real32)(bitmap->width - 1);
    real32 InvHeight = 1.0f / (real32)(bitmap->height - 1);
    
    uint8 *row = (uint8 *)bitmap->memory;
    for(int32 y = 0;
        y < bitmap->height;
        ++y)
    {
        uint32 *pixel = (uint32 *)row;
        for(int32 x = 0;
            x < bitmap->width;
            ++x)
        {
            v2 bitmapUV = {InvWidth*(real32)x, InvHeight*(real32)y};

            real32 Nx = Cx*(2.0f*bitmapUV.x - 1.0f);
            real32 Ny = Cy*(2.0f*bitmapUV.y - 1.0f);

            real32 rootTerm = 1.0f - Nx*Nx - Ny*Ny;
            real32 alpha = 0.0f;            
            if(rootTerm >= 0.0f)
            {
                alpha = 1.0f;
            }

            v3 baseColor = {0.0f, 0.0f, 0.0f};
            alpha *= 255.0f;
            v4 color = {alpha*baseColor.x,
                        alpha*baseColor.y,
                        alpha*baseColor.z,
                        alpha};

            *pixel++ = (((uint32)(color.a + 0.5f) << 24) |
                        ((uint32)(color.r + 0.5f) << 16) |
                        ((uint32)(color.g + 0.5f) << 8) |
                        ((uint32)(color.b + 0.5f) << 0));
        }

        row += bitmap->pitch;
    }
}

internal void
MakeSphereNormalMap(loaded_bitmap *bitmap, real32 roughness)
{
    real32 invWidth = 1.0f/(real32)(bitmap->width - 1);
    real32 invHeight = 1.0f/(real32)(bitmap->height - 1);

    uint8 *row = (uint8 *)bitmap->memory;
    for(int32 y = 0;
        y < bitmap->height;
        ++y)
    {
        uint32 *pixel = (uint32 *)row;
        for(int32 x = 0;
            x < bitmap->width;
            ++x)
        {
            v2 bitmapUV = {invWidth*(real32)x, invHeight*(real32)y};

            real32 nX = 2.0f*bitmapUV.x - 1.0f;
            real32 nY = 2.0f*bitmapUV.y - 1.0f;

            real32 rootTerm = 1.0f - nX*nX - nY*nY;
            
            // Normal is in -101 space!
            v3 normal = {0, 0.707106781188f, 0.707106781188f};
            real32 nZ = 0.0f;
            if(rootTerm >= 0.0f)
            {
                nZ = Root2(rootTerm);
                normal = {nX, nY, nZ};
            }

            v4 color = {255.0f*(0.5f*(normal.x + 1.0f)),
                        255.0f*(0.5f*(normal.y + 1.0f)),
                        255.0f*(0.5f*(normal.z + 1.0f)),
                        255.0f*roughness};
            
            *pixel++ = (((uint32)(color.a + 0.5f) << 24) |
                            ((uint32)(color.r + 0.5f) << 16) |
                            ((uint32)(color.g + 0.5f) << 8) |
                            ((uint32)(color.b + 0.5f) << 0));
        }

        row += bitmap->pitch;
    }
}

inline void
SetHeroBitmapAlign(hero_bitmaps *bitmap, v2 align)
{    
    v2 topDownAlign = SetTopDownAlign(&bitmap->head, align);

    bitmap->head.alignPercentage = topDownAlign;
    bitmap->cape.alignPercentage = topDownAlign;
    bitmap->torso.alignPercentage = topDownAlign;
}

#if FOX_DEBUG
game_memory *debugGlobalMemory;
#endif

extern "C" GAME_UPDATE_AND_RENDER(GameUpdateAndRender)
{
#if FOX_DEBUG
    debugGlobalMemory = memory;
#endif

    BEGIN_TIMED_BLOCK(GameUpdateAndRender);

    Assert(sizeof(game_state) <= memory->permanentStorageSize);
    game_state *gameState = (game_state *)memory->permanentStorage;

    uint32 groundBufferWidth = 256;
    uint32 groundBufferHeight = 256;

    real32 pixelsToMeters = 1.0f / 42.0f;

    if(!memory->isInitialized)
    {
        // TODO : Talk about this soon!  Let's start partitioning our memory space!
        InitializeArena(&gameState->worldArena, 
                        (memory_index)(memory->permanentStorageSize - sizeof(game_state)),
                        (uint8 *)memory->permanentStorage + sizeof(game_state));

        gameState->world = PushStruct(&gameState->worldArena, world);

        gameState->typicalFloorHeight = 3.0f;
        InitializeWorld(gameState->world, 
                        V3(pixelsToMeters*groundBufferWidth,
                            pixelsToMeters*groundBufferHeight,
                            gameState->typicalFloorHeight));

        uint32 tilesPerWidth = 17;
        uint32 tilesPerHeight = 9;

        real32 tileSideInMeters = 1.4f;
        real32 tileDeptInMeters = 3.0f;
        // NOTE : Reserve entity slot 0 for the null entity
        AddLowEntity(gameState, EntityType_Null, NullPosition());
        gameState->lowEntityCount = 1;

        gameState->swordCollision = MakeSimpleGroundedCollision(gameState, 1.0f, 0.5f, 0.1f);
        gameState->stairCollision = MakeSimpleGroundedCollision(gameState, 
                                                                tileSideInMeters, 
                                                                2.0f*tileSideInMeters, 
                                                                1.1f*tileDeptInMeters);
        gameState->playerCollision = MakeSimpleGroundedCollision(gameState, 1.0f, 0.5f, 1.2f);
        gameState->monsterCollision = MakeSimpleGroundedCollision(gameState, 1.0f, 0.5f, 0.5f);
        gameState->monsterCollision = MakeSimpleGroundedCollision(gameState, 1.0f, 0.5f, 0.5f);        
        gameState->wallCollision = MakeSimpleGroundedCollision(gameState, 
                                                                tileSideInMeters, 
                                                                tileSideInMeters, 
                                                                0.5f*tileDeptInMeters);  
                                                                
        gameState->standardRoomCollision = MakeSimpleGroundedCollision(gameState, 
                                                                tilesPerWidth * tileSideInMeters, 
                                                                tilesPerHeight * tileSideInMeters, 
                                                                0.9f*tileDeptInMeters);  


        gameState->grass[0] = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, 
                                            "../fox/data/test2/grass00.bmp");
        gameState->grass[1] = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, 
                                            "../fox/data/test2/grass01.bmp");

        gameState->stone[0] = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, 
                                            "../fox/data/test2/ground00.bmp");
        gameState->stone[1] = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, 
                                            "../fox/data/test2/ground01.bmp");
        gameState->stone[2] = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, 
                                            "../fox/data/test2/ground02.bmp");
        gameState->stone[3] = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, 
                                            "../fox/data/test2/ground03.bmp");
                            
        gameState->tuft[0] = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, 
                                            "../fox/data/test2/tuft00.bmp");
        gameState->tuft[1] = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, 
                                            "../fox/data/test2/tuft01.bmp");
        gameState->tuft[2] = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, 
                                            "../fox/data/test2/tuft02.bmp");
                            
                             

        gameState->background = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, 
                                            "../fox/data/test/test_background.bmp");
        gameState->tree = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, 
                                        "../fox/data/test2/tree00.bmp", 40, 80);                                            
        gameState->shadow = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, 
                                        "../fox/data/test/test_hero_shadow.bmp", 72, 182);                                            
        gameState->sword = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, 
                                        "../fox/data/test2/rock03.bmp", 29, 10);                                            
        gameState->stairwell = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, 
                                        "../fox/data/test2/rock03.bmp");                                            
            
                                            

        hero_bitmaps *bitmap = gameState->heroBitmaps;
        bitmap->head = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, 
                                            "../fox/data/test/test_hero_right_head.bmp");
        bitmap->cape = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, 
                                            "../fox/data/test/test_hero_right_cape.bmp");
        bitmap->torso = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, 
                                            "../fox/data/test/test_hero_right_torso.bmp");
        SetHeroBitmapAlign(bitmap, V2(72, 182));
        bitmap++;

        bitmap->head = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, 
                                            "../fox/data/test/test_hero_back_head.bmp");
        bitmap->cape = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, 
                                            "../fox/data/test/test_hero_back_cape.bmp");
        bitmap->torso = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, 
                                            "../fox/data/test/test_hero_back_torso.bmp");
        SetHeroBitmapAlign(bitmap, V2(72, 182));
        bitmap++;

        bitmap->head = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, 
                                            "../fox/data/test/test_hero_left_head.bmp");
        bitmap->cape = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, 
                                            "../fox/data/test/test_hero_left_cape.bmp");
        bitmap->torso = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, 
                                            "../fox/data/test/test_hero_left_torso.bmp");
        SetHeroBitmapAlign(bitmap, V2(72, 182));
        bitmap++;

        bitmap->head = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, 
                                            "../fox/data/test/test_hero_front_head.bmp");
        bitmap->cape = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, 
                                            "../fox/data/test/test_hero_front_cape.bmp");
        bitmap->torso = DEBUGLoadBMP(thread, memory->debugPlatformReadEntireFile, 
                                            "../fox/data/test/test_hero_front_torso.bmp");
        SetHeroBitmapAlign(bitmap, V2(72, 182));
        
        // (0, 0, 0) is the center of the world!!
        // becauseit's int
        uint32 screenBaseX = 0;
        uint32 screenBaseY = 0;
        uint32 screenBaseZ = 0;
        uint32 screenX = screenBaseX;
        uint32 screenY = screenBaseY;

        uint32 absTileZ = screenBaseZ;

        // Tracker of the random number table
        uint32 randomNumberIndex = 0;

        bool32 doorLeft = false;
        bool32 doorRight = false;
        bool32 doorTop = false;
        bool32 doorBottom = false;
        bool32 doorUp = false;
        bool32 doorDown =false;

        bool32 zDoorCreated = false;
        
        random_series series = Seed(321);
        // For now, one screen is worth one room
        for(uint32 screenIndex = 0;
            screenIndex < 2;
            ++screenIndex)
        {
            uint32 doorDirection = RandomChoice(&series, (doorUp || doorDown) ? 2 : 3);
            // uint32 doorDirection = RandomChoice(&series, 2);
            
            bool32 createdZDoor = false;
            if(doorDirection == 2)
            {
                createdZDoor = true;
                if(absTileZ == 0)
                {
                    doorUp = true;
                }
                else
                {
                    doorDown = true;
                }
            }
            else if(doorDirection == 1)
            {
                doorRight = true;
            }
            else
            {
                doorTop = true;
            }
            
            AddStandardSpace(gameState, 
                            screenX * tilesPerWidth + tilesPerWidth/2,
                            screenY * tilesPerHeight + tilesPerHeight/2,
                            absTileZ);

            for(uint32 tileY = 0;
                tileY < tilesPerHeight;
                ++tileY)
            {

                for(uint32 tileX = 0;
                    tileX < tilesPerWidth;
                    ++tileX)
                {
                    // These values will always increase
                    uint32 absTileX = screenX * tilesPerWidth + tileX;
                    uint32 absTileY = screenY * tilesPerHeight + tileY;

                    uint32 shouldBeDoor = false;

                    if(tileX == 0 && (!doorLeft || (tileY != (tilesPerHeight / 2))))
                    {
                        shouldBeDoor = true;
                    }

                    if(tileX == tilesPerWidth - 1 && (!doorRight || tileY != tilesPerHeight / 2))
                    {
                        shouldBeDoor = true;
                    }

                    if(tileY == 0 && (!doorBottom || tileX != tilesPerWidth / 2))
                    {
                        shouldBeDoor = true;
                    } 
                    
                    if(tileY == tilesPerHeight - 1 && (!doorTop || tileX != tilesPerWidth / 2))
                    {
                        shouldBeDoor = true;
                    }
                    
                    if(shouldBeDoor)
                    {
                        AddWall(gameState, absTileX, absTileY, absTileZ);
                    }
                    else if(createdZDoor)
                    {
                        if((tileX == 10) && (tileY == 6))
                        {
                            if(!zDoorCreated)
                            {
                                AddStair(gameState, absTileX, absTileY - 1, absTileZ);
                                zDoorCreated = true;
                            }
                            // if(doorUp)
                            // {
                            //     tileValue = 3;
                            // }

                            // if(doorDown)
                            // {
                            //     tileValue = 4;
                            // }   
                        }
                    }
                }
            }

            // If the door was OPEN to the left in this tilemap,
            // The door in the next tilemap should be OPEN to the right 
            // Same with the top and bottom
            doorLeft = doorRight;
            doorBottom = doorTop;

            if(createdZDoor)
            {
                doorDown = !doorDown;
                doorUp = !doorUp;
            }
            else
            {
                doorUp = false;
                doorDown = false;
            }

            doorRight = false;
            doorTop = false;

            if(doorDirection == 2)
            {
                if(absTileZ == screenBaseZ)
                {
                    absTileZ = screenBaseZ + 1;
                }
                else
                {
                    absTileZ = screenBaseZ;
                }                
            }
            else if(doorDirection == 1)
            {
                screenX += 1;
            }
            else
            {
                screenY += 1;
            }

        }

        world_position cameraPos = {};
        uint32 cameraTileX = screenBaseX * tilesPerWidth + 17/2;
        uint32 cameraTileY = screenBaseY * tilesPerHeight + 9/2;
        uint32 cameraTileZ = screenBaseZ;
        cameraPos = TilePositionToChunkPosition(gameState->world, cameraTileX, cameraTileY, cameraTileZ);
        gameState->cameraPos = cameraPos;
        
        // AddMonster(gameState, cameraTileX + 4, cameraTileY + 2, cameraTileZ);
        // AddFamiliar(gameState, cameraTileX - 2, cameraTileY + 2, cameraTileZ);

        memory->isInitialized = true;
    }
    
    // Initialize transient state
    Assert(sizeof(transient_state) <= memory->transientStorageSize);
    transient_state *tranState= (transient_state *)memory->transientStorage;
    if(!tranState->isInitialized)
    {
        InitializeArena(&tranState->tranArena, 
                        (memory_index)(memory->transientStorageSize - sizeof(transient_state)),
                        (uint8 *)memory->transientStorage + sizeof(transient_state));                

        tranState->groundBufferCount = 64;
        tranState->groundBuffers = 
            PushArray(&tranState->tranArena, tranState->groundBufferCount, ground_buffer);

        for(uint32 groundIndex = 0;
            groundIndex < tranState->groundBufferCount;
            ++groundIndex)
        {
            ground_buffer *groundBuffer = tranState->groundBuffers + groundIndex;
            groundBuffer->bitmap = MakeEmptyBitmap(&tranState->tranArena, groundBufferWidth, groundBufferHeight, false);
            groundBuffer->pos = NullPosition();
        }

        // FillGroundChunks(tranState, gameState, &tranState->groundBuffers[0], &gameState->cameraPos);
        
        gameState->diff = MakeEmptyBitmap(&tranState->tranArena, gameState->tree.width, gameState->tree.height, false);
        
        gameState->diffNormal = MakeEmptyBitmap(&tranState->tranArena, gameState->diff.width, gameState->diff.height, false);
        MakeSphereNormalMap(&gameState->diffNormal, 0.0f);
        MakeSphereDiffuseMap(&gameState->diff);
        
        tranState->envMapWidth = 512;
        tranState->envMapHeight = 256;
        
        for(int32 mapIndex = 0;
            mapIndex < ArrayCount(tranState->envMaps);
            ++mapIndex)
        {
            enviromnet_map *map = tranState->envMaps + mapIndex;
            int32 width = tranState->envMapWidth;
            int32 height = tranState->envMapHeight;

            for(int32 lodIndex = 0;
                lodIndex < ArrayCount(map->lod);
                ++lodIndex)
            {
                map->lod[lodIndex] = MakeEmptyBitmap(&tranState->tranArena, width, height, false);
                width >>= 1;
                height >>= 1;
            }
        }
        
        tranState->isInitialized = true;
    }

    for(int controllerIndex = 0;
        controllerIndex < ArrayCount(input->controllers);
        ++controllerIndex)
    {
		game_controller *controller = &input->controllers[controllerIndex];
        controlled_hero *conHero = gameState->controlledHeroes + controllerIndex;

        if(conHero->entityIndex == 0)
        {
            if(controller->start.endedDown)
            {
                *conHero = {};
                conHero->entityIndex = AddPlayer(gameState).lowIndex;
            }
        }
        else
        {
            conHero->dZ = 0.0f;
            // player acceleration
            conHero->ddPlayer = {};
            conHero->dSword = {};

            if(controller->isAnalog)
            {
                //The player is using sticks
                conHero->ddPlayer = v2{controller->averageStickX, controller->averageStickY};
            }
            else
            {
                //Use digital movement tuning

                // We are changing the acceleration of the player!!
                // Not the velocity of the player!!
                if(controller->moveUp.endedDown)
                {
                    conHero->ddPlayer.y = 1.0f;
                }
                if(controller->moveDown.endedDown)
                {
                    conHero->ddPlayer.y -= 1.0f;
                }
                if(controller->moveLeft.endedDown)
                {
                    conHero->ddPlayer.x -= 1.0f;
                }
                {
                if(controller->moveRight.endedDown)
                    conHero->ddPlayer.x = 1.0f;
                }
            }   

            if(controller->start.endedDown)
            {
                conHero->dZ = 9.0f;
            }

            if(controller->actionUp.endedDown)
            {
                conHero->dSword = V2(0.0f, 1.0f);
            }
            
            if(controller->actionDown.endedDown)
            {
                conHero->dSword = V2(0.0f, -1.0f);
            }
            if(controller->actionLeft.endedDown)
            {
                conHero->dSword = V2(-1.0f, 0.0f);
            }
            if(controller->actionRight.endedDown)
            {
                conHero->dSword = V2(1.0f, 0.0f);
            }
        }
    }

    //
    // NOTE : Rendering
    //

    temporary_memory renderMemory = BeginTemporaryMemory(&tranState->tranArena);

    loaded_bitmap drawBuffer_ = {};
    loaded_bitmap *drawBuffer = &drawBuffer_;
    drawBuffer->width = buffer->width;
    drawBuffer->height = buffer->height;
    drawBuffer->pitch = buffer->pitch;
    drawBuffer->memory = buffer->memory;

    // TODO : Find out what is the precies maxPushBufferSize is!
    render_group *renderGroup = AllocateRenderGroup(&tranState->tranArena, Megabytes(4), drawBuffer->width, drawBuffer->height);

    // Clear the buffer!
    Clear(renderGroup, V4(0.7f, 0.7f, 0.7f, 0));
    
    rect2 screenBound = GetCameraRectangleAtTarget(renderGroup);
    v2 screenCenter = 0.5f* V2i(drawBuffer->width, drawBuffer->height);

    rect3 cameraBoundsInMeters = RectMinMax(V3(screenBound.min, 0.0f), V3(screenBound.max, 0.0f));
    cameraBoundsInMeters.min.z = -3.0f*gameState->typicalFloorHeight;
    cameraBoundsInMeters.max.z = 1.0f*gameState->typicalFloorHeight;


#if 0
    // NOTE : Render the groundchunks
    for(uint32 groundBufferIndex = 0;
        groundBufferIndex < tranState->groundBufferCount;
        ++groundBufferIndex)
    {
        ground_buffer *groundBuffer = tranState->groundBuffers + groundBufferIndex;
        if(IsValid(groundBuffer->pos))
        {
            loaded_bitmap *bitmap = &groundBuffer->bitmap;
            v3 delta = SubstractTwoWMP(gameState->world, &groundBuffer->pos, &gameState->cameraPos);
            bitmap->alignPercentage = V2(0.5f, 0.5f);

            render_basis *basis = PushStruct(&tranState->tranArena, render_basis);
            renderGroup->defaultBasis = basis;
            basis->pos = delta;

            real32 groundSideInMeters = gameState->world->chunkDimInMeters.x;
            PushBitmap(renderGroup, bitmap, groundSideInMeters, V3(0, 0, 0));
            // TODO : Delete this! This is for the debug!
            PushRectOutline(renderGroup, V3(0, 0, 0), V2(groundSideInMeters, groundSideInMeters), V4(1, 0, 0, 1));
        }
    }
#endif

    // NOTE : Update the groundchunks
    {
        world_position minChunkPos = 
            MapIntoChunkSpace(gameState->world, gameState->cameraPos, GetMinCorner(cameraBoundsInMeters));
        world_position maxChunkPos = 
            MapIntoChunkSpace(gameState->world, gameState->cameraPos, GetMaxCorner(cameraBoundsInMeters));

        for(int32 chunkZ = minChunkPos.chunkZ;
            chunkZ <= maxChunkPos.chunkZ;
            ++chunkZ)
        {
            for(int32 chunkY = minChunkPos.chunkY;
                chunkY <= maxChunkPos.chunkY;
                ++chunkY)
            {
                for(int32 chunkX = minChunkPos.chunkX;
                    chunkX <= maxChunkPos.chunkX;
                    ++chunkX)
                {
                    // world_chunk *chunk = GetWorldChunk(gameState->world, chunkX, chunkY, chunkZ);
                    
                    // if(chunk)
                    {   
                        world_position chunkCenter = CenteredChunkPoint(chunkX, chunkY, chunkZ);
                        v3 relCenter = SubstractTwoWMP(gameState->world, &chunkCenter, &gameState->cameraPos);

                        real32 furthestBufferDistanceSq = 0.0f;
                        ground_buffer *furthestBuffer = 0;
                        for(uint32 groundChunkIndex = 0;
                            groundChunkIndex < tranState->groundBufferCount;
                            ++groundChunkIndex)
                        {
                            ground_buffer *groundBuffer = tranState->groundBuffers + groundChunkIndex;

                            if(AreInSameChunk(gameState->world, &groundBuffer->pos, &chunkCenter))
                            {
                                // We found the exact chunk, no need to do anything!
                                // NOTE : We already found a groundbuffer that is already assigned
                                // for this chunk!
                                furthestBuffer = 0;
                                break;
                            }
                            else if(IsValid(groundBuffer->pos))
                            {
                                // Where is the chunkCenter relative to the camera?
                                // NOTE : This is in meters        
                                v3 bufferRelPos = 
                                    SubstractTwoWMP(gameState->world, &groundBuffer->pos, &gameState->cameraPos);
                                real32 bufferLengthSq = LengthSq(bufferRelPos);
                                if(furthestBufferDistanceSq < bufferLengthSq)
                                {
                                    furthestBufferDistanceSq = bufferLengthSq;
                                    furthestBuffer = groundBuffer;
                                }
                            }
                            else
                            {
                                // NOTE : Even if we have found the groundbuffer that was furthest from the camera,
                                // if there is an assigned buffer, we would rather use that one! 
                                furthestBufferDistanceSq = Real32Max;
                                furthestBuffer = groundBuffer;
                            }
                        }

                        // NOTE : If we didn't found it but found a unassigned buffer, use that one!
                        if(furthestBuffer)
                        {
                            FillGroundChunks(tranState, gameState, furthestBuffer, &chunkCenter);
                        }
                    }
                }
            }
        }
    }

    // TODO : How big do we actually want to expand here?
    // TODO : Do we want to simulate the upper floor too?
    v3 simBoundsExpansion = V3(15.0f, 15.0f, 0.0f);
    // The center is (0, 0) because the cameraPos is (0, 0)!!
    rect3 simBounds = AddRadiusToRect(cameraBoundsInMeters, simBoundsExpansion);
    temporary_memory simMemory = BeginTemporaryMemory(&tranState->tranArena);
    world_position simCenterPos = gameState->cameraPos;
    sim_region *simRegion = 
        BeginSim(&tranState->tranArena, 
                gameState, gameState->world, 
                simCenterPos, simBounds, 
                input->dtForFrame);


    // TODO : Purely for the debugging purpose! Not a good API>> clean this up!
    render_basis *debugBasis = PushStruct(&tranState->tranArena, render_basis);
    debugBasis->pos = V3(0, 0, 0);
    renderGroup->defaultBasis = debugBasis;
    // PushRectOutline(renderGroup, V3(0, 0, 0), GetDim(screenBound), V4(1.0f, 0.7f, 0.0f, 1.0f));

    v3 cameraPos = SubstractTwoWMP(gameState->world, &gameState->cameraPos, &simCenterPos);
    
    for(uint32 entityIndex = 0;
        entityIndex < simRegion->entityCount;
        ++entityIndex)
    {
        sim_entity *entity = simRegion->entities + entityIndex;
    
        if(entity->updatable)
        {
            // TODO : This is incorrect, should be computed after update!!!!
            real32 shadowAlpha = 1.0f - 0.5f*entity->pos.z;
            if(shadowAlpha < 0)
            {
                shadowAlpha = 0.0f;
            }

            move_spec moveSpec = DefaultMoveSpec();
            v3 ddP = {};

            render_basis *basis = PushStruct(&tranState->tranArena, render_basis);
            basis->pos = GetEntityGroundPoint(entity);
            
            // Set this to basis so that when we PushPiece, the entity basis will be set to the basis
            // because we are setting the piece->basis = group->defaultBasis
            // even if we don't come to this scope, because the defaultBasis is (0, 0, 0), it does not matter
            // in pushpiece call.
            renderGroup->defaultBasis = basis;
            
            hero_bitmaps *heroBitmaps = &gameState->heroBitmaps[entity->facingDirection];

            v3 cameraRelativeGroundPos = GetEntityGroundPoint(entity) - cameraPos;
            // NOTE : This is written in Z order
            real32 fadeTopEndZ = 1.0f*gameState->typicalFloorHeight;
            real32 fadeTopStartZ = 0.5f*gameState->typicalFloorHeight;
            real32 fadeBottomStartZ = -2.0f*gameState->typicalFloorHeight;
            real32 fadeBottomEndZ = -2.25f*gameState->typicalFloorHeight;

            if(cameraRelativeGroundPos.z > fadeTopStartZ)
            {
                // because the direction of increase is different, we need to change the start and end values
                renderGroup->globalAlpha = Clamp01MapInRange(fadeTopEndZ, cameraRelativeGroundPos.z, fadeTopStartZ);
            }
            else if(cameraRelativeGroundPos.z < fadeBottomStartZ)
            {
                renderGroup->globalAlpha = Clamp01MapInRange(fadeBottomEndZ, cameraRelativeGroundPos.z, fadeBottomStartZ);
            }
            renderGroup->globalAlpha = Clamp01(1.5f - cameraRelativeGroundPos.z);

            switch(entity->type)
            {
                case EntityType_Hero:
                {
                    for(uint32 controlIndex = 0;
                        controlIndex < ArrayCount(gameState->controlledHeroes);
                        ++controlIndex)
                    {
                        controlled_hero *conHero = gameState->controlledHeroes + controlIndex;

                        // Find out which controller is controlling this entity, 
                        // so that we don't process this entity multiple times
                        if(entity->storageIndex == conHero->entityIndex)
                        {
                            if(conHero->dZ != 0.0f)
                            {
                                entity->dPos.z = conHero->dZ;
                            }

                            moveSpec.unitMaxAccelVector = true;
                            moveSpec.speed = 50.0f;
                            moveSpec.drag = 8.0f;
                            ddP = V3(conHero->ddPlayer, 0);

                            if(conHero->dSword.x != 0.0f || conHero->dSword.y != 0.0f)
                            {
                                sim_entity *sword = entity->sword.ptr;
                            
                                sword->distanceLimit = 5.0f;
                                MakeEntitySpatial(sword, 
                                                entity->pos, 
                                                entity->dPos + 5.0f * V3(conHero->dSword, 0));

                                AddCollisionRule(gameState, entity->storageIndex, sword->storageIndex, false);
                            }
                        }
                    }

                    real32 heroSizeC = 2.0f;

                    PushBitmap(renderGroup, &gameState->shadow, 0.5f, V3(0, 0, 0), V4(1, 1, 1, shadowAlpha));                
                    PushBitmap(renderGroup, &heroBitmaps->torso, heroSizeC*1.4f, V3(0, 0, 0));
                    PushBitmap(renderGroup, &heroBitmaps->cape, heroSizeC*1.4f, V3(0, 0, 0));                
                    PushBitmap(renderGroup, &heroBitmaps->head, heroSizeC*1.4f, V3(0, 0, 0));

                    DrawHitpoints(entity, renderGroup);
                }break;

                case EntityType_Sword:
                {
                    moveSpec.unitMaxAccelVector = false;
                    moveSpec.speed = 50.0f;
                    moveSpec.drag = 0.0f;
                    ddP = V3(0, 0, 0);

                    if(entity->distanceLimit <= 0.0f)
                    {
                        MakeEntityNonSpatial(entity);
                        // When we make the sword disapper, make it
                        ClearCollisionRulesFor(gameState, entity->storageIndex);
                    }

                    PushBitmap(renderGroup, &gameState->shadow, 1.4f, V3(0, 0, 0), V4(1, 1, 1, shadowAlpha));
                    PushBitmap(renderGroup, &gameState->sword, 1.4f, V3(0, 0, 0));
                    
                }break;

                case EntityType_Wall:
                {
                    PushBitmap(renderGroup, &gameState->shadow, 2.5f, V3(0, 0, 0), V4(1, 1, 1, shadowAlpha));                
                    PushBitmap(renderGroup, &gameState->tree, 2.5f, V3(0, 0, 0));
                }break;

                case EntityType_Monster:
                {

                }break;
                
                case EntityType_Space:
                {

                    for(uint32 volumeIndex = 0;
                        volumeIndex < entity->collision->volumeCount;
                        volumeIndex++)
                    {
                        sim_entity_collision_volume *volume = entity->collision->volumes + volumeIndex;
                        //PushRectOutline(renderGroup, V3(volume->offset.xy, 0), volume->dim.xy, V4(0.3f, 0.3f, 0.9f, 1));  
                    }
                }break;

                case EntityType_Stairwell:
                {
                    PushRect(renderGroup, V3(0, 0, 0), entity->walkableDim, V4(1, 1, 0, 1));
                    PushRect(renderGroup, V3(0, 0, entity->walkableHeight), entity->walkableDim, V4(1, 1, 0, 1));
                    
                }break;

                case EntityType_Familiar:
                {
                }break;

                default:
                {
                    InvalidCodePath;
                }
            }


            if(!IsSet(entity, EntityFlag_Nonspatial) && IsSet(entity, EntityFlag_Movable))
            {
                MoveEntity(gameState, simRegion, entity, input->dtForFrame, &moveSpec, ddP);
            }

        }
    }

#if 0
    {
        int32 checkerWidth = 16;
        int32 checkerHeight = 16;
        v4 mapColor[] = 
        {
            {1, 0, 0, 1},
            {0, 1, 0, 1},
            {0, 0, 1, 1},
        };
        for(uint32 mapIndex = 0;
            mapIndex < ArrayCount(tranState->envMaps);
            ++mapIndex)
        {
            loaded_bitmap *lod = tranState->envMaps[mapIndex].lod + 0;
            // TODO : This name must be changed?
            bool32 shouldBeColor = true;
            for(int32 y = 0;
                y < lod->height;
                y += checkerHeight)
            {
                if(!shouldBeColor)
                {
                    shouldBeColor = true;
                }
                else
                {
                    shouldBeColor = false;                    
                }
                for(int32 x = 0;
                    x < lod->width;
                    x += checkerWidth)
                {    
                    v2 minPos = V2i(x, y);
                    v2 maxPos = minPos + V2i(checkerWidth, checkerHeight);

                    v4 color = shouldBeColor ? mapColor[mapIndex] : V4(0, 0, 0, 1);
                    DrawRectangle(lod, minPos, maxPos, color);
                    shouldBeColor = !shouldBeColor;
                }
            }
        }
        
        tranState->envMaps[0].pZ = -1.5f;
        tranState->envMaps[1].pZ = 0.0f;
        tranState->envMaps[2].pZ = 1.5f;
        
        gameState->time += input->dtForFrame;
        
        real32 angle = 0.2f*gameState->time;
        v2 disp = {100.0f*Cos(5.0f*angle),
                    100.0f*Sin(3.0f*angle)};

        v2 origin = screenCenter;

#if 1
        v2 xAxis = 100.0f*V2(Cos(10.0f*angle), Sin(10.0f*angle));
        v2 yAxis = V2(-xAxis.y, xAxis.x);
#else
        v2 xAxis = V2(300.0f, 0.0f);
        v2 yAxis = V2(0.0f, 300.0f);
#endif
        
#if 0
        // Adding 0.5f* and multiplying 0.5f* to keep the values between 0.0f and 1.0f
        v4 color = V4(0.5f+0.5f*Cos(angle), 0.5f+0.5f*Cos(5.5f*angle), 0.5f+0.5f*Sin(3.5f*angle), 0.5f+0.5f*Cos(2.5f*angle));
#else
        v4 color = V4(1.0f, 1.0f, 1.0f, 1.0f);
#endif

        render_group_entry_coordinate_system *c = 
            PushCoordinateSystem(renderGroup, &gameState->diff, 
                                origin - 0.5f*xAxis - 0.5f*yAxis + disp, xAxis, yAxis, 
                                color,
                                &gameState->diffNormal, 
                                tranState->envMaps + 0, 
                                tranState->envMaps + 1, 
                                tranState->envMaps + 2);
        
        // NOTE : Drawing Envmaps for debugging purpose!
        v2 mapPos = {0.0f, 5.0f};
        for(int32 mapIndex = 0;
            mapIndex < ArrayCount(tranState->envMaps);
            ++mapIndex)
        {
            enviromnet_map *map = tranState->envMaps + mapIndex;
            loaded_bitmap *lod = &map->lod[0];
            
            xAxis = 0.5f*V2i(lod->width, 0);
            yAxis = 0.5f*V2i(0, lod->height);

            PushCoordinateSystem(renderGroup, lod, 
                                mapPos, xAxis, yAxis, V4(1.0f, 1.0f, 1.0f, 1.0f),
                                0, 0, 0, 0);

            mapPos += yAxis + V2(0.0f, 6.0f);
        }
        // uint32 pIndex = 0;
        // for(real32 x = 0.0f;
        //     x < 1.0f;
        //     x += 0.25f)
        // {
        //     for(real32 y = 0.0f;
        //         y < 1.0f;
        //         y += 0.25f)
        //     {
        //         c->points[pIndex++] = V2(x, y);
        //     }
        // }
    }
    
#endif
    RenderGroupToOutputBuffer(renderGroup, drawBuffer);

    EndSim(simRegion, gameState);
    EndTemporaryMemory(simMemory);
    EndTemporaryMemory(renderMemory);    
    
    CheckArena(&gameState->worldArena);
    CheckArena(&tranState->tranArena);

    END_TIMED_BLOCK(GameUpdateAndRender);
}

extern "C" GAME_GET_SOUND_SAMPLES(GameGetSoundSamples)
{
    game_state *gameState = (game_state *)memory->permanentStorage;
    GameOutputSound(gameState, soundBuffer);

}
