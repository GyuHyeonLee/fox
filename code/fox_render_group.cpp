#include "fox_render_group.h"

struct bilinear_sample
{
    uint32 a, b, c, d;
};

inline v4
SRGB255ToLinear1(v4 color)
{
    v4 result;

    // NOTE : because the input value is in 255 space,
    // convert it to be in linear 1 space
    real32 inv255 = 1.0f/255.0f;

    result.r = Square(inv255*color.r);
    result.g = Square(inv255*color.g);
    result.b = Square(inv255*color.b);
    // NOTE : alpha is not a part of this operation!
    // because it just means how much should we blend
    result.a = inv255*color.a;

    return result;
}

inline v4
Linear1ToSRGB255(v4 c)
{
    v4 result;

    result.r = 255.0f*Root2(c.r);
    result.g = 255.0f*Root2(c.g);
    result.b = 255.0f*Root2(c.b);
    result.a = 255.0f*c.a;

    return result;
}

inline v4
Unpack4x8(uint32 packed)
{
    v4 result = {(real32)((packed >> 16) & 0xFF),
                (real32)((packed >> 8) & 0xFF),
                (real32)((packed >> 0) & 0xFF),
                (real32)((packed >> 24) & 0xFF)};

    return result;
}

inline v4
UnscaleAndBiasNormal(v4 normal)
{
    v4 result;

    real32 inv255 = 1.0f / 255.0f;

    result.x = -1.0f + 2.0f*(inv255*normal.x);
    result.y = -1.0f + 2.0f*(inv255*normal.y);
    result.z = -1.0f + 2.0f*(inv255*normal.z);

    result.w = inv255*normal.w;

    return result;
}

inline v4
SRGBBilinearBlend(bilinear_sample sample, real32 fX, real32 fY)
{
    v4 pixel0 = Unpack4x8(sample.a);
    v4 pixel1 = Unpack4x8(sample.b);
    v4 pixel2 = Unpack4x8(sample.c);
    v4 pixel3 = Unpack4x8(sample.d);

    pixel0 = SRGB255ToLinear1(pixel0);
    pixel1 = SRGB255ToLinear1(pixel1);
    pixel2 = SRGB255ToLinear1(pixel2);
    pixel3 = SRGB255ToLinear1(pixel3);

    v4 result = Lerp(Lerp(pixel0, fX, pixel1), fY, Lerp(pixel2, fX, pixel3));

    return result;
}

inline bilinear_sample
BilinearSample(loaded_bitmap *texture, int x, int y)
{
    bilinear_sample result;

    uint8 *texelPtr = ((uint8 *)texture->memory +
                        x*sizeof(uint32) +
                        y*texture->pitch);

    // Get all 4 texels around the texelX and texelY
    result.a = *(uint32 *)(texelPtr);
    result.b = *(uint32 *)(texelPtr + sizeof(uint32));
    result.c = *(uint32 *)(texelPtr + texture->pitch);
    result.d = *(uint32 *)(texelPtr + texture->pitch + sizeof(uint32));

    return result;
}

inline v3
SampleEnvironmentMap(v2 screenSpaceUV, v3 sampleDirection, real32 roughness,
                    enviromnet_map *map, real32 distanceFromMapInZ)
{
    /* NOTE :

       ScreenSpaceUV tells us where the ray is being cast _from_ in
       normalized screen coordinates.

       SampleDirection tells us what direction the cast is going -
       it does not have to be normalized.

       Roughness says which LODs of Map we sample from.

       DistanceFromMapInZ says how far the map is from the sample point in Z, given
       in meters.
    */

    // NOTE : Pick which LOD to sample from
    uint32 lodIndex = (uint32)(roughness * (real32)(ArrayCount(map->lod) - 1) + 0.5f);
    Assert(lodIndex < ArrayCount(map->lod));

    loaded_bitmap *lod = &map->lod[lodIndex];

    // NOTE  Compute the distance to the map and the scaling
    // factor for meters to UVs
    real32 uvsPerMeter = 0.01f;
    real32 c = (uvsPerMeter*distanceFromMapInZ) / sampleDirection.y;
    v2 offset = c * V2(sampleDirection.x, sampleDirection.z);

    // NOTE : Find the intersection point
    v2 uv = screenSpaceUV + offset;

    uv.x = Clamp01(uv.x);
    uv.y = Clamp01(uv.y);

    // NOTE : Bilinear sample
    real32 tX = (uv.x*(real32)(lod->width - 2));
    real32 tY = (uv.y*(real32)(lod->height - 2));

    int32 x = (int32)tX;
    int32 y = (int32)tY;

    real32 fX = tX - (real32)x;
    real32 fY = tY - (real32)y;

#if 0
    // NOTE : Turn this on to see where in the map you're sampling!
    uint8 *TexelPtr = ((uint8 *)LOD->Memory) + Y*LOD->Pitch + X*sizeof(uint32);
    *(uint32 *)TexelPtr = 0xFFFFFFFF;
#endif

    bilinear_sample sample = BilinearSample(lod, x, y);
    v3 result = SRGBBilinearBlend(sample, fX, fY).xyz;

    return result;
}

