
#if !defined(FOX_MATH_H)

union v2
{
    struct
    {
        real32 x, y;
    };
    real32 e[2];
};

union v3
{
    struct
    {
        real32 x, y, z;
    };
    struct
    {
        real32 r, g, b;
    };
    struct
    {
        v2 xy;
        real32 ignored0_;
    };
    real32 e[3];
};

union v4
{
    struct
    {
        real32 x, y, z, w;
    };
    struct
    {
        union
        {
            v3 xyz;
            struct
            {
                real32 x, y, z;
            };
        };
        real32 w;
    };
    struct
    {
        union
        {
            v3 rgb;
            struct
            {
                real32 r, g, b;
            };
        };
        real32 a;
    };
    struct
    {
        v2 xy;
        real32 ignored0_;
        real32 ignored1_;
    };
    struct
    {
        real32 ignored2_;        
        v2 yz;
        real32 ignored3_;
    };
    struct
    {
        real32 ignored4_;
        real32 ignored5_;
        v2 zw;
    };
    
    real32 e[4];
};

inline v2
V2i(int32 x, int32 y)
{
    v2 result = {(real32)x, (real32)y};
    return result;
}

inline v2
V2i(uint32 x, uint32 y)
{
    v2 result = {(real32)x, (real32)y};
    return result;
}

// Constructor
inline v2
V2(real32 x, real32 y)
{
    v2 result;

    result.x = x;
    result.y = y;

    return(result);
}

inline v2
V2(real32 value)
{
    v2 result;

    result.x = value;
    result.y = value;

    return result;
}

inline v3
V3(real32 x, real32 y, real32 z)
{
    v3 result;

    result.x = x;
    result.y = y;
    result.z = z;

    return(result);
}

inline v3
V3(v2 xy, real32 z)
{
    v3 result;

    result.x = xy.x;
    result.y = xy.y;
    result.z = z;

    return(result);
}

inline v3
V3(real32 value)
{
    v3 result;

    result.x = value;
    result.y = value;
    result.z = value;
    
    return result;
}

inline v4
V4(real32 x, real32 y, real32 z, real32 w)
{
    v4 result;

    result.x = x;
    result.y = y;
    result.z = z;
    result.w = w;

    return(result);
}

inline v2
operator*(real32 a, v2 b)
{
    v2 result;

    result.x = a*b.x;
    result.y = a*b.y;

    return(result);
}

inline v2
operator*(v2 b, real32 a)
{
    v2 result = a*b;

    return(result);
}

inline v2 &
operator*=(v2 &b, real32 a)
{
    b = a * b;

    return(b);
}

inline v2
operator-(v2 a)
{
    v2 result;

    result.x = -a.x;
    result.y = -a.y;

    return(result);
}

inline v2
operator+(v2 a, v2 b)
{
    v2 result;

    result.x = a.x + b.x;
    result.y = a.y + b.y;

    return(result);
}

inline v2 &
operator+=(v2 &a, v2 b)
{
    a = a + b;

    return(a);
}

inline v2
operator-(v2 a, v2 b)
{
    v2 result;

    result.x = a.x - b.x;
    result.y = a.y - b.y;

    return(result);
}

inline v2 &
operator-=(v2 &a, v2 b)
{
    a = a - b;

    return(a);
}

inline v2
operator *(v2 a, v2 b)
{
    v2 result;

    result.x = a.x * b.x;
    result.y = a.y * b.y;

    return result;
}

inline real32
Square(real32 a)
{
    real32 result = a * a;

    return result;
}

// Linear interpolation
inline real32
Lerp(real32 a, real32 t, real32 b)
{
    // NOTE : t is always between 0.0f and 1.0f
    real32 result = (1.0f - t) * a + t * b;
    return result;
}

// NOTE : clamp means putting certain value to be a value
// between two values. 
inline real32
Clamp(real32 min, real32 value, real32 max)
{
    real32 result = value;

    if(result < min)
    {
        result = min;
    }
    else if(result > max)
    {
        result = max;
    }

    return result;
}

