#include "StdAfx.h"
#include "ThumbnailGenerator.h"

ThumbnailGenerator::ThumbnailGenerator(void) : thumbnail(0)
{
}

ThumbnailGenerator::~ThumbnailGenerator(void)
{
  if (thumbnail)
    delete thumbnail;
  thumbnail = NULL;
}

RgbImage* ThumbnailGenerator::getPreview() {
  if (!thumbnail)
    throw ThumbnailGeneratorException("No thumbnail loaded.");
  return thumbnail;
}