internal void
DrawRectangle(loaded_bitmap *buffer, v2 realMin, v2 realMax, v4 color)
{
    real32 r = color.r;
    real32 g = color.g;
    real32 b = color.b;
    real32 a = color.a;

    //Because we are going to display to the screen
    // It should be in int(pixel)
    int32 minX = RoundReal32ToInt32(realMin.x);
    int32 minY = RoundReal32ToInt32(realMin.y);
    int32 maxX = RoundReal32ToInt32(realMax.x);
    int32 maxY = RoundReal32ToInt32(realMax.y);

    //buffer overflow protection
    if(minX < 0)
    {
        minX = 0;
    }
    if(minY < 0)
    {
        minY = 0;
    }
    if(maxX > buffer->width)
    {
        maxX = buffer->width;
    }
    if(maxY > buffer->height)
    {
        maxY = buffer->height;
    }

    uint32 color32 = ((RoundReal32ToInt32(a * 255.0f) << 24) |
                    (RoundReal32ToInt32(r * 255.0f) << 16) |
                    (RoundReal32ToInt32(g * 255.0f) << 8) |
                    (RoundReal32ToInt32(b * 255.0f) << 0));

    uint8 *row = ((uint8 *)buffer->memory +
                    minX * BITMAP_BYTES_PER_PIXEL +
                    minY * buffer->pitch);
    for(int y = minY;
        y < maxY;
        ++y)
    {
        uint32 *pixel = (uint32 *)row;
        for(int x = minX;
            x < maxX;
            ++x)
        {
            *pixel++ = color32;
        }
        row += buffer->pitch;
    }
}

internal void
DrawRectangleOutline(loaded_bitmap *buffer, v2 realMin, v2 realMax, v4 color)
{
    // NOTE : This is of course in pixels
    real32 thickness = 2.0f;

    real32 width = realMax.x - realMin.x;
    real32 height = realMax.y - realMin.y;

    // Left
    DrawRectangle(buffer, realMin, realMin + V2(thickness, height), color);
    // Right
    DrawRectangle(buffer, realMax - V2(thickness, height), realMax, color);
    // Top
    DrawRectangle(buffer, realMin, realMin + V2(width, thickness), color);
    // Bottom
    DrawRectangle(buffer, realMax - V2(width, thickness), realMax, color);
}

