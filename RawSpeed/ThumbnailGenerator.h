#pragma once
#include "RGBImage.h"

class ThumbnailGenerator
{
public:
  typedef enum PreviewType {
    PT_smallest,      // Smallest (by pixel count)
    PT_largest,       // Largest (by pixel count)
    PT_first,         // First valid.
    PT_last,          // Last valid.
    PT_fewest_bytes,  // Smallest (by pixel compressed byte count)
    PT_most_bytes     // Largest (by pixel compressed byte count)
  };

  ThumbnailGenerator(void);
  virtual ~ThumbnailGenerator(void);
  virtual RgbImage* readPreview(PreviewType preferedPreview) = 0;
  virtual RgbImage* getPreview();

protected:
    RgbImage* thumbnail;
};

class ThumbnailGeneratorException : public std::runtime_error
{
public:
  ThumbnailGeneratorException(const string _msg) : runtime_error(_msg) {
    _RPT1(0, "Thumbnail Exception: %s\n", _msg.c_str());
  }
};