inline real32
Clamp01(real32 value)
{
    real32 result = Clamp(0.0f, value, 1.0f);
    return result;
}

inline real32
Clamp01MapInRange(real32 min, real32 value, real32 max)
{
    real32 result = 0.0f;
    real32 range = max - min;

    if(range != 0.0f)
    {
        result = Clamp01((value - min) / range);
    }

    return result;
}

inline v2
Clamp01(v2 value)
{
    v2 result = V2(Clamp(0.0f, value.x, 1.0f),
                    Clamp(0.0f, value.y, 1.0f));

    return result;
}

inline v3
Clamp01(v3 value)
{
    v3 result = V3(Clamp(0.0f, value.x, 1.0f),
                    Clamp(0.0f, value.y, 1.0f),
                    Clamp(0.0f, value.z, 1.0f));

    return result;
}

inline v2
Hadamard(v2 a, v2 b)
{
    v2 result = V2(a.x * b.x, a.y * b.y);

    return result;
}

inline real32
Inner(v2 a, v2 b)
{
    real32 result = a.x * b.x + a.y * b.y;

    return result;
}

inline real32
LengthSq(v2 a)
{
    real32 result = Inner(a, a);

    return result;
}

inline real32
Length(v2 a)
{
    real32 result = Root2(Inner(a, a));

    return result;
}


// If we say a / b,
// a is the numerator and
// b is the divisor
// This function prevents from dividing by 0
inline real32
SafeRatioN(real32 numerator, real32 divisor, real32 n)
{
    real32 result = n;
    
    if(divisor != 0.0f)
    {
        result = numerator / divisor;
    }

    return result;
}

inline real32
SafeRatio0(real32 numerator, real32 divisor)
{
    real32 result = SafeRatioN(numerator, divisor, 0.0f);
    return result;
}


inline real32
SafeRatio1(real32 numerator, real32 divisor)
{
    real32 result = SafeRatioN(numerator, divisor, 1.0f);
    return result;
}

///v3
inline v3
operator*(real32 a, v3 b)
{
    v3 result;

    result.x = a*b.x;
    result.y = a*b.y;
    result.z = a*b.z;    

    return(result);
}

inline v3
operator*(v3 b, real32 a)
{
    v3 result = a*b;

    return(result);
}

inline v3 &
operator*=(v3 &b, real32 a)
{
    b = a * b;

    return(b);
}

inline v3
operator-(v3 a)
{
    v3 result;

    result.x = -a.x;
    result.y = -a.y;
    result.z = -a.z;    

    return(result);
}

inline v3
operator+(v3 a, v3 b)
{
    v3 result;

    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;    

    return(result);
}

inline v3 &
operator+=(v3 &a, v3 b)
{
    a = a + b;

    return(a);
}

inline v3
operator-(v3 a, v3 b)
{
    v3 result;

    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;

    return(result);
}

inline v3 &
operator-=(v3 &a, v3 b)
{
    a = a - b;

    return(a);
}

inline v3
operator*(v3 a, v3 b)
{
    v3 result;

    result.x = a.x * b.x;
    result.y = a.y * b.y;
    result.z = a.z * b.z;

    return result;
}

inline v3
Hadamard(v3 a, v3 b)
{
    v3 result = V3(a.x * b.x, a.y * b.y, a.z * b.z);

    return result;
}

inline real32
Inner(v3 a, v3 b)
{
    real32 result = a.x * b.x + a.y * b.y + a.z * b.z;

    return result;
}

inline real32
Length(v3 a)
{
    real32 result = Root2(Inner(a, a));

    return result;
}

inline real32
LengthSq(v3 a)
{
    real32 result = Inner(a, a);

    return result;
}

struct rect2
{
    v2 min;
    v2 max;
};

// Make a rectangle using 2 points
inline rect2
RectMinMax(v2 min, v2 max)
{
    rect2 result;
    
    result.min = min;
    result.max = max;

    return result;
}