// TODO : Make this using simd!
internal void
DrawSomethingHopefullyFast(loaded_bitmap *buffer, v2 origin, v2 xAxis, v2 yAxis, v4 color,
                    loaded_bitmap *texture, loaded_bitmap *normalMap,
                    enviromnet_map *top,
                    enviromnet_map *middle,
                    enviromnet_map *bottom)
{
    BEGIN_TIMED_BLOCK(DrawSomethingHopefullyFast);

    // NOTE : Premulitplied color alpha!
    color.rgb *= color.a;

    real32 xAxisLength = Length(xAxis);
    real32 yAxisLength = Length(yAxis);

    real32 invXAxisSquare = 1.0f/LengthSq(xAxis);
    real32 invYAxisSquare = 1.0f/LengthSq(yAxis);

    // Normalized axises
    v2 nxAxis = invXAxisSquare*xAxis;
    v2 nyAxis = invYAxisSquare*yAxis;

    int32 widthMax = buffer->width - 1;
    int32 heightMax = buffer->height - 1;

    // NOTE : because the input value is in 255 space,
    // convert it to be in linear 1 space
    real32 inv255 = 1.0f/255.0f;
    real32 one255 = 255.0f;

    // Setting these to as low or high as they can so that we can modify
    int minX = widthMax;
    int minY = heightMax;
    int maxX = 0;
    int maxY = 0;

    // Buffer boundary checking
    v2 boundaryPoints[4] = {origin, origin+xAxis, origin+yAxis, origin+xAxis+yAxis};
    for(uint32 pointIndex = 0;
        pointIndex < ArrayCount(boundaryPoints);
        ++pointIndex)
    {
        v2 *testPoint = boundaryPoints + pointIndex;
        int32 floorX = FloorReal32ToInt32(testPoint->x);
        int32 ceilX = CeilReal32ToInt32(testPoint->x);
        int32 floorY = FloorReal32ToInt32(testPoint->y);
        int32 ceilY = CeilReal32ToInt32(testPoint->y);

        if(minX > floorX) {minX = floorX;}
        if(minY > floorY) {minY = floorY;}
        if(maxX < ceilX) {maxX = ceilX;}
        if(maxY < ceilY) {maxY = ceilY;}
    }

    if(minX < 0)
    {
        minX = 0;
    }
    if(minY < 0)
    {
        minY = 0;
    }
    if(maxX > buffer->width)
    {
        maxX = buffer->width;
    }
    if(maxY > buffer->height)
    {
        maxY = buffer->height;
    }

    uint8 *row = ((uint8 *)buffer->memory +
                            minX * BITMAP_BYTES_PER_PIXEL +
                            minY * buffer->pitch);

    for(int y = minY;
        y < maxY;
        ++y)
    {
        uint32 *pixel = (uint32 *)row;

        for(int x = minX;
            x < maxX;
            x += 4)
        {
            BEGIN_TIMED_BLOCK(TestPixel);
            
            for(int pIndex = 0;
                pIndex < 4;
                ++pIndex)
            {
                v2 pixelPos = V2i(x, y);
                // pixelPos based on the origin
                v2 basePos = pixelPos - origin;

                // Transform to the u-v coordinate system to get the bitmap based pixels!
                // First of all, we need to divide the value with the legnth of the axis
                // to make the axis unit length
                // Second, we need to divdie with the length of the axis AGAIN
                // because we need the coordinate to be matched to the normalized axis!
                real32 u = Inner(basePos, nxAxis);
                real32 v = Inner(basePos, nyAxis);

                // We can test whether the pixel is inside the texture or not with this test
                if((u >= 0.0f) && (u <= 1.0f) && 
                    (v >= 0.0f) && (v <= 1.0f))
                {
                    BEGIN_TIMED_BLOCK(FillPixel);

                    // TODO : Put this back to the original thing!
                    real32 texelX= ((u*(real32)(texture->width - 2)));
                    real32 texelY = ((v*(real32)(texture->height - 2)));

                    // What pixel should we use in the bitmap?
                    // NOTE : x and y in texture in integer value
                    int32 texelPixelX = (int32)(texelX);
                    int32 texelPixelY = (int32)(texelY);

                    real32 fX = texelX - (real32)texelPixelX;
                    real32 fY = texelY - (real32)texelPixelY;

                    // NOTE : Get(Sample) 4 texels around the target texel
                    uint8 *texelPtr = ((uint8 *)texture->memory + 
                                        texelPixelX*sizeof(uint32) + 
                                        texelPixelY*texture->pitch);

                    // Get all 4 texels around the texelX and texelY
                    uint32 sampleA = *(uint32 *)(texelPtr);
                    uint32 sampleB = *(uint32 *)(texelPtr + sizeof(uint32));
                    uint32 sampleC = *(uint32 *)(texelPtr + texture->pitch);
                    uint32 sampleD = *(uint32 *)(texelPtr + texture->pitch + sizeof(uint32));

                    // NOTE : Unpack texels
                    real32 texelAr = (real32)((sampleA >> 16) & 0xFF);
                    real32 texelAg = (real32)((sampleA >> 8) & 0xFF);
                    real32 texelAb = (real32)((sampleA >> 0) & 0xFF);
                    real32 texelAa = (real32)((sampleA >> 24) & 0xFF);

                    real32 texelBr = (real32)((sampleB >> 16) & 0xFF);
                    real32 texelBg = (real32)((sampleB >> 8) & 0xFF);
                    real32 texelBb = (real32)((sampleB >> 0) & 0xFF);
                    real32 texelBa = (real32)((sampleB >> 24) & 0xFF);

                    real32 texelCr = (real32)((sampleC >> 16) & 0xFF);
                    real32 texelCg = (real32)((sampleC >> 8) & 0xFF);
                    real32 texelCb = (real32)((sampleC >> 0) & 0xFF);
                    real32 texelCa = (real32)((sampleC >> 24) & 0xFF);

                    real32 texelDr = (real32)((sampleD >> 16) & 0xFF);
                    real32 texelDg = (real32)((sampleD >> 8) & 0xFF);
                    real32 texelDb = (real32)((sampleD >> 0) & 0xFF);
                    real32 texelDa = (real32)((sampleD >> 24) & 0xFF);

                    // NOTE : Convert pixels from 255 space to linear 1 space
                    texelAr = Square(inv255*texelAr);
                    texelAg = Square(inv255*texelAg);
                    texelAb = Square(inv255*texelAb);
                    texelAa = inv255*texelAa;

                    texelBr = Square(inv255*texelBr);
                    texelBg = Square(inv255*texelBg);
                    texelBb = Square(inv255*texelBb);
                    texelBa = inv255*texelBa;
                    
                    texelCr = Square(inv255*texelCr);
                    texelCg = Square(inv255*texelCg);
                    texelCb = Square(inv255*texelCb);
                    texelCa = inv255*texelCa;

                    texelDr = Square(inv255*texelDr);
                    texelDg = Square(inv255*texelDg);
                    texelDb = Square(inv255*texelDb);
                    texelDa = inv255*texelDa;

                    // NOTE : Leap so that we can get 4 texels blended.
                    // Bilinear texture blend
                    real32 invfX = 1.0f - fX;
                    real32 invfY = 1.0f - fY;

                    real32 l0 = invfX*invfY;
                    real32 l1 = invfY*fX;
                    real32 l2 = fY*invfX;
                    real32 l3 = fY*fX;

                    real32 texelr = l0*texelAr + l1*texelBr + l2*texelCr + l3*texelDr;
                    real32 texelg = l0*texelAg + l1*texelBg + l2*texelCg + l3*texelDg;
                    real32 texelb = l0*texelAb + l1*texelBb + l2*texelCb + l3*texelDb;
                    real32 texela = l0*texelAa + l1*texelBa + l2*texelCa + l3*texelDa;

                    // NOTE(casey): Modulate by incoming color
                    texelr = texelr*color.r;
                    texelg = texelg*color.g;
                    texelb = texelb*color.b;
                    texela = texela*color.a;

                    // NOTE : Clamp colors to valid range
                    texelr = Clamp01(texelr);
                    texelg = Clamp01(texelg);
                    texelb = Clamp01(texelb);

                    // NOTE : Get the destination pixel from the buffer
                    real32 destr = (real32)((*pixel >> 16) & 0xFF);
                    real32 destg = (real32)((*pixel >> 8) & 0xFF);
                    real32 destb = (real32)((*pixel >> 0) & 0xFF);
                    real32 desta = (real32)((*pixel >> 24) & 0xFF);

                    // NOTE : RGB to linear 1 space
                    destr = Square(inv255*destr);
                    destg = Square(inv255*destg);
                    destb = Square(inv255*destb);
                    desta = inv255*desta;

                    // NOTE : Destination blend
                    real32 invTexelA = (1.0f - texela);
                    real32 blendedr = invTexelA*destr + texelr;
                    real32 blendedg = invTexelA*destg + texelg;
                    real32 blendedb = invTexelA*destb + texelb;
                    real32 blendeda = (texela + desta - texela*desta);

                    // NOTE : RGB to linear 1 space
                    real32 resultr = one255*Root2(blendedr);
                    real32 resultg = one255*Root2(blendedg);
                    real32 resultb = one255*Root2(blendedb);
                    real32 resulta = one255*blendeda;

                    // NOTE : Put it back as a, r, g, b order
                    *pixel = (((uint32)(resulta + 0.5f) << 24) |
                            ((uint32)(resultr + 0.5f) << 16) |
                            ((uint32)(resultg + 0.5f) << 8) |
                            ((uint32)(resultb + 0.5f) << 0));

                    pixel++;
                    END_TIMED_BLOCK(FillPixel);
                }
            }

            // We could not use *pixel++ as we did because
            // we are performing some tests against pixels!

            // pixel += 4;

            END_TIMED_BLOCK(TestPixel);
        }

        row += buffer->pitch;
    }

    END_TIMED_BLOCK(DrawSomethingHopefullyFast);
}

