#include "StdAfx.h"
#include "ColorFilterArray.h"

ColorFilterArray::ColorFilterArray(void) {
  setCFA(CFA_UNKNOWN, CFA_UNKNOWN, CFA_UNKNOWN, CFA_UNKNOWN);
}

ColorFilterArray::ColorFilterArray( CFAColor up_left, CFAColor up_right, CFAColor down_left, CFAColor down_right )
{
  cfa[0] = up_left;
  cfa[1] = up_right;
  cfa[2] = down_left;
  cfa[3] = down_right;
}

ColorFilterArray::~ColorFilterArray(void)
{
}

void ColorFilterArray::setCFA( CFAColor up_left, CFAColor up_right, CFAColor down_left, CFAColor down_right )
{
  cfa[0] = up_left;
  cfa[1] = up_right;
  cfa[2] = down_left;
  cfa[3] = down_right;
}

void ColorFilterArray::setCFA( guchar dcrawCode )
{
  cfa[0] = (CFAColor)(dcrawCode&0x3);
  cfa[1] = (CFAColor)((dcrawCode>>2)&0x3);
  cfa[2] = (CFAColor)((dcrawCode>>4)&0x3);
  cfa[3] = (CFAColor)((dcrawCode>>6)&0x3);
}

guint ColorFilterArray::getDcrawFilter() {
  if (cfa[0]>3 || cfa[1]>3 || cfa[2]>3 || cfa[3]>3)
    ThrowRDE("getDcrawFilter: Invalid colors defined.");
  guint v =  cfa[0] | cfa[1]<<2 | cfa[2]<<4 | cfa[3]<<6;
  return v | (v<<8) | (v<<16) | (v<<24);
}

std::string ColorFilterArray::asString()
{
  string s("Upper left:");
  s += colorToString(cfa[0]); 
  s.append(" * Upper right:");
  s += colorToString(cfa[1]);
  s +=("\nLower left:");
  s += colorToString(cfa[2]); 
  s.append(" * Lower right:");
  s += colorToString(cfa[3]);
  s.append("\n");

  s += string("CFA_")+colorToString(cfa[0])+string(", CFA_")+colorToString(cfa[1]);
  s += string(", CFA_")+colorToString(cfa[2])+string(", CFA_")+colorToString(cfa[3])+string("\n");
  return s;
}

std::string ColorFilterArray::colorToString( CFAColor c ) {
  switch (c) {
  case CFA_RED:
    return string("RED");
  case CFA_GREEN:
    return string("GREEN");
  case CFA_BLUE:
    return string("BLUE");
  case CFA_GREEN2:
    return string("GREEN2");
  case CFA_CYAN:
    return string("CYAN");
  case CFA_MAGENTA:
    return string("MAGENTA");
  case CFA_YELLOW:
    return string("YELLOW");
  case CFA_WHITE:
    return string("WHITE");
  default:
    return string("UNKNOWN");
  }
}