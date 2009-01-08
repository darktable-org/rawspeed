#pragma once
#include "ColorFilterArray.h"

/********************************************************************************
 * The images are stored as planar data. 
 * That makes the following layout for this example CFA:
 *
 * [R1][G1] [R2][G2] [R3][G3] [R4][G4]
 * [G5][B1] [G6][B2] [G7][B3] [G8][G4]
 *
 * To this planar:
 *
 * Red: [R1] [R2] [R3] [R4] Green: [G1] [G2] [G3] [G4]  Blue: [B1] [B2] [B3] [B4]
 *      (next grid)                [G5] [G6] [G7] [G8]        (next grid)
 *
 * If the same color is present several times within one grid-array, they will be placed after eachother.
 *
 ********************************************************************************/

class RawImagePlaneWriter;

class RawImagePlane
{
public:
  RawImagePlane(void);
  RawImagePlane::RawImagePlane(iPoint2D dimension, CFAColor _color);
  virtual ~RawImagePlane(void);
  virtual void allocateScan();
  unsigned short* mScan;
  iPoint2D dim;
  int pitch;    // Pitch in bytes
  CFAColor color;   
  int bpp;      // Bytes per Pixel
  virtual RawImagePlaneWriter* getWriter();
protected:
  RawImagePlaneWriter* writer;
};

class RawImagePlaneWriter {
  public:
    RawImagePlaneWriter(RawImagePlane* p) : _p(p), line(0), ptr(p->mScan) {}
    unsigned short* ptr;
    void nextLine() { ptr = &_p->mScan[(++line*_p->pitch)/2]; _ASSERTE(line<=_p->dim.y);}
    void reset() { ptr = &_p->mScan[0]; line = 0;}
private:
    RawImagePlane* _p;
    int line;
};