internal void
DrawSomethingSlowly(loaded_bitmap *buffer, v2 origin, v2 xAxis, v2 yAxis, v4 color,
                    loaded_bitmap *texture, loaded_bitmap *normalMap,
                    enviromnet_map *top,
                    enviromnet_map *middle,
                    enviromnet_map *bottom,
                    real32 pixelsToMeters)
{
    BEGIN_TIMED_BLOCK(DrawSomethingSlowly);

    // NOTE : Premulitplied color alpha!
    color.rgb *= color.a;

    real32 xAxisLength = Length(xAxis);
    real32 yAxisLength = Length(yAxis);

    real32 invXAxisSquare = 1.0f/LengthSq(xAxis);
    real32 invYAxisSquare = 1.0f/LengthSq(yAxis);

    v2 nxAxis = (yAxisLength / xAxisLength) * xAxis;
    v2 nyAxis = (xAxisLength / yAxisLength) * yAxis;

    // NOTE : NzScale could be a parameter if we want people to
    // have control over the amount of scaling in the Z direction
    // that the normals appear to have.
    real32 nzScale = 0.5f*(xAxisLength + yAxisLength);

    int32 widthMax = buffer->width - 1;
    int32 heightMax = buffer->height - 1;

    // TODO : This will need to be specified separately!!
    real32 originZ = 0.0f;
    real32 originY = (origin + 0.5f*xAxis + 0.5f*yAxis).y;
    real32 fixedCastY = originY / heightMax;

    uint32 color32 = ((RoundReal32ToInt32(color.a * 255.0f) << 24) |
                    (RoundReal32ToInt32(color.r * 255.0f) << 16) |
                    (RoundReal32ToInt32(color.g * 255.0f) << 8) |
                    (RoundReal32ToInt32(color.b * 255.0f) << 0));

    // Setting these to as low or high as they can so that we can modify
    int minX = widthMax;
    int minY = heightMax;
    int maxX = 0;
    int maxY = 0;

    v2 points[4] = {origin, origin+xAxis, origin+yAxis, origin+xAxis+yAxis};
    for(uint32 pointIndex = 0;
        pointIndex < ArrayCount(points);
        ++pointIndex)
    {
        v2 testPoint = points[pointIndex];
        int32 floorX = FloorReal32ToInt32(testPoint.x);
        int32 ceilX = CeilReal32ToInt32(testPoint.x);
        int32 floorY = FloorReal32ToInt32(testPoint.y);
        int32 ceilY = CeilReal32ToInt32(testPoint.y);

        if(minX > floorX) {minX = floorX;}
        if(minY > floorY) {minY = floorY;}
        if(maxX < ceilX) {maxX = ceilX;}
        if(maxY < ceilY) {maxY = ceilY;}
    }

    if(minX < 0)
    {
        minX = 0;
    }
    if(minY < 0)
    {
        minY = 0;
    }
    if(maxX > buffer->width)
    {
        maxX = buffer->width;
    }
    if(maxY > buffer->height)
    {
        maxY = buffer->height;
    }

    uint8 *row = ((uint8 *)buffer->memory +
                            minX * BITMAP_BYTES_PER_PIXEL +
                            minY * buffer->pitch);

    for(int y = minY;
        y < maxY;
        ++y)
    {
        uint32 *pixel = (uint32 *)row;
        for(int x = minX;
            x < maxX;
            ++x)
        {

#if 1
            v2 pixelPos = V2i(x, y);
            // pixelPos based on the origin
            v2 basePos = pixelPos - origin;

            // The checking positions are all different because
            // the positions should have same origins with the edges that are checking against.
            // which means, they should be in same space!
            // Also, we need to consider clockwise to get the origin.
            // For example, the origin of the upper edge should be V2(xAxis, yAxis)
            real32 edge0 = Inner(basePos, -yAxis);
            real32 edge1 = Inner(basePos - xAxis, xAxis);
            real32 edge2 = Inner(basePos - xAxis - yAxis, yAxis);
            real32 edge3 = Inner(basePos - yAxis, -xAxis);

            if((edge0 < 0) && (edge1 < 0) && (edge2 < 0) && (edge3 < 0))
            {
                // NOTE : This is for the card like normal, and the y value
                // should be the fixed value for each card
                v2 screenSpaceUV = {(real32)x/widthMax, fixedCastY};
                real32 zDiff = pixelsToMeters * ((real32)y - originY);
#if 1
                // Transform to the u-v coordinate system to get the bitmap based pixels!
                // First of all, we need to divide the value with the legnth of the axis
                // to make the axis unit length
                // Second, we need to divdie with the length of the axis AGAIN
                // because we need the coordinate to be matched to the normalized axis!
                real32 u = invXAxisSquare*Inner(basePos, xAxis);
                real32 v = invYAxisSquare*Inner(basePos, yAxis);

                // TODO(casey): SSE clamping.
                //Assert(u >= 0.0f && u <= 1.0f);
                //Assert(v >= 0.0f && v <= 1.0f);

                // NOTE : x and y in texture in floating point value.
                // real32 texelX = (u*(real32)(texture->width - 1) + 0.5f);
                // real32 texelY = (v*(real32)(texture->height - 1) + 0.5f);

                // TODO : Put this back to the original thing!
                real32 texelX= ((u*(real32)(texture->width - 2)));
                real32 texelY = ((v*(real32)(texture->height - 2)));

                // What pixel should we use in the bitmap?
                // NOTE : x and y in texture in integer value
                int32 texelPixelX = (int32)(texelX);
                int32 texelPixelY = (int32)(texelY);

                real32 fX = texelX - (real32)texelPixelX;
                real32 fY = texelY - (real32)texelPixelY;

                bilinear_sample texelSample = BilinearSample(texture, texelPixelX, texelPixelY);
                v4 texel = SRGBBilinearBlend(texelSample, fX, fY);

                if(normalMap)
                {
                    bilinear_sample normalSample = BilinearSample(normalMap, texelPixelX, texelPixelY);

                    v4 normal0 = Unpack4x8(normalSample.a);
                    v4 normal1 = Unpack4x8(normalSample.b);
                    v4 normal2 = Unpack4x8(normalSample.c);
                    v4 normal3 = Unpack4x8(normalSample.d);

                    v4 normal = Lerp(Lerp(normal0, fX, normal1), fY, Lerp(normal2, fX, normal3));

                    // NOTE : Because normal is 255 space, put it back to -101 space
                    normal = UnscaleAndBiasNormal(normal);

                    // Because this normal axis is based on the xAxis and yAxis,
                    // recompute it based on those axises
                    normal.xy = normal.x*nxAxis + normal.y*nyAxis;
                    // This is not a 100% correct value, but it does the job.
                    normal.z *= nzScale;
                    normal.xyz = Normalize(normal.xyz);

                    // e^T * N * N means n direction vector wich size of e transposed to N
                    // The equation below is the simplified version of -e + 2e^T*N*N where e is eyevector 0, 0, 1
                    // because the x and y component of eyeVector is 0, e dot N is normal.z!
                    v3 bounceDirection = 2.0f*normal.z*normal.xyz;
                    bounceDirection.z -= 1.0f;

                    bounceDirection.z = -bounceDirection.z;

                    enviromnet_map *farMap = 0;
                    real32 pZ = originZ + zDiff;
                    real32 mapZ = 2.0f;
                    // NOTE : Tells us the blend of the enviromnet
                    real32 tEnvMap = bounceDirection.y;
                    // NOTE : How much should we grab from the farmap comparing to the middlemap
                    real32 tFarMap = 0.0f;
                    if(tEnvMap < -0.5f)
                    {
                        farMap = bottom;
                        // NOTE: If the tEnvMap is -0.5f, it means it's not even looking at the
                        // bottom so the tFarMap should be 0
                        // if it is -1.0f, it means it's directly looking at the bottom
                        // so the tFarMap should be 1
                        tFarMap = -1.0f - 2.0f*tEnvMap;
                    }
                    else if(tEnvMap > 0.5f)
                    {
                        farMap = top;
                        tFarMap = 2.0f*tEnvMap - 1.0f;
                    }

                    v3 lightColor = {0, 0, 0};
                    if(farMap)
                    {
                        real32 distanceFromMapInZ = farMap->pZ - pZ;
                        v3 farMapColor = SampleEnvironmentMap(screenSpaceUV, bounceDirection, normal.w, farMap, distanceFromMapInZ);
                        lightColor = Lerp(lightColor, tFarMap, farMapColor);
                    }

                    texel.rgb += texel.a*lightColor;
#if 0
                    // NOTE : Draws the bounce direction
                    texel.rgb = V3(0.5f, 0.5f, 0.5f) + 0.5f*bounceDirection;
                    texel.rgb *= texel.a;
#endif
                }
                texel = Hadamard(texel, color);

                texel.r = Clamp01(texel.r);
                texel.g = Clamp01(texel.g);
                texel.b = Clamp01(texel.b);

                // color channels of the dest bmp
                v4 dest = Unpack4x8(*pixel);

                dest = SRGB255ToLinear1(dest);

                real32 invTexelA = (1.0f - texel.a);

                // NOTE : Color blending equation : (1-sa)*d + s -> this s should be premulitplied by alpha!
                // Color blending equation with color : (1-sa)*d + ca*c*s
                v4 blended = {invTexelA*dest.r + texel.r,
                                invTexelA*dest.g + texel.g,
                                invTexelA*dest.b + texel.b,
                                (texel.a + dest.a - texel.a*dest.a)};

                v4 blended255 = Linear1ToSRGB255(blended);

                // NOTE : Put it back as a, r, g, b order
                *pixel = (((uint32)(blended255.a + 0.5f) << 24) |
                        ((uint32)(blended255.r + 0.5f) << 16) |
                        ((uint32)(blended255.g + 0.5f) << 8) |
                        ((uint32)(blended255.b + 0.5f) << 0));

#else
                *pixel = color32;
#endif
            }

            // We could not use *pixel++ as we did because
            // we are performing some tests against pixels!
            pixel++;
#else
            // Use this to see what region of pixels are we testing!
            *pixel++ = color32;
#endif
        }

        row += buffer->pitch;
    }

    END_TIMED_BLOCK(DrawSomethingSlowly);
}

