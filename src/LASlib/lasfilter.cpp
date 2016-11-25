/*
===============================================================================

  FILE:  lasfilter.cpp
  
  CONTENTS:
  
    see corresponding header file
  
  PROGRAMMERS:

    martin.isenburg@rapidlasso.com  -  http://rapidlasso.com

  COPYRIGHT:

    (c) 2007-2014, martin isenburg, rapidlasso - fast tools to catch reality

    This is free software; you can redistribute and/or modify it under the
    terms of the GNU Lesser General Licence as published by the Free Software
    Foundation. See the LICENSE.txt file for more information.

    This software is distributed WITHOUT ANY WARRANTY and without even the
    implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  
  CHANGE HISTORY:
  
    see corresponding header file
  
===============================================================================
*/
#include "lasfilter.hpp"

#include <stdio.h>
#include <Rcpp.h>

#include <Rcpp.h>

#include <map>
using namespace std;

typedef multimap<I64,F64> my_I64_F64_map;

class LAScriterionAnd : public LAScriterion
{
public:
  inline const CHAR* name() const { return "filter_and"; };
  inline I32 get_command(CHAR* string) const { int n = 0; n += one->get_command(&string[n]); n += two->get_command(&string[n]); n += sprintf(&string[n], "-%s ", name()); return n; };
  inline BOOL filter(const LASpoint* point) { return one->filter(point) && two->filter(point); };
  LAScriterionAnd(LAScriterion* one, LAScriterion* two) { this->one = one; this->two = two; };
private:
  LAScriterion* one;
  LAScriterion* two;
};

class LAScriterionOr : public LAScriterion
{
public:
  inline const CHAR* name() const { return "filter_or"; };
  inline I32 get_command(CHAR* string) const { int n = 0; n += one->get_command(&string[n]); n += two->get_command(&string[n]); n += sprintf(&string[n], "-%s ", name()); return n; };
  inline BOOL filter(const LASpoint* point) { return one->filter(point) || two->filter(point); };
  LAScriterionOr(LAScriterion* one, LAScriterion* two) { this->one = one; this->two = two; };
private:
  LAScriterion* one;
  LAScriterion* two;
};

class LAScriterionKeepTile : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_tile"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %g %g %g ", name(), ll_x, ll_y, tile_size); };
  inline BOOL filter(const LASpoint* point) { return (!point->inside_tile(ll_x, ll_y, ur_x, ur_y)); };
  LAScriterionKeepTile(F32 ll_x, F32 ll_y, F32 tile_size) { this->ll_x = ll_x; this->ll_y = ll_y; this->ur_x = ll_x+tile_size; this->ur_y = ll_y+tile_size; this->tile_size = tile_size; };
private:
  F32 ll_x, ll_y, ur_x, ur_y, tile_size;
};

class LAScriterionKeepCircle : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_circle"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %g %g %g ", name(), center_x, center_y, radius); };
  inline BOOL filter(const LASpoint* point) { return (!point->inside_circle(center_x, center_y, radius_squared)); };
  LAScriterionKeepCircle(F64 x, F64 y, F64 radius) { this->center_x = x; this->center_y = y; this->radius = radius; this->radius_squared = radius*radius; };
private:
  F64 center_x, center_y, radius, radius_squared;
};

class LAScriterionKeepxyz : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_xyz"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %g %g %g %g %g %g ", name(), min_x, min_y, min_z, max_x, max_y, max_z); };
  inline BOOL filter(const LASpoint* point) { return (!point->inside_box(min_x, min_y, min_z, max_x, max_y, max_z)); };
  LAScriterionKeepxyz(F64 min_x, F64 min_y, F64 min_z, F64 max_x, F64 max_y, F64 max_z) { this->min_x = min_x; this->min_y = min_y; this->min_z = min_z; this->max_x = max_x; this->max_y = max_y; this->max_z = max_z; };
private:
  F64 min_x, min_y, min_z, max_x, max_y, max_z;
};

class LAScriterionDropxyz : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_xyz"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %g %g %g %g %g %g ", name(), min_x, min_y, min_z, max_x, max_y, max_z); };
  inline BOOL filter(const LASpoint* point) { return (point->inside_box(min_x, min_y, min_z, max_x, max_y, max_z)); };
  LAScriterionDropxyz(F64 min_x, F64 min_y, F64 min_z, F64 max_x, F64 max_y, F64 max_z) { this->min_x = min_x; this->min_y = min_y; this->min_z = min_z; this->max_x = max_x; this->max_y = max_y; this->max_z = max_z; };
private:
  F64 min_x, min_y, min_z, max_x, max_y, max_z;
};

class LAScriterionKeepxy : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_xy"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %g %g %g %g ", name(), below_x, below_y, above_x, above_y); };
  inline BOOL filter(const LASpoint* point) { return (!point->inside_rectangle(below_x, below_y, above_x, above_y)); };
  LAScriterionKeepxy(F64 below_x, F64 below_y, F64 above_x, F64 above_y) { this->below_x = below_x; this->below_y = below_y; this->above_x = above_x; this->above_y = above_y; };
private:
  F64 below_x, below_y, above_x, above_y;
};

class LAScriterionDropxy : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_xy"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %g %g %g %g ", name(), below_x, below_y, above_x, above_y); };
  inline BOOL filter(const LASpoint* point) { return (point->inside_rectangle(below_x, below_y, above_x, above_y)); };
  LAScriterionDropxy(F64 below_x, F64 below_y, F64 above_x, F64 above_y) { this->below_x = below_x; this->below_y = below_y; this->above_x = above_x; this->above_y = above_y; };
private:
  F64 below_x, below_y, above_x, above_y;
};

class LAScriterionKeepx : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_x"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %g %g ", name(), below_x, above_x); };
  inline BOOL filter(const LASpoint* point) { F64 x = point->get_x(); return (x < below_x) || (x >= above_x); };
  LAScriterionKeepx(F64 below_x, F64 above_x) { this->below_x = below_x; this->above_x = above_x; };
private:
  F64 below_x, above_x;
};

class LAScriterionDropx : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_x"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %g %g ", name(), below_x, above_x); };
  inline BOOL filter(const LASpoint* point) { F64 x = point->get_x(); return ((below_x <= x) && (x < above_x)); };
  LAScriterionDropx(F64 below_x, F64 above_x) { this->below_x = below_x; this->above_x = above_x; };
private:
  F64 below_x, above_x;
};

class LAScriterionKeepy : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_y"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %g %g ", name(), below_y, above_y); };
  inline BOOL filter(const LASpoint* point) { F64 y = point->get_y(); return (y < below_y) || (y >= above_y); };
  LAScriterionKeepy(F64 below_y, F64 above_y) { this->below_y = below_y; this->above_y = above_y; };
private:
  F64 below_y, above_y;
};

class LAScriterionDropy : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_y"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %g %g ", name(), below_y, above_y); };
  inline BOOL filter(const LASpoint* point) { F64 y = point->get_y(); return ((below_y <= y) && (y < above_y)); };
  LAScriterionDropy(F64 below_y, F64 above_y) { this->below_y = below_y; this->above_y = above_y; };
private:
  F64 below_y, above_y;
};

class LAScriterionKeepz : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_z"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %g %g ", name(), below_z, above_z); };
  inline BOOL filter(const LASpoint* point) { F64 z = point->get_z(); return (z < below_z) || (z >= above_z); };
  LAScriterionKeepz(F64 below_z, F64 above_z) { this->below_z = below_z; this->above_z = above_z; };
private:
  F64 below_z, above_z;
};

class LAScriterionDropz : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_z"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %g %g ", name(), below_z, above_z); };
  inline BOOL filter(const LASpoint* point) { F64 z = point->get_z(); return ((below_z <= z) && (z < above_z)); };
  LAScriterionDropz(F64 below_z, F64 above_z) { this->below_z = below_z; this->above_z = above_z; };
private:
  F64 below_z, above_z;
};

class LAScriterionDropxBelow : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_x_below"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %g ", name(), below_x); };
  inline BOOL filter(const LASpoint* point) { return (point->get_x() < below_x); };
  LAScriterionDropxBelow(F64 below_x) { this->below_x = below_x; };
private:
  F64 below_x;
};

class LAScriterionDropxAbove : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_x_above"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %g ", name(), above_x); };
  inline BOOL filter(const LASpoint* point) { return (point->get_x() >= above_x); };
  LAScriterionDropxAbove(F64 above_x) { this->above_x = above_x; };
private:
  F64 above_x;
};

class LAScriterionDropyBelow : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_y_below"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %g ", name(), below_y); };
  inline BOOL filter(const LASpoint* point) { return (point->get_y() < below_y); };
  LAScriterionDropyBelow(F64 below_y) { this->below_y = below_y; };
private:
  F64 below_y;
};

class LAScriterionDropyAbove : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_y_above"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %g ", name(), above_y); };
  inline BOOL filter(const LASpoint* point) { return (point->get_y() >= above_y); };
  LAScriterionDropyAbove(F64 above_y) { this->above_y = above_y; };
private:
  F64 above_y;
};

class LAScriterionDropzBelow : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_z_below"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %g ", name(), below_z); };
  inline BOOL filter(const LASpoint* point) { return (point->get_z() < below_z); };
  LAScriterionDropzBelow(F64 below_z) { this->below_z = below_z; };
private:
  F64 below_z;
};

class LAScriterionDropzAbove : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_z_above"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %g ", name(), above_z); };
  inline BOOL filter(const LASpoint* point) { return (point->get_z() >= above_z); };
  LAScriterionDropzAbove(F64 above_z) { this->above_z = above_z; };
private:
  F64 above_z;
};

class LAScriterionKeepXY : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_XY"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d %d %d %d ", name(), below_X, below_Y, above_X, above_Y); };
  inline BOOL filter(const LASpoint* point) { return (point->get_X() < below_X) || (point->get_Y() < below_Y) || (point->get_X() >= above_X) || (point->get_Y() >= above_Y); };
  LAScriterionKeepXY(I32 below_X, I32 below_Y, I32 above_X, I32 above_Y) { this->below_X = below_X; this->below_Y = below_Y; this->above_X = above_X; this->above_Y = above_Y; };
private:
  I32 below_X, below_Y, above_X, above_Y;
};

class LAScriterionKeepX : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_X"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d %d ", name(), below_X, above_X); };
  inline BOOL filter(const LASpoint* point) { return (point->get_X() < below_X) || (above_X <= point->get_X()); };
  LAScriterionKeepX(I32 below_X, I32 above_X) { this->below_X = below_X; this->above_X = above_X; };
private:
  I32 below_X, above_X;
};

class LAScriterionDropX : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_X"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d %d ", name(), below_X, above_X); };
  inline BOOL filter(const LASpoint* point) { return ((below_X <= point->get_X()) && (point->get_X() < above_X)); };
  LAScriterionDropX(I32 below_X, I32 above_X) { this->below_X = below_X; this->above_X = above_X; };
private:
  I32 below_X;
  I32 above_X;
};

class LAScriterionKeepY : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_Y"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d %d ", name(), below_Y, above_Y); };
  inline BOOL filter(const LASpoint* point) { return (point->get_Y() < below_Y) || (above_Y <= point->get_Y()); };
  LAScriterionKeepY(I32 below_Y, I32 above_Y) { this->below_Y = below_Y; this->above_Y = above_Y; };
private:
  I32 below_Y, above_Y;
};

class LAScriterionDropY : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_Y"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d %d ", name(), below_Y, above_Y); };
  inline BOOL filter(const LASpoint* point) { return ((below_Y <= point->get_Y()) && (point->get_Y() < above_Y)); };
  LAScriterionDropY(I32 below_Y, I32 above_Y) { this->below_Y = below_Y; this->above_Y = above_Y; };
private:
  I32 below_Y;
  I32 above_Y;
};

