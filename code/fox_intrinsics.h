#ifndef FOX_INTRINSICS_H
#define FOX_INTRINSICS_H

//These functions should be converted
//to platform_efficeint versions

// TODO : This library should go away!
#include "math.h"

inline int32
SignOf(int32 value)
{
    int32 result = (value >= 0) ? 1 : -1;

    return result;
}

inline real32
Root2(real32 value)
{
    real32 result = sqrtf(value);

    return result;
}

inline real32
AbsoluteValue(real32 value)
{
    real32 result = (real32)fabs(value);
    return result;
}

inline uint32
RotateLeft(uint32 Value, int32 Amount)
{
#if COMPILER_MSVC
    uint32 Result = _rotl(Value, Amount);
#else
    // TODO(casey): Actually port this to other compiler platforms!
    Amount &= 31;
    uint32 Result = ((Value << Amount) | (Value >> (32 - Amount)));
#endif

    return(Result);
}

inline int32
CeilReal32ToInt32(real32 value)
{
    int32 result = (int32)ceilf(value);

    return result;
}

inline uint32
SafeTruncateUInt64(uint64 value)
{
    Assert(value <= 0xFFFFFFFF);
    uint32 result = (uint32)value;

    return result;
}

inline int32
RoundReal32ToInt32(real32 value)
{
    // TODO : Intrinsic?
    return (int32)roundf(value);
}

inline uint32
RoundReal32ToUInt32(real32 value)
{
    // TODO : Intrinsic?
    return (uint32)roundf(value);
}

inline int32
TruncateReal32ToInt32(real32 value)
{
    // TODO : Intrinsic?
    return (int32)value;
}

inline int32
FloorReal32ToInt32(real32 value)
{
    // TODO : Intrinsic?
    return (int32)floorf(value);
}

inline real32
Sin(real32 Angle)
{
    real32 Result = sinf(Angle);
    return(Result);
}

inline real32
Cos(real32 Angle)
{
    real32 Result = cosf(Angle);
    return(Result);
}

inline real32
ATan2(real32 Y, real32 X)
{
    real32 Result = atan2f(Y, X);
    return(Result);
}

struct bit_scan_result
{
    bool32 found;
    uint32 index;
};

// starting from lower 1 and going to left, check if any bit is set to 1 
inline bit_scan_result
FindLeastSignificantSetBit(uint32 value)
{
    bit_scan_result result  {};

#if COMPILER_MSVC
    // This is much more faster because it's intrinsic
    result.found = _BitScanForward((unsigned long *)&result.index, value);
#else
    for(uint32 test = 0;
        test < 32;
        ++test)
    {
        if(value & (1 << test))
        {
            result.index = test;
            result.found = true;
            break;
        }
    }
#endif

    return result;
}

#endif