/*
    NOTE : This is how DrawBitmap works!
    1. blend two bitamps(buffer and sourceBitmap)
        Let's say that the result is blendedColor
    2. blend the screen and the blendedColor
        This is our result!

    Basically, the function is B(Cs, B(S, D))
    and the equation is
    Alpha = A0 + A1 - A0A\*/

struct entity_basis_pos_result
{
    v2 pos;
    real32 scale;
    bool32 valid;
};

// Get the bitmap position, size in the buffer
internal entity_basis_pos_result
GetRenderEntityBasePoint(render_group *renderGroup, render_entry_basis *entityBasis, v2 screenDim)
{
    entity_basis_pos_result result = {};

    v2 screenCenter = 0.5f * screenDim;
    // real32 zFudge = (1.0f + 0.01f*entityBasePos.z);

    /*
        Perspective Projection

        According to the similiar triangles, this equation is made.
        EntityX / ProjectedX = (CameraZ - EntityZ) / focalLength
        -> ProjectedX = focalLength * EntityX / (CameraZ - EntityZ)

        Where EntityX is rawX and
        FocalLength is distance between the camera(eye) and the monitor

        Same thing is also true with Y.
    */

    v3 entityBasePos = entityBasis->basis->pos;
    real32 distanceToPz = renderGroup->renderCamera.distanceAboveTarget - entityBasePos.z;
    // Because we don't want the object to be right in front of the camera
    // or pass the camera, we use this value to check the distanceToPz
    real32 nearClipPlane = 0.2f;

    if(distanceToPz > nearClipPlane)
    {
        v3 rawXY = V3(entityBasePos.xy + entityBasis->offset.xy, 1.0f);
        v3 projectedXY = (1.0f / distanceToPz) * renderGroup->renderCamera.focalLength * rawXY;

        // NOTE : Now this is the only things that care about the metersToPixels
        // This metersToPixels - this meter is the meter based on the monitor, not the game!
        result.pos = screenCenter + renderGroup->metersToPixels*projectedXY.xy;
        result.scale = renderGroup->metersToPixels*projectedXY.z;
        result.valid = true;
    }

    return result;
}