class LAScriterionKeepZ : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_Z"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d %d ", name(), below_Z, above_Z); };
  inline BOOL filter(const LASpoint* point) { return (point->get_Z() < below_Z) || (above_Z <= point->get_Z()); };
  LAScriterionKeepZ(I32 below_Z, I32 above_Z) { this->below_Z = below_Z; this->above_Z = above_Z; };
private:
  I32 below_Z, above_Z;
};

class LAScriterionDropZ : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_Z"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d %d ", name(), below_Z, above_Z); };
  inline BOOL filter(const LASpoint* point) { return ((below_Z <= point->get_Z()) && (point->get_Z() < above_Z)); };
  LAScriterionDropZ(I32 below_Z, I32 above_Z) { this->below_Z = below_Z; this->above_Z = above_Z; };
private:
  I32 below_Z;
  I32 above_Z;
};

class LAScriterionDropXBelow : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_X_below"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d ", name(), below_X); };
  inline BOOL filter(const LASpoint* point) { return (point->get_X() < below_X); };
  LAScriterionDropXBelow(I32 below_X) { this->below_X = below_X; };
private:
  I32 below_X;
};

class LAScriterionDropXAbove : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_X_above"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d ", name(), above_X); };
  inline BOOL filter(const LASpoint* point) { return (point->get_X() >= above_X); };
  LAScriterionDropXAbove(I32 above_X) { this->above_X = above_X; };
private:
  I32 above_X;
};

class LAScriterionDropYBelow : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_Y_below"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d ", name(), below_Y); };
  inline BOOL filter(const LASpoint* point) { return (point->get_Y() < below_Y); };
  LAScriterionDropYBelow(I32 below_Y) { this->below_Y = below_Y; };
private:
  I32 below_Y;
};

class LAScriterionDropYAbove : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_Y_above"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d ", name(), above_Y); };
  inline BOOL filter(const LASpoint* point) { return (point->get_Y() >= above_Y); };
  LAScriterionDropYAbove(I32 above_Y) { this->above_Y = above_Y; };
private:
  I32 above_Y;
};

class LAScriterionDropZBelow : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_Z_below"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d ", name(), below_Z); };
  inline BOOL filter(const LASpoint* point) { return (point->get_Z() < below_Z); };
  LAScriterionDropZBelow(I32 below_Z) { this->below_Z = below_Z; };
private:
  I32 below_Z;
};

class LAScriterionDropZAbove : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_Z_above"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d ", name(), above_Z); };
  inline BOOL filter(const LASpoint* point) { return (point->get_Z() >= above_Z); };
  LAScriterionDropZAbove(I32 above_Z) { this->above_Z = above_Z; };
private:
  I32 above_Z;
};

class LAScriterionKeepFirstReturn : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_first"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s ", name()); };
  inline BOOL filter(const LASpoint* point) { return (point->return_number > 1); };
};

class LAScriterionKeepFirstOfManyReturn : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_first_of_many"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s ", name()); };
  inline BOOL filter(const LASpoint* point) { return ((point->number_of_returns == 1) || (point->return_number > 1)); };
};

class LAScriterionKeepMiddleReturn : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_middle"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s ", name()); };
  inline BOOL filter(const LASpoint* point) { return ((point->return_number == 1) || (point->return_number >= point->number_of_returns)); };
};

class LAScriterionKeepLastReturn : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_last"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s ", name()); };
  inline BOOL filter(const LASpoint* point) { return (point->return_number < point->number_of_returns); };
};

class LAScriterionKeepLastOfManyReturn : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_last_of_many"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s ", name()); };
  inline BOOL filter(const LASpoint* point) { return ((point->return_number == 1) || (point->return_number < point->number_of_returns)); };
};

class LAScriterionDropFirstReturn : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_first"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s ", name()); };
  inline BOOL filter(const LASpoint* point) { return (point->return_number == 1); };
};

class LAScriterionDropFirstOfManyReturn : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_first_of_many"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s ", name()); };
  inline BOOL filter(const LASpoint* point) { return ((point->number_of_returns > 1) && (point->return_number == 1)); };
};

class LAScriterionDropMiddleReturn : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_middle"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s ", name()); };
  inline BOOL filter(const LASpoint* point) { return ((point->return_number > 1) && (point->return_number < point->number_of_returns)); };
};

class LAScriterionDropLastReturn : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_last"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s ", name()); };
  inline BOOL filter(const LASpoint* point) { return (point->return_number >= point->number_of_returns); };
};

class LAScriterionDropLastOfManyReturn : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_last_of_many"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s ", name()); };
  inline BOOL filter(const LASpoint* point) { return ((point->number_of_returns > 1) && (point->return_number >= point->number_of_returns)); };
};

class LAScriterionKeepReturns : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_return_mask"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %u ", name(), ~drop_return_mask); };
  inline BOOL filter(const LASpoint* point) { return ((1 << point->return_number) & drop_return_mask); };
  LAScriterionKeepReturns(U32 keep_return_mask) { drop_return_mask = ~keep_return_mask; };
private:
  U32 drop_return_mask;
};

class LAScriterionKeepSpecificNumberOfReturns : public LAScriterion
{
public:
  inline const CHAR* name() const { return (numberOfReturns == 1 ? "keep_single" : (numberOfReturns == 2 ? "keep_double" : (numberOfReturns == 3 ? "keep_triple" : (numberOfReturns == 4 ? "keep_quadruple" : "keep_quintuple")))); };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s ", name()); };
  inline BOOL filter(const LASpoint* point) { return (point->number_of_returns != numberOfReturns); };
  LAScriterionKeepSpecificNumberOfReturns(U32 numberOfReturns) { this->numberOfReturns = numberOfReturns; };
private:
  U32 numberOfReturns;
};

class LAScriterionDropSpecificNumberOfReturns : public LAScriterion
{
public:
  inline const CHAR* name() const { return (numberOfReturns == 1 ? "drop_single" : (numberOfReturns == 2 ? "drop_double" : (numberOfReturns == 3 ? "drop_triple" : (numberOfReturns == 4 ? "drop_quadruple" : "drop_quintuple")))); };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s ", name()); };
  inline BOOL filter(const LASpoint* point) { return (point->number_of_returns == numberOfReturns); };
  LAScriterionDropSpecificNumberOfReturns(U32 numberOfReturns) { this->numberOfReturns = numberOfReturns; };
private:
  U32 numberOfReturns;
};

class LAScriterionDropScanDirection : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_scan_direction"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d ", name(), scan_direction); };
  inline BOOL filter(const LASpoint* point) { return (scan_direction == point->scan_direction_flag); };
  LAScriterionDropScanDirection(I32 scan_direction) { this->scan_direction = scan_direction; };
private:
  I32 scan_direction;
};

class LAScriterionKeepScanDirectionChange : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_scan_direction_change"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s ", name()); };
  inline BOOL filter(const LASpoint* point) { if (scan_direction_flag == point->scan_direction_flag) return TRUE; I32 s = scan_direction_flag; scan_direction_flag = point->scan_direction_flag; return s == -1; };
  void reset() { scan_direction_flag = -1; };
  LAScriterionKeepScanDirectionChange() { reset(); };
private:
  I32 scan_direction_flag;
};

class LAScriterionKeepEdgeOfFlightLine : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_edge_of_flight_line"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s ", name()); };
  inline BOOL filter(const LASpoint* point) { return (point->edge_of_flight_line == 0); };
};

class LAScriterionKeepRGB : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_RGB"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s_%s %d %d ", name(), (channel == 0 ? "red" : (channel == 1 ? "green" : (channel == 2 ? "blue" : "nir"))),  below_RGB, above_RGB); };
  inline BOOL filter(const LASpoint* point) { return ((point->rgb[channel] < below_RGB) || (above_RGB < point->rgb[channel])); };
  LAScriterionKeepRGB(I32 below_RGB, I32 above_RGB, I32 channel) { if (above_RGB < below_RGB) { this->below_RGB = above_RGB; this->above_RGB = below_RGB; } else { this->below_RGB = below_RGB; this->above_RGB = above_RGB; }; this->channel = channel; };
private:
  I32 below_RGB, above_RGB, channel;
};

class LAScriterionKeepScanAngle : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_scan_angle"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d %d ", name(), below_scan, above_scan); };
  inline BOOL filter(const LASpoint* point) { return (point->scan_angle_rank < below_scan) || (above_scan < point->scan_angle_rank); };
  LAScriterionKeepScanAngle(I32 below_scan, I32 above_scan) { if (above_scan < below_scan) { this->below_scan = above_scan; this->above_scan = below_scan; } else { this->below_scan = below_scan; this->above_scan = above_scan; } };
private:
  I32 below_scan, above_scan;
};

class LAScriterionDropScanAngleBelow : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_scan_angle_below"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d ", name(), below_scan); };
  inline BOOL filter(const LASpoint* point) { return (point->scan_angle_rank < below_scan); };
  LAScriterionDropScanAngleBelow(I32 below_scan) { this->below_scan = below_scan; };
private:
  I32 below_scan;
};

class LAScriterionDropScanAngleAbove : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_scan_angle_above"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d ", name(), above_scan); };
  inline BOOL filter(const LASpoint* point) { return (point->scan_angle_rank > above_scan); };
  LAScriterionDropScanAngleAbove(I32 above_scan) { this->above_scan = above_scan; };
private:
  I32 above_scan;
};

class LAScriterionDropScanAngleBetween : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_scan_angle_between"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d %d ", name(), below_scan, above_scan); };
  inline BOOL filter(const LASpoint* point) { return (below_scan <= point->scan_angle_rank) && (point->scan_angle_rank <= above_scan); };
  LAScriterionDropScanAngleBetween(I32 below_scan, I32 above_scan) { if (above_scan < below_scan) { this->below_scan = above_scan; this->above_scan = below_scan; } else { this->below_scan = below_scan; this->above_scan = above_scan; } };
private:
  I32 below_scan, above_scan;
};

class LAScriterionKeepIntensity : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_intensity"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d %d ", name(), below_intensity, above_intensity); };
  inline BOOL filter(const LASpoint* point) { return (point->intensity < below_intensity) || (point->intensity > above_intensity); };
  LAScriterionKeepIntensity(I32 below_intensity, I32 above_intensity) { this->below_intensity = below_intensity; this->above_intensity = above_intensity; };
private:
  I32 below_intensity, above_intensity;
};

class LAScriterionKeepIntensityBelow : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_intensity_below"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d ", name(), below_intensity); };
  inline BOOL filter(const LASpoint* point) { return (point->intensity >= below_intensity); };
  LAScriterionKeepIntensityBelow(I32 below_intensity) { this->below_intensity = below_intensity; };
private:
  I32 below_intensity;
};

class LAScriterionKeepIntensityAbove : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_intensity_above"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d ", name(), above_intensity); };
  inline BOOL filter(const LASpoint* point) { return (point->intensity <= above_intensity); };
  LAScriterionKeepIntensityAbove(I32 above_intensity) { this->above_intensity = above_intensity; };
private:
  I32 above_intensity;
};

class LAScriterionDropIntensityBelow : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_intensity_below"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d ", name(), below_intensity); };
  inline BOOL filter(const LASpoint* point) { return (point->intensity < below_intensity); };
  LAScriterionDropIntensityBelow(I32 below_intensity) { this->below_intensity = below_intensity; };
private:
  I32 below_intensity;
};

class LAScriterionDropIntensityAbove : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_intensity_above"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d ", name(), above_intensity); };
  inline BOOL filter(const LASpoint* point) { return (point->intensity > above_intensity); };
  LAScriterionDropIntensityAbove(I32 above_intensity) { this->above_intensity = above_intensity; };
private:
  I32 above_intensity;
};

class LAScriterionDropIntensityBetween : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_intensity_between"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d %d ", name(), below_intensity, above_intensity); };
  inline BOOL filter(const LASpoint* point) { return (below_intensity <= point->intensity) && (point->intensity <= above_intensity); };
  LAScriterionDropIntensityBetween(I32 below_intensity, I32 above_intensity) { this->below_intensity = below_intensity; this->above_intensity = above_intensity; };
