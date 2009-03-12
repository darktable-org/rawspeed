#pragma once

class BlackArea
{
public:
  BlackArea(int offset, int size, gboolean isVertical);
  virtual ~BlackArea(void);
  gboolean isVertical;  // Otherwise horizontal
  guint size;   // Size in bayer pixels.
  guint offset; // Offset in bayer pixels.
};