internal void
RenderGroupToOutputBuffer(render_group *renderGroup, loaded_bitmap *outputTarget)
{
    BEGIN_TIMED_BLOCK(RenderGroupToOutputBuffer);
    v2 screenDim = V2i(outputTarget->width, outputTarget->height);

    real32 pixelsToMeters = 1.0f/renderGroup->metersToPixels;

    for(uint32 baseIndex = 0;
        baseIndex < renderGroup->pushBufferSize;
        )
    {
        render_group_entry_header *header = (render_group_entry_header *)(renderGroup->pushBufferBase + baseIndex);
        void *data = (uint8 *)header + sizeof(*header);

        switch(header->type)
        {
            case RenderGroupEntryType_render_group_entry_clear:
            {
                render_group_entry_clear *entry = (render_group_entry_clear *)data;

                // Most hardware has faster way to clear the buffer rather than
                // drawing the whole buffer again, so change the thing after!
                // TODO : Maybe add alpha value to this buffer?
                DrawRectangle(outputTarget, V2(0, 0),
                    V2((real32)outputTarget->width, (real32)outputTarget->height), entry->color);

                baseIndex += sizeof(*entry) + sizeof(*header);
            }break;

            case RenderGroupEntryType_render_group_entry_bitmap:
            {
                render_group_entry_bitmap *entry = (render_group_entry_bitmap *)data;

                entity_basis_pos_result basis = GetRenderEntityBasePoint(renderGroup, &entry->entryBasis, screenDim);

                DrawSomethingHopefullyFast(outputTarget, basis.pos,
                                            basis.scale*V2(entry->size.x, 0),
                                            basis.scale*V2(0, entry->size.y),
                                            entry->color,
                                            entry->bitmap, 0, 0, 0, 0);

                baseIndex += sizeof(*entry) + sizeof(*header);
            }break;

            case RenderGroupEntryType_render_group_entry_rectangle:
            {
                render_group_entry_rectangle *entry = (render_group_entry_rectangle *)data;

                entity_basis_pos_result basis = GetRenderEntityBasePoint(renderGroup, &entry->entryBasis, screenDim);
                DrawRectangle(outputTarget, basis.pos, basis.pos + basis.scale*entry->dim, entry->color);

                baseIndex += sizeof(*entry) + sizeof(*header);
            }break;

            InvalidDefaultCase;
        }
    }

    END_TIMED_BLOCK(RenderGroupToOutputBuffer);
}