private:
  I32 below_intensity, above_intensity;
};

class LAScriterionDropClassifications : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_classification_mask"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %u ", name(), drop_classification_mask); };
  inline BOOL filter(const LASpoint* point) { return ((1 << point->classification) & drop_classification_mask); };
  LAScriterionDropClassifications(U32 drop_classification_mask) { this->drop_classification_mask = drop_classification_mask; };
private:
  U32 drop_classification_mask;
};

class LAScriterionDropExtendedClassifications : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_extended_classification_mask"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %u %u %u %u %u %u %u %u ", name(), drop_extended_classification_mask[7], drop_extended_classification_mask[6], drop_extended_classification_mask[5], drop_extended_classification_mask[4], drop_extended_classification_mask[3], drop_extended_classification_mask[2], drop_extended_classification_mask[1], drop_extended_classification_mask[0]); };
  inline BOOL filter(const LASpoint* point) { return ((1 << (point->extended_classification - (32 * (point->extended_classification / 32)))) & drop_extended_classification_mask[point->extended_classification / 32]); };
  LAScriterionDropExtendedClassifications(U32 drop_extended_classification_mask[8]) { for (I32 i = 0; i < 8; i++) this->drop_extended_classification_mask[i] = drop_extended_classification_mask[i]; };
private:
  U32 drop_extended_classification_mask[8];
};

class LAScriterionDropSynthetic : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_synthetic"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s ", name()); };
  inline BOOL filter(const LASpoint* point) { return (point->get_synthetic_flag() == 1); };
};

class LAScriterionKeepSynthetic : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_synthetic"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s ", name()); };
  inline BOOL filter(const LASpoint* point) { return (point->get_synthetic_flag() == 0); };
};

class LAScriterionDropKeypoint : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_keypoint"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s ", name()); };
  inline BOOL filter(const LASpoint* point) { return (point->get_keypoint_flag() == 1); };
};

class LAScriterionKeepKeypoint : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_keypoint"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s ", name()); };
  inline BOOL filter(const LASpoint* point) { return (point->get_keypoint_flag() == 0); };
};

class LAScriterionDropWithheld : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_withheld"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s ", name()); };
  inline BOOL filter(const LASpoint* point) { return (point->get_withheld_flag() == 1); };
};

class LAScriterionKeepWithheld : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_withheld"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s ", name()); };
  inline BOOL filter(const LASpoint* point) { return (point->get_withheld_flag() == 0); };
};

class LAScriterionDropOverlap : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_overlap"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s ", name()); };
  inline BOOL filter(const LASpoint* point) { return (point->get_extended_overlap_flag() == 1); };
};

class LAScriterionKeepOverlap : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_overlap"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s ", name()); };
  inline BOOL filter(const LASpoint* point) { return (point->get_extended_overlap_flag() == 0); };
};

class LAScriterionKeepUserData : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_user_data"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d ", name(), user_data); };
  inline BOOL filter(const LASpoint* point) { return (point->user_data != user_data); };
  LAScriterionKeepUserData(U8 user_data) { this->user_data = user_data; };
private:
  U8 user_data;
};

class LAScriterionKeepUserDataBelow : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_user_data_below"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d ", name(), below_user_data); };
  inline BOOL filter(const LASpoint* point) { return (point->user_data >= below_user_data); };
  LAScriterionKeepUserDataBelow(U8 below_user_data) { this->below_user_data = below_user_data; };
private:
  U8 below_user_data;
};

class LAScriterionKeepUserDataAbove : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_user_data_above"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d ", name(), above_user_data); };
  inline BOOL filter(const LASpoint* point) { return (point->user_data <= above_user_data); };
  LAScriterionKeepUserDataAbove(U8 above_user_data) { this->above_user_data = above_user_data; };
private:
  U8 above_user_data;
};

class LAScriterionKeepUserDataBetween : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_user_data_between"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d %d ", name(), below_user_data, above_user_data); };
  inline BOOL filter(const LASpoint* point) { return (point->user_data < below_user_data) || (above_user_data < point->user_data); };
  LAScriterionKeepUserDataBetween(U8 below_user_data, U8 above_user_data) { this->below_user_data = below_user_data; this->above_user_data = above_user_data; };
private:
  U8 below_user_data, above_user_data;
};

class LAScriterionDropUserData : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_user_data"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d ", name(), user_data); };
  inline BOOL filter(const LASpoint* point) { return (point->user_data == user_data); };
  LAScriterionDropUserData(U8 user_data) { this->user_data = user_data; };
private:
  U8 user_data;
};

class LAScriterionDropUserDataBelow : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_user_data_below"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d ", name(), below_user_data); };
  inline BOOL filter(const LASpoint* point) { return (point->user_data < below_user_data) ; };
  LAScriterionDropUserDataBelow(U8 below_user_data) { this->below_user_data = below_user_data; };
private:
  U8 below_user_data;
};

class LAScriterionDropUserDataAbove : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_user_data_above"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d ", name(), above_user_data); };
  inline BOOL filter(const LASpoint* point) { return (point->user_data > above_user_data); };
  LAScriterionDropUserDataAbove(U8 above_user_data) { this->above_user_data = above_user_data; };
private:
  U8 above_user_data;
};

class LAScriterionDropUserDataBetween : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_user_data_between"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d %d ", name(), below_user_data, above_user_data); };
  inline BOOL filter(const LASpoint* point) { return (below_user_data <= point->user_data) && (point->user_data <= above_user_data); };
  LAScriterionDropUserDataBetween(U8 below_user_data, U8 above_user_data) { this->below_user_data = below_user_data; this->above_user_data = above_user_data; };
private:
  U8 below_user_data, above_user_data;
};

class LAScriterionKeepPointSource : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_point_source"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d ", name(), point_source_id); };
  inline BOOL filter(const LASpoint* point) { return (point->point_source_ID != point_source_id); };
  LAScriterionKeepPointSource(U16 point_source_id) { this->point_source_id = point_source_id; };
private:
  U16 point_source_id;
};

class LAScriterionKeepPointSourceBetween : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_point_source_between"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d %d ", name(), below_point_source_id, above_point_source_id); };
  inline BOOL filter(const LASpoint* point) { return (point->point_source_ID < below_point_source_id) || (above_point_source_id < point->point_source_ID); };
  LAScriterionKeepPointSourceBetween(U16 below_point_source_id, U16 above_point_source_id) { this->below_point_source_id = below_point_source_id; this->above_point_source_id = above_point_source_id; };
private:
  U16 below_point_source_id, above_point_source_id;
};

class LAScriterionDropPointSource : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_point_source"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d ", name(), point_source_id); };
  inline BOOL filter(const LASpoint* point) { return (point->point_source_ID == point_source_id) ; };
  LAScriterionDropPointSource(U16 point_source_id) { this->point_source_id = point_source_id; };
private:
  U16 point_source_id;
};

class LAScriterionDropPointSourceBelow : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_point_source_below"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d ", name(), below_point_source_id); };
  inline BOOL filter(const LASpoint* point) { return (point->point_source_ID < below_point_source_id) ; };
  LAScriterionDropPointSourceBelow(U16 below_point_source_id) { this->below_point_source_id = below_point_source_id; };
private:
  U16 below_point_source_id;
};

class LAScriterionDropPointSourceAbove : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_point_source_above"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d ", name(), above_point_source_id); };
  inline BOOL filter(const LASpoint* point) { return (point->point_source_ID > above_point_source_id); };
  LAScriterionDropPointSourceAbove(U16 above_point_source_id) { this->above_point_source_id = above_point_source_id; };
private:
  U16 above_point_source_id;
};

class LAScriterionDropPointSourceBetween : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_point_source_between"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d %d ", name(), below_point_source_id, above_point_source_id); };
  inline BOOL filter(const LASpoint* point) { return (below_point_source_id <= point->point_source_ID) && (point->point_source_ID <= above_point_source_id); };
  LAScriterionDropPointSourceBetween(U16 below_point_source_id, U16 above_point_source_id) { this->below_point_source_id = below_point_source_id; this->above_point_source_id = above_point_source_id; };
private:
  U16 below_point_source_id, above_point_source_id;
};

class LAScriterionKeepGpsTime : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_gps_time"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %.6f %.6f ", name(), below_gpstime, above_gpstime); };
  inline BOOL filter(const LASpoint* point) { return (point->have_gps_time && ((point->gps_time < below_gpstime) || (point->gps_time > above_gpstime))); };
  LAScriterionKeepGpsTime(F64 below_gpstime, F64 above_gpstime) { this->below_gpstime = below_gpstime; this->above_gpstime = above_gpstime; };
private:
  F64 below_gpstime, above_gpstime;
};

class LAScriterionDropGpsTimeBelow : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_gps_time_below"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %.6f ", name(), below_gpstime); };
  inline BOOL filter(const LASpoint* point) { return (point->have_gps_time && (point->gps_time < below_gpstime)); };
  LAScriterionDropGpsTimeBelow(F64 below_gpstime) { this->below_gpstime = below_gpstime; };
private:
  F64 below_gpstime;
};

class LAScriterionDropGpsTimeAbove : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_gps_time_above"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %.6f ", name(), above_gpstime); };
  inline BOOL filter(const LASpoint* point) { return (point->have_gps_time && (point->gps_time > above_gpstime)); };
  LAScriterionDropGpsTimeAbove(F64 above_gpstime) { this->above_gpstime = above_gpstime; };
private:
  F64 above_gpstime;
};

class LAScriterionDropGpsTimeBetween : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_gps_time_between"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %.6f %.6f ", name(), below_gpstime, above_gpstime); };
  inline BOOL filter(const LASpoint* point) { return (point->have_gps_time && ((below_gpstime <= point->gps_time) && (point->gps_time <= above_gpstime))); };
  LAScriterionDropGpsTimeBetween(F64 below_gpstime, F64 above_gpstime) { this->below_gpstime = below_gpstime; this->above_gpstime = above_gpstime; };
private:
  F64 below_gpstime, above_gpstime;
};

class LAScriterionKeepWavepacket : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_wavepacket"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %u ", name(), keep_wavepacket); };
  inline BOOL filter(const LASpoint* point) { return (point->wavepacket.getIndex() != keep_wavepacket); };
  LAScriterionKeepWavepacket(U32 keep_wavepacket) { this->keep_wavepacket = keep_wavepacket; };
private:
  U32 keep_wavepacket;
};

class LAScriterionDropWavepacket : public LAScriterion
{
public:
  inline const CHAR* name() const { return "drop_wavepacket"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %u ", name(), drop_wavepacket); };
  inline BOOL filter(const LASpoint* point) { return (point->wavepacket.getIndex() == drop_wavepacket); };
  LAScriterionDropWavepacket(U32 drop_wavepacket) { this->drop_wavepacket = drop_wavepacket; };
private:
  U32 drop_wavepacket;
};

class LAScriterionKeepEveryNth : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_every_nth"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %d ", name(), every); };
  inline BOOL filter(const LASpoint* point) { if (counter == every) { counter = 1; return FALSE; } else { counter++; return TRUE; } };
  LAScriterionKeepEveryNth(I32 every) { this->every = every; counter = 1; };
private:
  I32 counter;
  I32 every;
};

