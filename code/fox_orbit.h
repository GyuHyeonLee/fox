#ifndef FOX_ORBIT_H
#define FOX_ORBIT_H

struct planet_system
{
	real32 angle;
	v2 size;
};

struct orbit_total_system 
{
	v2 center;

	// orbit 1
	real32 r1;
	real32 speed1;

	planet_system planet1;

	// orbit 2
	real32 r2;
	real32 speed2;

	planet_system planet2;

	// orbit 3
	real32 r3;
	real32 speed3;

	planet_system planet3;
};

#endif