internal render_group *
AllocateRenderGroup(memory_arena *arena, uint32 maxPushBufferSize, uint32 resolutionX, uint32 resolutionY)
{
    render_group *result = PushStruct(arena, render_group);
    result->pushBufferBase = (uint8 *)PushSize(arena, maxPushBufferSize);
    result->maxPushBufferSize = maxPushBufferSize;
    result->pushBufferSize = 0;

    // So that we don't need to check defaultBasis is NULL
    render_basis *defaultBasis = PushStruct(arena, render_basis);
    result->defaultBasis = defaultBasis;

    result->gameCamera.focalLength = 0.6f;
    result->gameCamera.distanceAboveTarget = 9.0f;

    result->renderCamera = result->gameCamera;
    // result->renderCamera.distanceAboveTarget = 30.0f;

    // TODO : Probably indicates we want to seperate update and render
    // for entities sometime?
    result->globalAlpha = 1.0f;

    // TODO : need to adjust this baed on buffer size!
    real32 widthOfMonitorInMeter = 0.635f;
    result->metersToPixels = (real32)resolutionX * widthOfMonitorInMeter;

    real32 pixelsToMeters = 1.0f/result->metersToPixels;
    // This is the value based on the monitor
    result->monitorHalfDimInMeters = V2(0.5f*resolutionX*pixelsToMeters, 
                                        0.5f*resolutionY*pixelsToMeters);

    return result;
}


