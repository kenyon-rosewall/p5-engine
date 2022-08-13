#pragma once

#ifndef P5ENGINE_RENDER_GROUP_H
#define P5ENGINE_RENDER_GROUP_H

/* NOTE: 
* 
*  1) Everywhere outside the renderer, Y _always_ goes upward, X to the right.
*
*  2) All bitmaps including the render target are assumed to be bottom-up
*	(meaning that the first row pointer points to the bottom-most row when
*	 viewed on screen.
* 
*  3) It is mandatory that all inputs to the renderer are in world 
*	coordinates ("meters"), NOT pixels. If for some reason something
*   absolutely has to be specified in pixels, that will be explicitly
*	marked in the API, but this should occur exceedingly sparingly.
* 
*  4) Z is a special coordinate because it is broken up into discrete slices,
*	and the renderer actually understands these slices. Z slices are what control
*	the _scaling_ of things, wheras Z offsets inside a slice are what control Y
*	offsetting.
* 
*  5) All color values specified to the renderer as V4s are in NON-premultiplied
*	alpha.
* 
*/

struct loaded_bitmap
{
	v2 AlignPercentage;
	real32 WidthOverHeight;

	int32 Width;
	int32 Height;
	int32 Pitch;
	void* Memory;
};

struct environment_map
{
	loaded_bitmap LOD[4];
	real32 Pz;
};

struct render_basis
{
	v3 Pos;
};

struct render_entity_basis
{
	render_basis* Basis;
	v3 Offset;
};

enum class render_group_entry_type
{
	render_entry_clear,
	render_entry_bitmap,
	render_entry_rectangle,
	render_entry_coordinate_system
};

struct render_group_entry_header
{
	render_group_entry_type Type;
};

struct render_entry_clear
{
	v4 Color;
};

struct render_entry_saturation
{
	real32 Level;
};

struct render_entry_bitmap
{
	render_entity_basis EntityBasis;
	loaded_bitmap* Bitmap;
	v2 Size;
	v4 Color;
};

struct render_entry_rectangle
{
	render_entity_basis EntityBasis;
	v2 Dim;
	v4 Color;
};

// NOTE: This only for test:
// {
struct render_entry_coordinate_system
{
	v2 Origin;
	v2 XAxis;
	v2 YAxis;
	v4 Color;
	loaded_bitmap* Texture;
	loaded_bitmap* NormalMap;

	environment_map* Top;
	environment_map* Middle;
	environment_map* Bottom;
};
// }

struct render_group_camera
{
	// NOTE: Camera parameters
	real32 FocalLength;
	real32 DistanceAboveTarget;
};

struct render_group
{
	render_group_camera GameCamera;
	render_group_camera RenderCamera;

	// NOTE: This translates meters _on the monitor_ into pixels _on the monitor_
	real32 MetersToPixels;
	v2 MonitorHalfDimInMeters;

	real32 GlobalAlpha;

	render_basis* DefaultBasis;

	uint32 MaxPushBufferSize;
	uint32 PushBufferSize;
	uint8* PushBufferBase;
};

#endif // !P5ENGINE_RENDER_GROUP_H