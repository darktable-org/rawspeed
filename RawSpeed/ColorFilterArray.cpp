#include "StdAfx.h"
#include "ColorFilterArray.h"
//  Cols-----\
//  Rows-----/
//  ||
//  \/

ColorFilterArray::ColorFilterArray() : size(0,0), mCFAColors(0)
{
}

ColorFilterArray::ColorFilterArray(iPoint2D _size) : size(_size), mCFAColors(new CFAColor[_size.Area()])
{
}

ColorFilterArray::ColorFilterArray(const ColorFilterArray& obj) : size(obj.size), mCFAColors(new CFAColor[obj.size.Area()]) {
}


ColorFilterArray& ColorFilterArray::operator= (const ColorFilterArray& f) {
  if (this == &f) return *this;   // Gracefully handle self assignment
  if (mCFAColors)
    delete[] mCFAColors;
  size = f.size;

  mCFAColors = new CFAColor[f.size.Area()];
  memcpy(mCFAColors,f.mCFAColors, f.size.Area());

  return *this;
}

ColorFilterArray::~ColorFilterArray(void)
{
  if (mCFAColors)
    delete[] mCFAColors;
  mCFAColors = 0;
}

CFAColor ColorFilterArray::getColorAt(const iPoint2D &pos) {
  if (!size.Area())
    ThrowRDE("CFA getColorAt: No size set of CFA array.");
  if (pos.isInside(size))
    ThrowRDE("CFA getColorAt: Point outside size if ColorFilter Array.");
  return mCFAColors[pos.x + pos.y * size.x];
}

void ColorFilterArray::setColorAt(const iPoint2D &pos, CFAColor c){
  if (!size.Area())
    ThrowRDE("CFA setColorAt: No size set of CFA array.");
  if (pos.isInside(size))
    ThrowRDE("CFA setColorAt: Point outside size if ColorFilter Array.");
  if (c < CFA_COLOR_MIN || c > CFA_COLOR_MAX)
    ThrowRDE("CFA setColorAt: Invalid color specified.");    

  mCFAColors[pos.x + pos.y * size.x] = c;
}

int ColorFilterArray::getColorNum(){
  return (int)getUniqueColors().size();
}

vector<CFAColor> ColorFilterArray::getUniqueColors() {
  if (!size.Area())
    ThrowRDE("CFA getUniqueColors: No size set of CFA array.");

  vector<CFAColor> c;

  for (int y = 0; y < size.y; y++) {
    for (int x = 0; x < size.x; x++) {
      CFAColor color = getColorAt(iPoint2D(x,y));
      bool found = false;
      for (unsigned int i = 0; i<c.size(); i++) {
        if (c[i] == color)         
          found = true;
      }
      if (!found)
        c.push_back(color);
    }
  }
  return c;
}

int ColorFilterArray::getColorNumInRow(int n, CFAColor c) {
  if (!size.Area())
    ThrowRDE("CFA getColorNumInRow: No size set of CFA array.");
  if (n >= size.y)
    ThrowRDE("CFA getColorNumInRow: Row size outside CFA array.");
  int num = 0;
  for (int x = 0; x < size.x; x++) {
    CFAColor color = getColorAt(iPoint2D(x,n));
    if (color == c)
      num ++;
  }
  return num;
}

int ColorFilterArray::getColorNumInCol(int n, CFAColor c) {
  if (!size.Area())
    ThrowRDE("CFA getColorNumInCol: No size set of CFA array.");
  if (n >= size.x)
    ThrowRDE("CFA getColorNumInCol: Col size outside CFA array.");
  int num = 0;
  for (int y = 0; y < size.y; y++) {
    CFAColor color = getColorAt(iPoint2D(n,y));
    if (color == c)
      num ++;
  }
  return num;
}

int ColorFilterArray::getWidthMultiplier(CFAColor c) { // This will return the maximum number of this color in one row.
  int maxn = 0;
  for (int y = 0; y < size.y; y++) {
    maxn = max(maxn, getColorNumInRow(y,c));
  }
  return maxn;
}

int ColorFilterArray::getHeightMultiplier(CFAColor c) { // This will get the accumulated number of rows where the color is present.
  int accn = 0;
  for (int y = 0; y < size.y; y++) {
    accn += getColorNumInRow(y,c) ? 1 : 0;
  }
  return accn;
}