// Make a rectangle using 1 point and width and height
inline rect2
RectCenterHalfDim(v2 center, v2 halfDim)
{
    rect2 result;

    result.min = center - halfDim;
    result.max = center + halfDim;

    return result;
}

inline rect2
RectCenterDim(v2 center, v2 dim)
{
    rect2 result = RectCenterHalfDim(center, 0.5f * dim);

    return result;
}

inline rect2
RectMinDim(v2 min, v2 dim)
{
    rect2 result;

    result.min = min;
    result.max = min + dim;

    return result;
}

inline rect2
AddRadiusToRect(rect2 rectangle, v2 radius)
{
    rectangle.min -= radius;
    rectangle.max += radius;
    
    return rectangle;
}

inline bool32
IsInRectangle(rect2 rectangle, v2 testPos)
{
    bool32 result = false;

    if(testPos.x >= rectangle.min.x && 
        testPos.x < rectangle.max.x &&
        testPos.y >= rectangle.min.y && 
        testPos.y < rectangle.max.y)
    {
        result = true;
    }

    return result;
}

inline v2
GetMinCorner(rect2 rect)
{
    return rect.min;
}

inline v2
GetMaxCorner(rect2 rect)
{
    return rect.max;
}

inline v2
GetDim(rect2 rect)
{
    return rect.max - rect.min;
}

inline v2
GetCenter(rect2 rect)
{
    return 0.5f * (rect.min + rect.max);
}

inline v2
GetBarycentric(rect2 a, v2 p)
{
    v2 result;

    result.x = SafeRatio0((p.x - a.min.x), (a.max.x - a.min.x));
    result.y = SafeRatio0((p.y - a.min.y), (a.max.y - a.min.y));
    
    return result;
}


//v4
inline v4
operator*(real32 a, v4 b)
{
    v4 result;

    result.x = a*b.x;
    result.y = a*b.y;
    result.z = a*b.z;    
    result.w = a*b.w;    

    return(result);
}

inline v4
operator*(v4 b, real32 a)
{
    v4 result = a*b;

    return(result);
}

inline v4 &
operator*=(v4 &b, real32 a)
{
    b = a * b;

    return(b);
}

inline v4
operator-(v4 a)
{
    v4 result;

    result.x = -a.x;
    result.y = -a.y;
    result.z = -a.z;    
    result.w = -a.w;    

    return(result);
}

inline v4
operator+(v4 a, v4 b)
{
    v4 result;

    result.x = a.x + b.x;
    result.y = a.y + b.y;
    result.z = a.z + b.z;    
    result.w = a.w + b.w;    
    
    return(result);
}

inline v4 &
operator+=(v4 &a, v4 b)
{
    a = a + b;

    return(a);
}

inline v4
operator-(v4 a, v4 b)
{
    v4 result;

    result.x = a.x - b.x;
    result.y = a.y - b.y;
    result.z = a.z - b.z;

    return(result);
}

inline v4 &
operator-=(v4 &a, v4 b)
{
    a = a - b;

    return(a);
}

inline v4
operator*(v4 a, v4 b)
{
    v4 result;

    result.x = a.x * b.x;
    result.y = a.y * b.y;
    result.z = a.z * b.z;
    result.w = a.w * b.w;

    return result;
}

inline v4
Hadamard(v4 a, v4 b)
{
    v4 result = V4(a.x*b.x, a.y*b.y, a.z*b.z, a.w*b.w);

    return result;
}

inline real32
Inner(v4 a, v4 b)
{
    real32 result = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;

    return result;
}

inline real32
LengthSq(v4 a)
{
    real32 result = Inner(a, a);

    return result;
}

inline real32
Length(v4 a)
{
    real32 result = Root2(Inner(a, a));

    return result;
}

//


struct rect3
{
    v3 min;
    v3 max;
};