#define PushRenderElement(group, type) (type *)PushRenderElement_(group, sizeof(type), RenderGroupEntryType_##type)
internal void *
PushRenderElement_(render_group *group, uint32 size, render_group_entry_type type)
{
    void *result = 0;
    // NOTE : Because we are now pushing the header and the entry spartely!
    size += sizeof(render_group_entry_type);

    if(group->pushBufferSize + size < group->maxPushBufferSize)
    {
        render_group_entry_header *header = (render_group_entry_header *)(group->pushBufferBase + group->pushBufferSize);
        header->type = type;
        result = (uint8 *)header + sizeof(*header);
        group->pushBufferSize += size;
    }
    else
    {
        InvalidCodePath;
    }

    return result;
}

inline void
PushBitmap(render_group *group, loaded_bitmap *bitmap, real32 heightInMeters, v3 offset, v4 color = V4(1, 1, 1, 1))
{
    render_group_entry_bitmap *piece = PushRenderElement(group, render_group_entry_bitmap);

    if(piece)
    {
        piece->bitmap = bitmap;
        piece->entryBasis.basis = group->defaultBasis;

        v2 size = V2(bitmap->widthOverHeight*heightInMeters, heightInMeters);
        // This align is topdown.
        v2 align = Hadamard(bitmap->alignPercentage, size);

        piece->entryBasis.offset = offset - V3(align, 0);
        piece->color = group->globalAlpha*color;
        piece->size = size;
    }
}

inline void
PushRect(render_group *group, v3 offset, v2 dim, v4 color)
{
    render_group_entry_rectangle *piece = PushRenderElement(group, render_group_entry_rectangle);

    if(piece)
    {
        piece->entryBasis.basis = group->defaultBasis;
        piece->entryBasis.offset = offset - V3(0.5f*dim, 0);
        piece->color = color;
        piece->dim = dim;
    }
}

inline void
PushRectOutline(render_group *group, v3 offset, v2 dim, v4 color)
{
    real32 thickness = 0.2f;

    // Top and Bottom
    PushRect(group, offset - V3(0, 0.5f*dim.y, 0), V2(dim.x, thickness), color);
    PushRect(group, offset + V3(0, 0.5f*dim.y, 0), V2(dim.x, thickness), color);

    // Left and Right
    PushRect(group, offset - V3(0.5f*dim.x, 0, 0), V2(thickness, dim.y), color);
    PushRect(group, offset + V3(0.5f*dim.x, 0, 0), V2(thickness, dim.y), color);
}

inline void
Clear(render_group *group, v4 color)
{
    render_group_entry_clear *piece = PushRenderElement(group, render_group_entry_clear);
    if(piece)
    {
        piece->color = color;
    }
}

inline render_group_entry_coordinate_system *
PushCoordinateSystem(render_group *group, loaded_bitmap *bitmap, v2 origin, v2 xAxis, v2 yAxis, v4 color,
                    loaded_bitmap *normalMap, enviromnet_map *top, enviromnet_map *middle, enviromnet_map *bottom)
{
    render_group_entry_coordinate_system *piece = PushRenderElement(group, render_group_entry_coordinate_system);
    if(piece)
    {
        piece->origin = origin;
        piece->xAxis = xAxis;
        piece->yAxis = yAxis;
        piece->color = color;
        piece->bitmap = bitmap;

        piece->normalMap = normalMap;

        piece->top = top;
        piece->middle = middle;
        piece->bottom = bottom;

        piece->top = top;
        piece->middle = middle;
        piece->bottom = bottom;
    }

    return piece;
}

// Get a point from the monitor and get the position in the game world
inline v2
Unproject(render_group *renderGroup, v2 projectedXY, real32 distanceFromCamera)
{
    // Exact opposite operation of projecting
    v2 worldXY = (distanceFromCamera / renderGroup->gameCamera.focalLength) * projectedXY;
    return worldXY;
}

inline rect2
GetCameraRectangleAtDistance(render_group *renderGroup, real32 distanceFromCamera)
{
    v2 rawWorldXY = Unproject(renderGroup, renderGroup->monitorHalfDimInMeters, distanceFromCamera);

    rect2 result = RectCenterHalfDim(V2(0, 0), rawWorldXY);

    return result;
}

inline rect2
GetCameraRectangleAtTarget(render_group *renderGroup)
{
    rect2 result = GetCameraRectangleAtDistance(renderGroup, renderGroup->gameCamera.distanceAboveTarget);

    return result;
}