class LAScriterionKeepRandomFraction : public LAScriterion
{
public:
  inline const CHAR* name() const { return "keep_random_fraction"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %g ", name(), fraction); };
  inline BOOL filter(const LASpoint* point)
  {
    //srand(seed);
    seed = R::runif(0, RAND_MAX);
    return ((F32)seed/(F32)RAND_MAX) > fraction;
  };
  void reset() { seed = 0; };
  LAScriterionKeepRandomFraction(F32 fraction) { seed = 0; this->fraction = fraction; };
private:
  U32 seed;
  F32 fraction;
};

class LAScriterionThinWithGrid : public LAScriterion
{
public:
  inline const CHAR* name() const { return "thin_with_grid"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %g ", name(), (grid_spacing > 0 ? grid_spacing : -grid_spacing)); };
  inline BOOL filter(const LASpoint* point)
  { 
    if (grid_spacing < 0)
    {
      grid_spacing = -grid_spacing;
      anker = I32_FLOOR(point->get_y() / grid_spacing);
    }
    I32 pos_x = I32_FLOOR(point->get_x() / grid_spacing);
    I32 pos_y = I32_FLOOR(point->get_y() / grid_spacing) - anker;
    BOOL no_x_anker = FALSE;
    U32* array_size;
    I32** ankers;
    U32*** array;
    U16** array_sizes;
    if (pos_y < 0)
    {
      pos_y = -pos_y - 1;
      ankers = &minus_ankers;
      if ((U32)pos_y < minus_plus_size && minus_plus_sizes[pos_y])
      {
        pos_x -= minus_ankers[pos_y];
        if (pos_x < 0)
        {
          pos_x = -pos_x - 1;
          array_size = &minus_minus_size;
          array = &minus_minus;
          array_sizes = &minus_minus_sizes;
        }
        else
        {
          array_size = &minus_plus_size;
          array = &minus_plus;
          array_sizes = &minus_plus_sizes;
        }
      }
      else
      {
        no_x_anker = TRUE;
        array_size = &minus_plus_size;
        array = &minus_plus;
        array_sizes = &minus_plus_sizes;
      }
    }
    else
    {
      ankers = &plus_ankers;
      if ((U32)pos_y < plus_plus_size && plus_plus_sizes[pos_y])
      {
        pos_x -= plus_ankers[pos_y];
        if (pos_x < 0)
        {
          pos_x = -pos_x - 1;
          array_size = &plus_minus_size;
          array = &plus_minus;
          array_sizes = &plus_minus_sizes;
        }
        else
        {
          array_size = &plus_plus_size;
          array = &plus_plus;
          array_sizes = &plus_plus_sizes;
        }
      }
      else
      {
        no_x_anker = TRUE;
        array_size = &plus_plus_size;
        array = &plus_plus;
        array_sizes = &plus_plus_sizes;
      }
    }
    // maybe grow banded grid in y direction
    if ((U32)pos_y >= *array_size)
    {
      U32 array_size_new = ((pos_y/1024)+1)*1024;
      if (*array_size)
      {
        if (array == &minus_plus || array == &plus_plus) *ankers = (I32*)realloc(*ankers, array_size_new*sizeof(I32));
        *array = (U32**)realloc(*array, array_size_new*sizeof(U32*));
        *array_sizes = (U16*)realloc(*array_sizes, array_size_new*sizeof(U16));
      }
      else
      {
        if (array == &minus_plus || array == &plus_plus) *ankers = (I32*)malloc(array_size_new*sizeof(I32));
        *array = (U32**)malloc(array_size_new*sizeof(U32*));
        *array_sizes = (U16*)malloc(array_size_new*sizeof(U16));
      }
      for (U32 i = *array_size; i < array_size_new; i++)
      {
        (*array)[i] = 0;
        (*array_sizes)[i] = 0;
      }
      *array_size = array_size_new;
    }
    // is this the first x anker for this y pos?
    if (no_x_anker)
    {
      (*ankers)[pos_y] = pos_x;
      pos_x = 0;
    }
    // maybe grow banded grid in x direction
    U32 pos_x_pos = pos_x/32;
    if (pos_x_pos >= (*array_sizes)[pos_y])
    {
      U32 array_sizes_new = ((pos_x_pos/256)+1)*256;
      if ((*array_sizes)[pos_y])
      {
        (*array)[pos_y] = (U32*)realloc((*array)[pos_y], array_sizes_new*sizeof(U32));
      }
      else
      {
        (*array)[pos_y] = (U32*)malloc(array_sizes_new*sizeof(U32));
      }
      for (U16 i = (*array_sizes)[pos_y]; i < array_sizes_new; i++)
      {
        (*array)[pos_y][i] = 0;
      }
      (*array_sizes)[pos_y] = array_sizes_new;
    }
    U32 pos_x_bit = 1 << (pos_x%32);
    if ((*array)[pos_y][pos_x_pos] & pos_x_bit) return TRUE;
    (*array)[pos_y][pos_x_pos] |= pos_x_bit;
    return FALSE;
  }
  void reset()
  {
    if (grid_spacing > 0) grid_spacing = -grid_spacing;
    if (minus_minus_size)
    {
      for (U32 i = 0; i < minus_minus_size; i++) if (minus_minus[i]) free(minus_minus[i]);
      free(minus_minus);
      minus_minus = 0;
      free(minus_minus_sizes);
      minus_minus_sizes = 0;
      minus_minus_size = 0;
    }
    if (minus_plus_size)
    {
      free(minus_ankers);
      minus_ankers = 0;
      for (U32 i = 0; i < minus_plus_size; i++) if (minus_plus[i]) free(minus_plus[i]);
      free(minus_plus);
      minus_plus = 0;
      free(minus_plus_sizes);
      minus_plus_sizes = 0;
      minus_plus_size = 0;
    }
    if (plus_minus_size)
    {
      for (U32 i = 0; i < plus_minus_size; i++) if (plus_minus[i]) free(plus_minus[i]);
      free(plus_minus);
      plus_minus = 0;
      free(plus_minus_sizes);
      plus_minus_sizes = 0;
      plus_minus_size = 0;
    }
    if (plus_plus_size)
    {
      free(plus_ankers);
      plus_ankers = 0;
      for (U32 i = 0; i < plus_plus_size; i++) if (plus_plus[i]) free(plus_plus[i]);
      free(plus_plus);
      plus_plus = 0;
      free(plus_plus_sizes);
      plus_plus_sizes = 0;
      plus_plus_size = 0;
    }
  };
  LAScriterionThinWithGrid(F32 grid_spacing)
  {
    this->grid_spacing = -grid_spacing;
    minus_ankers = 0;
    minus_minus_size = 0;
    minus_minus = 0;
    minus_minus_sizes = 0;
    minus_plus_size = 0;
    minus_plus = 0;
    minus_plus_sizes = 0;
    plus_ankers = 0;
    plus_minus_size = 0;
    plus_minus = 0;
    plus_minus_sizes = 0;
    plus_plus_size = 0;
    plus_plus = 0;
    plus_plus_sizes = 0;
  };
  ~LAScriterionThinWithGrid() { reset(); };
private:
  F32 grid_spacing;
  I32 anker;
  I32* minus_ankers;
  U32 minus_minus_size;
  U32** minus_minus;
  U16* minus_minus_sizes;
  U32 minus_plus_size;
  U32** minus_plus;
  U16* minus_plus_sizes;
  I32* plus_ankers;
  U32 plus_minus_size;
  U32** plus_minus;
  U16* plus_minus_sizes;
  U32 plus_plus_size;
  U32** plus_plus;
  U16* plus_plus_sizes;
};

class LAScriterionThinWithTime : public LAScriterion
{
public:
  inline const CHAR* name() const { return "thin_with_time"; };
  inline I32 get_command(CHAR* string) const { return sprintf(string, "-%s %g ", name(), (time_spacing > 0 ? time_spacing : -time_spacing)); };
  inline BOOL filter(const LASpoint* point)
  { 
    I64 pos_t = I64_FLOOR(point->get_gps_time() / time_spacing);
    my_I64_F64_map::iterator map_element = times.find(pos_t);
    if (map_element == times.end())
    {
      times.insert(my_I64_F64_map::value_type(pos_t, point->get_gps_time()));
      return FALSE;
    }
    else if ((*map_element).second == point->get_gps_time())
    {
      return FALSE;
    }
    else
    {
      return TRUE;
    }
  }
  void reset()
  {
    times.clear();
  };
  LAScriterionThinWithTime(F64 time_spacing)
  {
    this->time_spacing = time_spacing;
  };
  ~LAScriterionThinWithTime() { reset(); };
private:
  F64 time_spacing;
  my_I64_F64_map times;
};

void LASfilter::clean()
{
  U32 i;
  for (i = 0; i < num_criteria; i++)
  {
    delete criteria[i];
  }
  if (criteria) delete [] criteria;
  if (counters) delete [] counters;
  alloc_criteria = 0;
  num_criteria = 0;
  criteria = 0;
  counters = 0;
}

void LASfilter::usage() const
{
  Rcpp::Rcerr << "Filter points based on their coordinates." << std::endl;
  Rcpp::Rcerr << "  -keep_tile 631000 4834000 1000 (ll_x ll_y size)" << std::endl;
  Rcpp::Rcerr << "  -keep_circle 630250.00 4834750.00 100 (x y radius)" << std::endl;
  Rcpp::Rcerr << "  -keep_xy 630000 4834000 631000 4836000 (min_x min_y max_x max_y)" << std::endl;
  Rcpp::Rcerr << "  -drop_xy 630000 4834000 631000 4836000 (min_x min_y max_x max_y)" << std::endl;
  Rcpp::Rcerr << "  -keep_x 631500.50 631501.00 (min_x max_x)" << std::endl;
  Rcpp::Rcerr << "  -drop_x 631500.50 631501.00 (min_x max_x)" << std::endl;
  Rcpp::Rcerr << "  -drop_x_below 630000.50 (min_x)" << std::endl;
  Rcpp::Rcerr << "  -drop_x_above 630500.50 (max_x)" << std::endl;
  Rcpp::Rcerr << "  -keep_y 4834500.25 4834550.25 (min_y max_y)" << std::endl;
  Rcpp::Rcerr << "  -drop_y 4834500.25 4834550.25 (min_y max_y)" << std::endl;
  Rcpp::Rcerr << "  -drop_y_below 4834500.25 (min_y)" << std::endl;
  Rcpp::Rcerr << "  -drop_y_above 4836000.75 (max_y)" << std::endl;
  Rcpp::Rcerr << "  -keep_z 11.125 130.725 (min_z max_z)" << std::endl;
  Rcpp::Rcerr << "  -drop_z 11.125 130.725 (min_z max_z)" << std::endl;
  Rcpp::Rcerr << "  -drop_z_below 11.125 (min_z)" << std::endl;
  Rcpp::Rcerr << "  -drop_z_above 130.725 (max_z)" << std::endl;
  Rcpp::Rcerr << "  -keep_xyz 620000 4830000 100 621000 4831000 200 (min_x min_y min_z max_x max_y max_z)" << std::endl;
  Rcpp::Rcerr << "  -drop_xyz 620000 4830000 100 621000 4831000 200 (min_x min_y min_z max_x max_y max_z)" << std::endl;
  Rcpp::Rcerr << "Filter points based on their return number." << std::endl;
  Rcpp::Rcerr << "  -keep_first -first_only -drop_first" << std::endl;
  Rcpp::Rcerr << "  -keep_last -last_only -drop_last" << std::endl;
  Rcpp::Rcerr << "  -keep_first_of_many -keep_last_of_many" << std::endl;
  Rcpp::Rcerr << "  -drop_first_of_many -drop_last_of_many" << std::endl;
  Rcpp::Rcerr << "  -keep_middle -drop_middle" << std::endl;
  Rcpp::Rcerr << "  -keep_return 1 2 3" << std::endl;
  Rcpp::Rcerr << "  -drop_return 3 4" << std::endl;
  Rcpp::Rcerr << "  -keep_single -drop_single" << std::endl;
  Rcpp::Rcerr << "  -keep_double -drop_double" << std::endl;
  Rcpp::Rcerr << "  -keep_triple -drop_triple" << std::endl;
  Rcpp::Rcerr << "  -keep_quadruple -drop_quadruple" << std::endl;
  Rcpp::Rcerr << "  -keep_quintuple -drop_quintuple" << std::endl;
  Rcpp::Rcerr << "Filter points based on the scanline flags." << std::endl;
  Rcpp::Rcerr << "  -drop_scan_direction 0" << std::endl;
  Rcpp::Rcerr << "  -keep_scan_direction_change" << std::endl;
  Rcpp::Rcerr << "  -keep_edge_of_flight_line" << std::endl;
  Rcpp::Rcerr << "Filter points based on their intensity." << std::endl;
  Rcpp::Rcerr << "  -keep_intensity 20 380" << std::endl;
  Rcpp::Rcerr << "  -drop_intensity_below 20" << std::endl;
  Rcpp::Rcerr << "  -drop_intensity_above 380" << std::endl;
  Rcpp::Rcerr << "  -drop_intensity_between 4000 5000" << std::endl;
  Rcpp::Rcerr << "Filter points based on classifications or flags." << std::endl;
  Rcpp::Rcerr << "  -keep_class 1 3 7" << std::endl;
  Rcpp::Rcerr << "  -drop_class 4 2" << std::endl;
  Rcpp::Rcerr << "  -keep_extended_class 43" << std::endl;
  Rcpp::Rcerr << "  -drop_extended_class 129 135" << std::endl;
  Rcpp::Rcerr << "  -drop_synthetic -keep_synthetic" << std::endl;
  Rcpp::Rcerr << "  -drop_keypoint -keep_keypoint" << std::endl;
  Rcpp::Rcerr << "  -drop_withheld -keep_withheld" << std::endl;
  Rcpp::Rcerr << "  -drop_overlap -keep_overlap" << std::endl;
  Rcpp::Rcerr << "Filter points based on their user data." << std::endl;
  Rcpp::Rcerr << "  -keep_user_data 1" << std::endl;
  Rcpp::Rcerr << "  -drop_user_data 255" << std::endl;
  Rcpp::Rcerr << "  -keep_user_data_below 50" << std::endl;
  Rcpp::Rcerr << "  -keep_user_data_above 150" << std::endl;
  Rcpp::Rcerr << "  -keep_user_data_between 10 20" << std::endl;
  Rcpp::Rcerr << "  -drop_user_data_below 1" << std::endl;
  Rcpp::Rcerr << "  -drop_user_data_above 100" << std::endl;
  Rcpp::Rcerr << "  -drop_user_data_between 10 40" << std::endl;
  Rcpp::Rcerr << "Filter points based on their point source ID." << std::endl;
  Rcpp::Rcerr << "  -keep_point_source 3" << std::endl;
  Rcpp::Rcerr << "  -keep_point_source_between 2 6" << std::endl;
  Rcpp::Rcerr << "  -drop_point_source 27" << std::endl;
  Rcpp::Rcerr << "  -drop_point_source_below 6" << std::endl;
  Rcpp::Rcerr << "  -drop_point_source_above 15" << std::endl;
  Rcpp::Rcerr << "  -drop_point_source_between 17 21" << std::endl;
  Rcpp::Rcerr << "Filter points based on their scan angle." << std::endl;
  Rcpp::Rcerr << "  -keep_scan_angle -15 15" << std::endl;
  Rcpp::Rcerr << "  -drop_abs_scan_angle_above 15" << std::endl;
  Rcpp::Rcerr << "  -drop_abs_scan_angle_below 1" << std::endl;
  Rcpp::Rcerr << "  -drop_scan_angle_below -15" << std::endl;
  Rcpp::Rcerr << "  -drop_scan_angle_above 15" << std::endl;
  Rcpp::Rcerr << "  -drop_scan_angle_between -25 -23" << std::endl;
  Rcpp::Rcerr << "Filter points based on their gps time." << std::endl;
  Rcpp::Rcerr << "  -keep_gps_time 11.125 130.725" << std::endl;
  Rcpp::Rcerr << "  -drop_gps_time_below 11.125" << std::endl;
  Rcpp::Rcerr << "  -drop_gps_time_above 130.725" << std::endl;
  Rcpp::Rcerr << "  -drop_gps_time_between 22.0 48.0" << std::endl;
  Rcpp::Rcerr << "Filter points based on their RGB/NIR channel." << std::endl;
  Rcpp::Rcerr << "  -keep_RGB_red 1 1" << std::endl;
  Rcpp::Rcerr << "  -keep_RGB_green 30 100" << std::endl;
  Rcpp::Rcerr << "  -keep_RGB_blue 0 0" << std::endl;
  Rcpp::Rcerr << "  -keep_RGB_nir 64 127" << std::endl;
  Rcpp::Rcerr << "Filter points based on their wavepacket." << std::endl;
  Rcpp::Rcerr << "  -keep_wavepacket 0" << std::endl;
  Rcpp::Rcerr << "  -drop_wavepacket 3" << std::endl;
  Rcpp::Rcerr << "Filter points with simple thinning." << std::endl;
  Rcpp::Rcerr << "  -keep_every_nth 2" << std::endl;
  Rcpp::Rcerr << "  -keep_random_fraction 0.1" << std::endl;
  Rcpp::Rcerr << "  -thin_with_grid 1.0" << std::endl;
  Rcpp::Rcerr << "  -thin_with_time 0.001" << std::endl;
  Rcpp::Rcerr << "Boolean combination of filters." << std::endl;
  Rcpp::Rcerr << "  -filter_and" << std::endl;
}

BOOL LASfilter::parse(int argc, char* argv[])
{
  int i;

  U32 keep_return_mask = 0;
  U32 drop_return_mask = 0;

  U32 keep_classification_mask = 0;
  U32 drop_classification_mask = 0;

  U32 keep_extended_classification_mask[8] = {0, 0, 0, 0, 0, 0, 0, 0};
  U32 drop_extended_classification_mask[8] = {0, 0, 0, 0, 0, 0, 0, 0};

  for (i = 1; i < argc; i++)
  {
    if (argv[i][0] == '\0')
    {
      continue;
    }
    else if (strcmp(argv[i],"-h") == 0 || strcmp(argv[i],"-help") == 0)
    {
      usage();
      return TRUE;
    }
    else if (strncmp(argv[i],"-clip_", 6) == 0)
    {
      if (strcmp(argv[i], "-clip_z_below") == 0)
      {
        Rcpp::Rcerr << "WARNING: '" << argv[i] << "' will not be supported in the future. check documentation with '-h'." << std::endl;
        Rcpp::Rcerr << "  rename '-clip_z_below' to '-drop_z_below'." << std::endl;
        Rcpp::Rcerr << "  rename '-clip_z_above' to '-drop_z_above'." << std::endl;
        Rcpp::Rcerr << "  rename '-clip_z_between' to '-drop_z'." << std::endl;
        Rcpp::Rcerr << "  rename '-clip' to '-keep_xy'." << std::endl;
        Rcpp::Rcerr << "  rename '-clip_tile' to '-keep_tile'." << std::endl;
        if ((i+1) >= argc)
        {
          Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: max_z" << std::endl;
          return FALSE;
        }
        add_criterion(new LAScriterionDropzBelow(atof(argv[i+1])));
        *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
      }
      else if (strcmp(argv[i], "-clip_z_above") == 0)
      {
        Rcpp::Rcerr << "WARNING: '" << argv[i] << "' will not be supported in the future. check documentation with '-h'." << std::endl;
        Rcpp::Rcerr << "  rename '-clip_z_below' to '-drop_z_below'." << std::endl;
        Rcpp::Rcerr << "  rename '-clip_z_above' to '-drop_z_above'." << std::endl;
        Rcpp::Rcerr << "  rename '-clip_z_between' to '-drop_z'." << std::endl;
        Rcpp::Rcerr << "  rename '-clip' to '-keep_xy'." << std::endl;
        Rcpp::Rcerr << "  rename '-clip_tile' to '-keep_tile'." << std::endl;
        if ((i+1) >= argc)
        {
          Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: max_z" << std::endl;
          return FALSE;
        }
        add_criterion(new LAScriterionDropzAbove(atof(argv[i+1])));
        *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
      }
      else if ((strcmp(argv[i], "-clip_to_bounding_box") != 0) && (strcmp(argv[i],"-clip_to_bb") != 0))
      {
        Rcpp::Rcerr << "ERROR: '" << argv[i] << "' is no longer recognized. check documentation with '-h'." << std::endl;
        Rcpp::Rcerr << "  rename '-clip' to '-keep_xy'." << std::endl;
        Rcpp::Rcerr << "  rename '-clip_box' to '-keep_xyz'." << std::endl;
        Rcpp::Rcerr << "  rename '-clip_tile' to '-keep_tile'." << std::endl;
        Rcpp::Rcerr << "  rename '-clip_z_below' to '-drop_z_below'." << std::endl;
        Rcpp::Rcerr << "  rename '-clip_z_above' to '-drop_z_above'." << std::endl;
        Rcpp::Rcerr << "  rename '-clip_z_between' to '-drop_z'." << std::endl;
        Rcpp::Rcerr << "  etc ..." << std::endl;
        return FALSE;
      }
    }
    else if (strncmp(argv[i],"-keep_", 6) == 0)
    {
      if (strncmp(argv[i],"-keep_first", 11) == 0)
      {
        if (strcmp(argv[i],"-keep_first") == 0)
        {
          add_criterion(new LAScriterionKeepFirstReturn());
          *argv[i]='\0';
        }
        else if (strcmp(argv[i],"-keep_first_of_many") == 0)
        {
          add_criterion(new LAScriterionKeepFirstOfManyReturn());
          *argv[i]='\0';
        }
      }
      else if (strcmp(argv[i],"-keep_middle") == 0)
      {
        add_criterion(new LAScriterionKeepMiddleReturn());
        *argv[i]='\0';
      }
      else if (strncmp(argv[i],"-keep_last", 10) == 0)
      {
        if (strcmp(argv[i],"-keep_last") == 0)
        {
          add_criterion(new LAScriterionKeepLastReturn());
          *argv[i]='\0';
        }
        else if (strcmp(argv[i],"-keep_last_of_many") == 0)
        {
          add_criterion(new LAScriterionKeepLastOfManyReturn());
          *argv[i]='\0';
        }
      }
      else if (strncmp(argv[i],"-keep_class", 11) == 0)
      {
        if (strcmp(argv[i],"-keep_classification") == 0 || strcmp(argv[i],"-keep_class") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 at least argument: classification" << std::endl;
            return FALSE;
          }
          *argv[i]='\0';
          i+=1;
          do
          {
            if (atoi(argv[i]) > 31)
            {
              Rcpp::Rcerr << "ERROR: cannot keep classification " << atoi(argv[i]) << " because it is larger than 31" << std::endl;
              return FALSE;
            }
            else if (atoi(argv[i]) < 0)
            {
              Rcpp::Rcerr << "ERROR: cannot keep classification " << atoi(argv[i]) << " because it is smaller than 0" << std::endl;
              return FALSE;
            }
            keep_classification_mask |= (1 << atoi(argv[i]));
            *argv[i]='\0';
            i+=1;
          } while ((i < argc) && ('0' <= *argv[i]) && (*argv[i] <= '9'));
          i-=1;
        }
      }
      else if (strncmp(argv[i],"-keep_extended_", 15) == 0)
      {
        if (strcmp(argv[i],"-keep_extended_classification") == 0 || strcmp(argv[i],"-keep_extended_class") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 at least argument: classification" << std::endl;
            return FALSE;
          }
          *argv[i]='\0';
          i+=1;
          do
          {
            if (atoi(argv[i]) > 255)
            {
              Rcpp::Rcerr << "ERROR: cannot keep extended classification " << atoi(argv[i]) << " because it is larger than 255" << std::endl;
              return FALSE;
            }
            else if (atoi(argv[i]) < 0)
            {
              Rcpp::Rcerr << "ERROR: cannot keep extended classification " << atoi(argv[i]) << " because it is smaller than 0" << std::endl;
              return FALSE;
            }
            keep_extended_classification_mask[atoi(argv[i])/32] |= (1 << (atoi(argv[i]) - (32*(atoi(argv[i])/32))));
            *argv[i]='\0';
            i+=1;
          } while ((i < argc) && ('0' <= *argv[i]) && (*argv[i] <= '9'));
          i-=1;
        }
      }
      else if (strncmp(argv[i],"-keep_x", 7) == 0)
      {
        if (strcmp(argv[i],"-keep_xy") == 0)
        {
          if ((i+4) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 4 arguments: min_x min_y max_x max_y" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionKeepxy(atof(argv[i+1]), atof(argv[i+2]), atof(argv[i+3]), atof(argv[i+4])));
          *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; *argv[i+3]='\0'; *argv[i+4]='\0'; i+=4; 
        }
        else if (strcmp(argv[i],"-keep_xyz") == 0)
        {
          if ((i+6) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 6 arguments: min_x min_y min_z max_x max_y max_z" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionKeepxyz(atof(argv[i+1]), atof(argv[i+2]), atof(argv[i+3]), atof(argv[i+4]), atof(argv[i+5]), atof(argv[i+6])));
          *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; *argv[i+3]='\0'; *argv[i+4]='\0'; *argv[i+5]='\0'; *argv[i+6]='\0'; i+=6; 
        }
        else if (strcmp(argv[i],"-keep_x") == 0)
        {
          if ((i+2) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 2 arguments: min_x max_x" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionKeepx(atof(argv[i+1]), atof(argv[i+2])));
          *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; i+=2;
        }
      }
      else if (strcmp(argv[i],"-keep_y") == 0)
      {
        if ((i+2) >= argc)
        {
          Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 2 arguments: min_y max_y" << std::endl;
          return FALSE;
        }
        add_criterion(new LAScriterionKeepy(atof(argv[i+1]), atof(argv[i+2])));
        *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; i+=2;
      }
      else if (strcmp(argv[i],"-keep_z") == 0)
      {
        if ((i+2) >= argc)
        {
          Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 2 arguments: min_z max_z" << std::endl;
          return FALSE;
        }
        add_criterion(new LAScriterionKeepz(atof(argv[i+1]), atof(argv[i+2])));
        *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; i+=2;
      }
      else if (strncmp(argv[i],"-keep_X", 7) == 0)
      {
        if (strcmp(argv[i],"-keep_XY") == 0)
        {
          if ((i+4) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 4 arguments: min_X min_Y max_X max_Y" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionKeepXY(atoi(argv[i+1]), atoi(argv[i+2]), atoi(argv[i+3]), atoi(argv[i+4])));
          *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; *argv[i+3]='\0'; *argv[i+4]='\0'; i+=4; 
        }
        else if (strcmp(argv[i],"-keep_X") == 0)
        {
          if ((i+2) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 2 arguments: min_X max_X" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionKeepX(atoi(argv[i+1]), atoi(argv[i+2])));
          *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; i+=2; 
        }
      }
      else if (strcmp(argv[i],"-keep_Y") == 0)
      {
        if ((i+2) >= argc)
        {
          Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 2 arguments: min_Y max_Y" << std::endl;
          return FALSE;
        }
        add_criterion(new LAScriterionKeepY(atoi(argv[i+1]), atoi(argv[i+2])));
        *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; i+=2; 
      }
      else if (strcmp(argv[i],"-keep_Z") == 0)
      {
        if ((i+2) >= argc)
        {
          Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 2 arguments: min_Z max_Z" << std::endl;
          return FALSE;
        }
        add_criterion(new LAScriterionKeepZ(atoi(argv[i+1]), atoi(argv[i+2])));
        *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; i+=2; 
      }
      else if (strcmp(argv[i],"-keep_tile") == 0)
      {
        if ((i+3) >= argc)
        {
          Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 3 arguments: llx lly size" << std::endl;
          return FALSE;
        }
        add_criterion(new LAScriterionKeepTile((F32)atof(argv[i+1]), (F32)atof(argv[i+2]), (F32)atof(argv[i+3])));
        *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; *argv[i+3]='\0'; i+=3; 
      }
      else if (strcmp(argv[i],"-keep_circle") == 0)
      {
        if ((i+3) >= argc)
        {
          Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 3 arguments: center_x center_y radius" << std::endl;
          return FALSE;
        }
        add_criterion(new LAScriterionKeepCircle(atof(argv[i+1]), atof(argv[i+2]), atof(argv[i+3])));
        *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; *argv[i+3]='\0'; i+=3;
      }
      else if (strcmp(argv[i],"-keep_return") == 0)
      {
        if ((i+1) >= argc)
        {
          Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs at least 1 argument: return_number" << std::endl;
          return FALSE;
        }
        *argv[i]='\0';
        i+=1;
        do
        {
          keep_return_mask |= (1 << atoi(argv[i]));
          *argv[i]='\0';
          i+=1;
        } while ((i < argc) && ('0' <= *argv[i]) && (*argv[i] <= '9'));
        i-=1;
      }
      else if (strcmp(argv[i],"-keep_return_mask") == 0)
      {
        if ((i+1) >= argc)
        {
          Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: return_mask" << std::endl;
          return FALSE;
        }
        keep_return_mask = atoi(argv[i+1]);
        *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
      }
      else if (strcmp(argv[i],"-keep_single") == 0)
      {
        add_criterion(new LAScriterionKeepSpecificNumberOfReturns(1));
        *argv[i]='\0';
      }
      else if (strcmp(argv[i],"-keep_double") == 0)
      {
        add_criterion(new LAScriterionKeepSpecificNumberOfReturns(2));
        *argv[i]='\0';
      }
      else if (strcmp(argv[i],"-keep_triple") == 0)
      {
        add_criterion(new LAScriterionKeepSpecificNumberOfReturns(3));
        *argv[i]='\0';
      }
      else if (strcmp(argv[i],"-keep_quadruple") == 0)
      {
        add_criterion(new LAScriterionKeepSpecificNumberOfReturns(4));
        *argv[i]='\0';
      }
      else if (strcmp(argv[i],"-keep_quintuple") == 0)
      {
        add_criterion(new LAScriterionKeepSpecificNumberOfReturns(5));
        *argv[i]='\0';
      }
      else if (strncmp(argv[i],"-keep_intensity", 15) == 0)
      {
        if (strcmp(argv[i],"-keep_intensity") == 0)
        {
          if ((i+2) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 2 arguments: min max" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionKeepIntensity(atoi(argv[i+1]), atoi(argv[i+2])));
          *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; i+=2;
        }
        else if (strcmp(argv[i],"-keep_intensity_above") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: max" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionKeepIntensityAbove(atoi(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
        else if (strcmp(argv[i],"-keep_intensity_below") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: min" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionKeepIntensityBelow(atoi(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
      }
      else if (strncmp(argv[i],"-keep_RGB_", 10) == 0)
      {
        if (strcmp(argv[i]+10,"red") == 0)
        {
          if ((i+2) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 2 arguments: min max" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionKeepRGB(atoi(argv[i+1]), atoi(argv[i+2]), 0));
          *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; i+=2;
        }
        else if (strcmp(argv[i]+10,"green") == 0)
        {
          if ((i+2) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 2 arguments: min max" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionKeepRGB(atoi(argv[i+1]), atoi(argv[i+2]), 1));
          *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; i+=2;
        }
        else if (strcmp(argv[i]+10,"blue") == 0)
        {
          if ((i+2) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 2 arguments: min max" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionKeepRGB(atoi(argv[i+1]), atoi(argv[i+2]), 2));
          *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; i+=2;
        }
        else if (strcmp(argv[i]+10,"nir") == 0)
        {
          if ((i+2) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 2 arguments: min max" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionKeepRGB(atoi(argv[i+1]), atoi(argv[i+2]), 3));
          *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; i+=2;
        }
      }
      else if (strcmp(argv[i],"-keep_scan_angle") == 0)
      {
        if ((i+2) >= argc)
        {
          Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 2 arguments: min max" << std::endl;
          return FALSE;
        }
        add_criterion(new LAScriterionKeepScanAngle(atoi(argv[i+1]), atoi(argv[i+2])));
        *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; i+=2;
      }
      else if (strcmp(argv[i],"-keep_synthetic") == 0)
      {
        add_criterion(new LAScriterionKeepSynthetic());
        *argv[i]='\0';
      }
      else if (strcmp(argv[i],"-keep_keypoint") == 0)
      {
        add_criterion(new LAScriterionKeepKeypoint());
        *argv[i]='\0';
      }
      else if (strcmp(argv[i],"-keep_withheld") == 0)
      {
        add_criterion(new LAScriterionKeepWithheld());
        *argv[i]='\0';
      }
      else if (strcmp(argv[i],"-keep_overlap") == 0)
      {
        add_criterion(new LAScriterionKeepOverlap());
        *argv[i]='\0';
      }
      else if (strcmp(argv[i],"-keep_wavepacket") == 0)
      {
        if ((i+1) >= argc)
        {
          Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: index" << std::endl;
          return FALSE;
        }
        *argv[i]='\0';
        i+=1;
        add_criterion(new LAScriterionKeepWavepacket(atoi(argv[i])));
        *argv[i]='\0';
      }
      else if (strncmp(argv[i],"-keep_user_data", 15) == 0)
      {
        if (strcmp(argv[i],"-keep_user_data") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: value" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionKeepUserData(atoi(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
        else if (strcmp(argv[i],"-keep_user_data_below") == 0)
        {
          if ((i+2) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: value" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionKeepUserDataBelow(atoi(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
        else if (strcmp(argv[i],"-keep_user_data_above") == 0)
        {
          if ((i+2) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: value" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionKeepUserDataAbove(atoi(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
        else if (strcmp(argv[i],"-keep_user_data_between") == 0)
        {
          if ((i+2) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 2 arguments: min_value max_value" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionKeepUserDataBetween(atoi(argv[i+1]), atoi(argv[i+2])));
          *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; i+=2;
        }
      }
      else if (strncmp(argv[i],"-keep_point_source", 18) == 0)
      {
        if (strcmp(argv[i],"-keep_point_source") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: ID" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionKeepPointSource(atoi(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
        else if (strcmp(argv[i],"-keep_point_source_between") == 0)
        {
          if ((i+2) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 2 arguments: min_ID max_ID" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionKeepPointSourceBetween(atoi(argv[i+1]), atoi(argv[i+2])));
          *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; i+=2;
        }
      }
      else if (strncmp(argv[i],"-keep_gps", 9) == 0)
      {
        if (strcmp(argv[i],"-keep_gps_time") == 0 || strcmp(argv[i],"-keep_gpstime") == 0)
        {
          if ((i+2) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 2 arguments: min max" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionKeepGpsTime(atof(argv[i+1]), atof(argv[i+2])));
          *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; i+=2;
        }
      }
      else if (strcmp(argv[i],"-keep_every_nth") == 0)
      {
        if ((i+1) >= argc)
        {
          Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: nth" << std::endl;
          return FALSE;
        }
        add_criterion(new LAScriterionKeepEveryNth((I32)atoi(argv[i+1])));
        *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
      }
      else if (strcmp(argv[i],"-keep_random_fraction") == 0)
      {
        if ((i+1) >= argc)
        {
          Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: fraction" << std::endl;
          return FALSE;
        }
        add_criterion(new LAScriterionKeepRandomFraction((F32)atof(argv[i+1])));
        *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
      }
      else if (strcmp(argv[i],"-keep_scan_direction_change") == 0)
      {
        add_criterion(new LAScriterionKeepScanDirectionChange());
        *argv[i]='\0';
      }
      else if (strcmp(argv[i],"-keep_edge_of_flight_line") == 0)
      {
        add_criterion(new LAScriterionKeepEdgeOfFlightLine());
        *argv[i]='\0';
      }
    }
    else if (strncmp(argv[i],"-drop_", 6) == 0)
    {
      if (strncmp(argv[i],"-drop_first", 11) == 0)
      {
        if (strcmp(argv[i],"-drop_first") == 0)
        {
          add_criterion(new LAScriterionDropFirstReturn());
          *argv[i]='\0';
        }
        else if (strcmp(argv[i],"-drop_first_of_many") == 0)
        {
          add_criterion(new LAScriterionDropFirstOfManyReturn());
          *argv[i]='\0';
        }
      }
      else if (strncmp(argv[i],"-drop_last", 10) == 0)
      {
        if (strcmp(argv[i],"-drop_last") == 0)
        {
          add_criterion(new LAScriterionDropLastReturn());
          *argv[i]='\0';
        }
        else if (strcmp(argv[i],"-drop_last_of_many") == 0)
        {
          add_criterion(new LAScriterionDropLastOfManyReturn());
          *argv[i]='\0';
        }
      }
      else if (strcmp(argv[i],"-drop_middle") == 0)
      {
        add_criterion(new LAScriterionDropMiddleReturn());
        *argv[i]='\0';
      }
      else if (strncmp(argv[i],"-drop_class", 11) == 0)
      {
        if (strcmp(argv[i],"-drop_classification") == 0 || strcmp(argv[i],"-drop_class") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs at least 1 argument: classification" << std::endl;
            return FALSE;
          }
          *argv[i]='\0';
          i+=1;
          do
          {
            if (atoi(argv[i]) > 31)
            {
              Rcpp::Rcerr << "ERROR: cannot drop classification " << atoi(argv[i]) << " because it is larger than 31" << std::endl;
              return FALSE;
            }
            else if (atoi(argv[i]) < 0)
            {
              Rcpp::Rcerr << "ERROR: cannot drop classification " << atoi(argv[i]) << " because it is smaller than 0" << std::endl;
              return FALSE;
            }
            drop_classification_mask |= (1 << atoi(argv[i]));
            *argv[i]='\0';
            i+=1;
          } while ((i < argc) && ('0' <= *argv[i]) && (*argv[i] <= '9'));
          i-=1;
        }
        else if (strcmp(argv[i],"-drop_classification_mask") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: mask" << std::endl;
            return FALSE;
          }
          drop_classification_mask = atoi(argv[i+1]);
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
      }
      else if (strncmp(argv[i],"-drop_extended_", 15) == 0)
      {
        if (strcmp(argv[i],"-drop_extended_classification") == 0 || strcmp(argv[i],"-drop_extended_class") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs at least 1 argument: classification" << std::endl;
            return FALSE;
          }
          *argv[i]='\0';
          i+=1;
          do
          {
            if (atoi(argv[i]) > 255)
            {
              Rcpp::Rcerr << "ERROR: cannot drop extended classification " << atoi(argv[i]) << " because it is larger than 255" << std::endl;
              return FALSE;
            }
            else if (atoi(argv[i]) < 0)
            {
              Rcpp::Rcerr << "ERROR: cannot drop extended classification " << atoi(argv[i]) << " because it is smaller than 0" << std::endl;
              return FALSE;
            }
            drop_extended_classification_mask[atoi(argv[i])/32] |= (1 << (atoi(argv[i]) - (32*(atoi(argv[i])/32))));
            *argv[i]='\0';
            i+=1;
          } while ((i < argc) && ('0' <= *argv[i]) && (*argv[i] <= '9'));
          i-=1;
        }
        else if (strcmp(argv[i],"-drop_extended_classification_mask") == 0)
        {
          if ((i+8) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 8 arguments: mask7 mask6 mask5 mask4 mask3 mask2 mask1 mask0" << std::endl;
            return FALSE;
          }
          drop_extended_classification_mask[7] = atoi(argv[i+1]);
          drop_extended_classification_mask[6] = atoi(argv[i+2]);
          drop_extended_classification_mask[5] = atoi(argv[i+3]);
          drop_extended_classification_mask[4] = atoi(argv[i+4]);
          drop_extended_classification_mask[3] = atoi(argv[i+5]);
          drop_extended_classification_mask[2] = atoi(argv[i+6]);
          drop_extended_classification_mask[1] = atoi(argv[i+7]);
          drop_extended_classification_mask[0] = atoi(argv[i+8]);
          *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; *argv[i+3]='\0'; *argv[i+4]='\0'; *argv[i+5]='\0'; *argv[i+6]='\0'; *argv[i+7]='\0'; *argv[i+8]='\0'; i+=8;
        }
      }
      else if (strncmp(argv[i],"-drop_x", 7) == 0)
      {
        if (strcmp(argv[i],"-drop_xy") == 0)
        {
          if ((i+4) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 4 arguments: min_x min_y max_x max_y" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropxy(atof(argv[i+1]), atof(argv[i+2]), atof(argv[i+3]), atof(argv[i+4])));
          *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; *argv[i+3]='\0'; *argv[i+4]='\0'; i+=4; 
        }
        else if (strcmp(argv[i],"-drop_xyz") == 0)
        {
          if ((i+6) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 6 arguments: min_x min_y min_z max_x max_y max_z" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropxyz(atof(argv[i+1]), atof(argv[i+2]), atof(argv[i+3]), atof(argv[i+4]), atof(argv[i+5]), atof(argv[i+6])));
          *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; *argv[i+3]='\0'; *argv[i+4]='\0'; *argv[i+5]='\0'; *argv[i+6]='\0'; i+=6; 
        }
        else if (strcmp(argv[i],"-drop_x") == 0)
        {
          if ((i+2) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 2 arguments: min_x max_x" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropx(atof(argv[i+1]), atof(argv[i+2])));
          *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; i+=2;
        }
        else if (strcmp(argv[i],"-drop_x_below") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: min_x" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropxBelow(atof(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
        else if (strcmp(argv[i],"-drop_x_above") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: max_x" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropxAbove(atof(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
      }
      else if (strncmp(argv[i],"-drop_y", 7) == 0)
      {
        if (strcmp(argv[i],"-drop_y") == 0)
        {
          if ((i+2) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 2 arguments: min_y max_y" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropy(atof(argv[i+1]), atof(argv[i+2])));
          *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; i+=2;
        }
        else if (strcmp(argv[i],"-drop_y_below") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: min_y" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropyBelow(atof(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
        else if (strcmp(argv[i],"-drop_y_above") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: max_y" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropyAbove(atof(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
      }
      else if (strncmp(argv[i],"-drop_z", 7) == 0)
      {
        if (strcmp(argv[i],"-drop_z") == 0)
        {
          if ((i+2) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 2 arguments: min_z max_z" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropz(atof(argv[i+1]), atof(argv[i+2])));
          *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; i+=2;
        }
        else if (strcmp(argv[i],"-drop_z_below") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: min_z" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropzBelow(atof(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
        else if (strcmp(argv[i],"-drop_z_above") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: max_z" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropzAbove(atof(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
      }
      else if (strncmp(argv[i],"-drop_X", 7) == 0)
      {
        if (strcmp(argv[i],"-drop_X") == 0)
        {
          if ((i+2) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 2 arguments: min_X max_X" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropX(atoi(argv[i+1]), atoi(argv[i+2])));
          *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; i+=2;
        }
        else if (strcmp(argv[i],"-drop_X_below") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: min_X" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropXBelow(atoi(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
        else if (strcmp(argv[i],"-drop_X_above") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: max_X" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropXAbove(atoi(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
      }
      else if (strncmp(argv[i],"-drop_Y", 7) == 0)
      {
        if (strcmp(argv[i],"-drop_Y") == 0)
        {
          if ((i+2) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 2 arguments: min_Y max_Y" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropY(atoi(argv[i+1]), atoi(argv[i+2])));
          *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; i+=2;
        }
        else if (strcmp(argv[i],"-drop_Y_below") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: min_Y" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropYBelow(atoi(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
        else if (strcmp(argv[i],"-drop_Y_above") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: max_Y" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropYAbove(atoi(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
      }
      else if (strncmp(argv[i],"-drop_Z", 7) == 0)
      {
        if (strcmp(argv[i],"-drop_Z") == 0)
        {
          if ((i+2) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 2 arguments: min_Z max_Z" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropZ(atoi(argv[i+1]), atoi(argv[i+2])));
          *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; i+=2;
        }
        else if (strcmp(argv[i],"-drop_Z_below") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: min_Z" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropZBelow(atoi(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
        else if (strcmp(argv[i],"-drop_Z_above") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: max_Z" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropZAbove(atoi(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
      }
      else if (strcmp(argv[i],"-drop_return") == 0)
      {
        if ((i+1) >= argc)
        {
          Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs at least 1 argument: return_number" << std::endl;
          return FALSE;
        }
        *argv[i]='\0';
        i+=1;
        do
        {
          drop_return_mask |= (1 << atoi(argv[i]));
          *argv[i]='\0';
          i+=1;
        } while ((i < argc) && ('0' <= *argv[i]) && (*argv[i] <= '9'));
        i-=1;
      }
      else if (strcmp(argv[i],"-drop_single") == 0)
      {
        add_criterion(new LAScriterionDropSpecificNumberOfReturns(1));
        *argv[i]='\0';
      }
      else if (strcmp(argv[i],"-drop_double") == 0)
      {
        add_criterion(new LAScriterionDropSpecificNumberOfReturns(2));
        *argv[i]='\0';
      }
      else if (strcmp(argv[i],"-drop_triple") == 0)
      {
        add_criterion(new LAScriterionDropSpecificNumberOfReturns(3));
        *argv[i]='\0';
      }
      else if (strcmp(argv[i],"-drop_quadruple") == 0)
      {
        add_criterion(new LAScriterionDropSpecificNumberOfReturns(4));
        *argv[i]='\0';
      }
      else if (strcmp(argv[i],"-drop_quintuple") == 0)
      {
        add_criterion(new LAScriterionDropSpecificNumberOfReturns(5));
        *argv[i]='\0';
      }
      else if (strcmp(argv[i],"-drop_scan_direction") == 0)
      {
        add_criterion(new LAScriterionDropScanDirection(atoi(argv[i+1])));
        *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
      }
      else if (strncmp(argv[i],"-drop_intensity_above",15) == 0)
      {
        if (strcmp(argv[i],"-drop_intensity_above") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: max" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropIntensityAbove(atoi(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
        else if (strcmp(argv[i],"-drop_intensity_below") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: min" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropIntensityBelow(atoi(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
        else if (strcmp(argv[i],"-drop_intensity_between") == 0)
        {
          if ((i+2) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 2 arguments: min max" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropIntensityBetween(atoi(argv[i+1]), atoi(argv[i+2])));
          *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; i+=2;
        }
      }
      else if (strncmp(argv[i],"-drop_abs_scan_angle_",21) == 0)
      {
        if (strcmp(argv[i],"-drop_abs_scan_angle_above") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: max" << std::endl;
            return FALSE;
          }
          I32 angle = atoi(argv[i+1]);
          add_criterion(new LAScriterionKeepScanAngle(-angle, angle));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
        else if (strcmp(argv[i],"-drop_abs_scan_angle_below") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: min" << std::endl;
            return FALSE;
          }
          I32 angle = atoi(argv[i+1]);
          add_criterion(new LAScriterionDropScanAngleBetween(-angle+1, angle-1));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
      }
      else if (strncmp(argv[i],"-drop_scan_angle_",17) == 0)
      {
        if (strcmp(argv[i],"-drop_scan_angle_above") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: max" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropScanAngleAbove(atoi(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
        else if (strcmp(argv[i],"-drop_scan_angle_below") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: min" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropScanAngleBelow(atoi(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }    
        else if (strcmp(argv[i],"-drop_scan_angle_between") == 0)
        {
          if ((i+2) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 2 arguments: min max" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropScanAngleBetween(atoi(argv[i+1]), atoi(argv[i+2])));
          *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; i+=2;
        }
      }
      else if (strcmp(argv[i],"-drop_synthetic") == 0)
      {
        add_criterion(new LAScriterionDropSynthetic());
        *argv[i]='\0';
      }
      else if (strcmp(argv[i],"-drop_keypoint") == 0)
      {
        add_criterion(new LAScriterionDropKeypoint());
        *argv[i]='\0';
      }
      else if (strcmp(argv[i],"-drop_withheld") == 0)
      {
        add_criterion(new LAScriterionDropWithheld());
        *argv[i]='\0';
      }
      else if (strcmp(argv[i],"-drop_overlap") == 0)
      {
        add_criterion(new LAScriterionDropOverlap());
        *argv[i]='\0';
      }
      else if (strcmp(argv[i],"-drop_wavepacket") == 0)
      {
        if ((i+1) >= argc)
        {
          Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: index" << std::endl;
          return FALSE;
        }
        *argv[i]='\0';
        i+=1;
        add_criterion(new LAScriterionDropWavepacket(atoi(argv[i])));
        *argv[i]='\0';
      }
      else if (strncmp(argv[i],"-drop_user_data", 15) == 0)
      {
        if (strcmp(argv[i],"-drop_user_data") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs at least 1 argument: ID" << std::endl;
            return FALSE;
          }
          *argv[i]='\0';
          i+=1;
          do
          {
            add_criterion(new LAScriterionDropUserData(atoi(argv[i])));
            *argv[i]='\0';
            i+=1;
          } while ((i < argc) && ('0' <= *argv[i]) && (*argv[i] <= '9'));
          i-=1;
        }
        else if (strcmp(argv[i],"-drop_user_data_below") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: min_value" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropUserDataBelow(atoi(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
        else if (strcmp(argv[i],"-drop_user_data_above") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: max_value" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropUserDataAbove(atoi(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
        else if (strcmp(argv[i],"-drop_user_data_between") == 0)
        {
          if ((i+2) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 2 arguments: min_value max_value" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropUserDataBetween(atoi(argv[i+1]), atoi(argv[i+2])));
          *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; i+=2;
        }
      }
      else if (strncmp(argv[i],"-drop_point_source", 18) == 0)
      {
        if (strcmp(argv[i],"-drop_point_source") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs at least 1 argument: ID" << std::endl;
            return FALSE;
          }
          *argv[i]='\0';
          i+=1;
          do
          {
            add_criterion(new LAScriterionDropPointSource(atoi(argv[i])));
            *argv[i]='\0';
            i+=1;
          } while ((i < argc) && ('0' <= *argv[i]) && (*argv[i] <= '9'));
          i-=1;
        }
        else if (strcmp(argv[i],"-drop_point_source_below") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: min_ID" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropPointSourceBelow(atoi(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
        else if (strcmp(argv[i],"-drop_point_source_above") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: max_ID" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropPointSourceAbove(atoi(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
        else if (strcmp(argv[i],"-drop_point_source_between") == 0)
        {
          if ((i+2) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 2 arguments: min_ID max_ID" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropPointSourceBetween(atoi(argv[i+1]), atoi(argv[i+2])));
          *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; i+=2;
        }
      }
      else if (strncmp(argv[i],"-drop_gps", 9) == 0)
      {
        if (strcmp(argv[i],"-drop_gps_time_above") == 0 || strcmp(argv[i],"-drop_gpstime_above") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: max_gps_time" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropGpsTimeAbove(atof(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
        else if (strcmp(argv[i],"-drop_gps_time_below") == 0 || strcmp(argv[i],"-drop_gpstime_below") == 0)
        {
          if ((i+1) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: min_gps_time" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropGpsTimeBelow(atof(argv[i+1])));
          *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
        }
        else if (strcmp(argv[i],"-drop_gps_time_between") == 0 || strcmp(argv[i],"-drop_gpstime_between") == 0)
        {
          if ((i+2) >= argc)
          {
            Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 2 arguments: min max" << std::endl;
            return FALSE;
          }
          add_criterion(new LAScriterionDropGpsTimeBetween(atof(argv[i+1]), atof(argv[i+2])));
          *argv[i]='\0'; *argv[i+1]='\0'; *argv[i+2]='\0'; i+=2;
        }
      }
    }
    else if (strcmp(argv[i],"-first_only") == 0)
    {
      add_criterion(new LAScriterionKeepFirstReturn());
      *argv[i]='\0';
    }
    else if (strcmp(argv[i],"-last_only") == 0)
    {
      add_criterion(new LAScriterionKeepLastReturn());
      *argv[i]='\0';
    }
    else if (strncmp(argv[i],"-thin_", 6) == 0)
    {
      if (strcmp(argv[i],"-thin_with_grid") == 0)
      {
        if ((i+1) >= argc)
        {
          Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: grid_spacing" << std::endl;
          return FALSE;
        }
        add_criterion(new LAScriterionThinWithGrid((F32)atof(argv[i+1])));
        *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
      }
      else if (strcmp(argv[i],"-thin_with_time") == 0)
      {
        if ((i+1) >= argc)
        {
          Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs 1 argument: time_spacing" << std::endl;
          return FALSE;
        }
        add_criterion(new LAScriterionThinWithTime((F32)atof(argv[i+1])));
        *argv[i]='\0'; *argv[i+1]='\0'; i+=1;
      }
    }
    else if (strncmp(argv[i],"-filter_", 8) == 0)
    {
      if (strcmp(argv[i],"-filter_and") == 0)
      {
        if (num_criteria < 2)
        {
          Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs to be preceeded by at least two filters" << std::endl;
          return FALSE;
        }
        LAScriterion* filter_criterion = new LAScriterionAnd(criteria[num_criteria-2], criteria[num_criteria-1]);
        num_criteria--;
        criteria[num_criteria] = 0;
        num_criteria--;
        criteria[num_criteria] = 0;
        add_criterion(filter_criterion);
        *argv[i]='\0';
      }
      else if (strcmp(argv[i],"-filter_or") == 0)
      {
        if (num_criteria < 2)
        {
          Rcpp::Rcerr << "ERROR: '" << argv[i] << "' needs to be preceeded by at least two filters" << std::endl;
          return FALSE;
        }
        LAScriterion* filter_criterion = new LAScriterionOr(criteria[num_criteria-2], criteria[num_criteria-1]);
        num_criteria--;
        criteria[num_criteria] = 0;
        num_criteria--;
        criteria[num_criteria] = 0;
        add_criterion(filter_criterion);
        *argv[i]='\0';
      }
    }
  }

  if (drop_return_mask)
  {
    if (keep_return_mask)
    {
      Rcpp::Rcerr << "ERROR: cannot use '-drop_return' and '-keep_return' simultaneously" << std::endl;
      return FALSE;
    }
    else
    {
      keep_return_mask = 255 & ~drop_return_mask;
    }
  }
  if (keep_return_mask)
  {
    add_criterion(new LAScriterionKeepReturns(keep_return_mask));
  }

  if (keep_classification_mask)
  {
    if (drop_classification_mask)
    {
      Rcpp::Rcerr << "ERROR: cannot use '-drop_class' and '-keep_class' simultaneously" << std::endl;
      return FALSE;
    }
    else
    {
      drop_classification_mask = ~keep_classification_mask;
    }
  }
  if (drop_classification_mask)
  {
    add_criterion(new LAScriterionDropClassifications(drop_classification_mask));
  }

  if (keep_extended_classification_mask[0] || keep_extended_classification_mask[1] || keep_extended_classification_mask[2] || keep_extended_classification_mask[3] || keep_extended_classification_mask[4] || keep_extended_classification_mask[5] || keep_extended_classification_mask[6] || keep_extended_classification_mask[7])
  {
    if (drop_extended_classification_mask[0] || drop_extended_classification_mask[1] || drop_extended_classification_mask[2] || drop_extended_classification_mask[3] || drop_extended_classification_mask[4] || drop_extended_classification_mask[5] || drop_extended_classification_mask[6] || drop_extended_classification_mask[7])
    {
      Rcpp::Rcerr << "ERROR: cannot use '-drop_extended_class' and '-keep_extended_class' simultaneously" << std::endl;
      return FALSE;
    }
    else
    {
      drop_extended_classification_mask[0] = ~keep_extended_classification_mask[0];
      drop_extended_classification_mask[1] = ~keep_extended_classification_mask[1];
      drop_extended_classification_mask[2] = ~keep_extended_classification_mask[2];
      drop_extended_classification_mask[3] = ~keep_extended_classification_mask[3];
      drop_extended_classification_mask[4] = ~keep_extended_classification_mask[4];
      drop_extended_classification_mask[5] = ~keep_extended_classification_mask[5];
      drop_extended_classification_mask[6] = ~keep_extended_classification_mask[6];
      drop_extended_classification_mask[7] = ~keep_extended_classification_mask[7];
    }
  }
  if (drop_extended_classification_mask[0] || drop_extended_classification_mask[1] || drop_extended_classification_mask[2] || drop_extended_classification_mask[3] || drop_extended_classification_mask[4] || drop_extended_classification_mask[5] || drop_extended_classification_mask[6] || drop_extended_classification_mask[7])
  {
    add_criterion(new LAScriterionDropExtendedClassifications(drop_extended_classification_mask));
  }

  return TRUE;
}

BOOL LASfilter::parse(CHAR* string)
{
  int p = 0;
  int argc = 1;
  char* argv[64];
  int len = strlen(string);

  while (p < len)
  {
    while ((p < len) && (string[p] == ' ')) p++;
    if (p < len)
    {
      argv[argc] = string + p;
      argc++;
      while ((p < len) && (string[p] != ' ')) p++;
      string[p] = '\0';
      p++;
    }
  }

  return parse(argc, argv);
}

I32 LASfilter::unparse(CHAR* string) const
{
  U32 i;
  I32 n = 0;
  for (i = 0; i < num_criteria; i++)
  {
    n += criteria[i]->get_command(&string[n]);
  }
  return n;
}

void LASfilter::addClipCircle(F64 x, F64 y, F64 radius)
{
  add_criterion(new LAScriterionKeepCircle(x, y, radius));
}

void LASfilter::addClipBox(F64 min_x, F64 min_y, F64 min_z, F64 max_x, F64 max_y, F64 max_z)
{
  add_criterion(new LAScriterionKeepxyz(min_x, min_y, min_z, max_x, max_y, max_z));
}

void LASfilter::addKeepScanDirectionChange()
{
  add_criterion(new LAScriterionKeepScanDirectionChange());
}

BOOL LASfilter::filter(const LASpoint* point)
{
  U32 i;

  for (i = 0; i < num_criteria; i++)
  {
    if (criteria[i]->filter(point))
    {
      counters[i]++;
      return TRUE; // point was filtered
    }
  }
  return FALSE; // point survived
}

void LASfilter::reset()
{
  U32 i;
  for (i = 0; i < num_criteria; i++)
  {
    criteria[i]->reset();
  }
}

LASfilter::LASfilter()
{
  alloc_criteria = 0;
  num_criteria = 0;
  criteria = 0;
  counters = 0;
}

LASfilter::~LASfilter()
{
  if (criteria) clean();
}

void LASfilter::add_criterion(LAScriterion* filter_criterion)
{
  if (num_criteria == alloc_criteria)
  {
    U32 i;
    alloc_criteria += 16;
    LAScriterion** temp_criteria = new LAScriterion*[alloc_criteria];
    int* temp_counters = new int[alloc_criteria];
    if (criteria)
    {
      for (i = 0; i < num_criteria; i++)
      {
        temp_criteria[i] = criteria[i];
        temp_counters[i] = counters[i];
      }
      delete [] criteria;
      delete [] counters;
    }
    criteria = temp_criteria;
    counters = temp_counters;
  }
  criteria[num_criteria] = filter_criterion;
  counters[num_criteria] = 0;
  num_criteria++;
}