inline v3
GetMinCorner(rect3 rect)
{
    return rect.min;
}

inline v3
GetMaxCorner(rect3 rect)
{
    return rect.max;
}

inline v3
GetCenter(rect3 rect)
{
    return 0.5f * (rect.min + rect.max);
}

// Make a rectangle using 2 points
inline rect3
RectMinMax(v3 min, v3 max)
{
    rect3 result;
    
    result.min = min;
    result.max = max;

    return result;
}

// Make a rectangle using 1 point and width and height
inline rect3
RectCenterHalfDim(v3 center, v3 halfDim)
{
    rect3 result;

    result.min = center - halfDim;
    result.max = center + halfDim;

    return result;
}

inline rect3
RectCenterDim(v3 center, v3 dim)
{
    rect3 result = RectCenterHalfDim(center, 0.5f * dim);

    return result;
}

inline rect3
RectMinDim(v3 min, v3 dim)
{
    rect3 result;

    result.min = min;
    result.max = min + dim;

    return result;
}

inline rect3
AddRadiusToRect(rect3 rectangle, v3 radius)
{
    rectangle.min -= radius;
    rectangle.max += radius;
    
    return rectangle;   
}

inline bool32
IsInRectangle(rect3 rectangle, v3 testPos)
{
    bool32 result = false;

    if(testPos.x >= rectangle.min.x && 
        testPos.x < rectangle.max.x &&
        testPos.y >= rectangle.min.y && 
        testPos.y < rectangle.max.y &&
        testPos.z >= rectangle.min.z && 
        testPos.z < rectangle.max.z)
    {
        result = true;
    }

    return result;
}

inline bool32
RectanglesIntersect(rect3 a, rect3 b)
{
    bool32 Result = !((b.max.x <= a.min.x) ||
                      (b.min.x >= a.max.x) ||
                      (b.max.y <= a.min.y) ||
                      (b.min.y >= a.max.y) ||
                      (b.max.z <= a.min.z) ||
                      (b.min.z >= a.max.z));
    return(Result);
}

// Get the point and produce a coordinate based on the min of the rectangle
// which means, min corner of the rectangle is 0, 0, 0 and diameters are 1.0f
inline v3
GetBarycentric(rect3 a, v3 p)
{
    v3 result;

    result.x = SafeRatio0((p.x - a.min.x), (a.max.x - a.min.x));
    result.y = SafeRatio0((p.y - a.min.y), (a.max.y - a.min.y));
    result.z = SafeRatio0((p.z - a.min.z), (a.max.z - a.min.z));
    
    return result;
}

// XY means slicing the rect perpendicular to Z axis
inline rect2
ToRectangleXY(rect3 a)
{
    rect2 result;

    result.min = a.min.xy;
    result.max = a.max.xy;

    return result;
}

inline v3
Lerp(v3 a, real32 t, v3 b)
{
    v3 result = {};
    
    // t is always between 0.0f and 1.0f
    result.x = Lerp(a.x, t, b.x);
    result.y = Lerp(a.y, t, b.y);
    result.z = Lerp(a.z, t, b.z);
    
    return result;
}

// Linear interpolation
inline v4
Lerp(v4 a, real32 t, v4 b)
{
    v4 result = {};
    
    // t is always between 0.0f and 1.0f
    result.x = Lerp(a.x, t, b.x);
    result.y = Lerp(a.y, t, b.y);
    result.z = Lerp(a.z, t, b.z);
    result.w = Lerp(a.w, t, b.w);
    
    return result;
}

inline v4
V4(v3 xyz, real32 w)
{
    v4 result;
    
    result.x = xyz.x;
    result.y = xyz.y;
    result.z = xyz.z;
    result.w = w;

    return result;
}

inline v3
Normalize(v3 vector)
{
    v3 result;

    real32 length = Length(vector);
    
    result.x = vector.x / length;
    result.y = vector.y / length;
    result.z = vector.z / length;

    return result;
}

#define FOX_MATH_H
#endif