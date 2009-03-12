#pragma once
/* 
RawSpeed - RAW file decoder.

Copyright (C) 2009 Klaus Post

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA

http://www.klauspost.com
*/

#include "RawImage.h"

/****
* Conventions:
*
* The dimensions of an image is determined by the largest plane.
* Each plane size is defined as the largest plane divided by a power of two.
* Each plane can only hold one component.
* Each planar image can only hold up to four planes.
****/
class PlanarPlane {
public:
  PlanarPlane() : data(0), subfactor(0) {};
  guchar* data;
  guint subfactor;  
};

class PlanarRawImage :
  public RawImage
{
public:
  PlanarRawImage(void);
  virtual ~PlanarRawImage(void);
  PlanarPlane p[4];
};
