#pragma once
#include "ColorFilterArray.h"
#include <libxml/parser.h>
#include "BlackArea.h"
#include "CameraMetadataException.h"

class Camera
{
public:
  Camera(xmlDocPtr doc, xmlNodePtr cur);
  void parseCameraChild(xmlDocPtr doc, xmlNodePtr cur);
  virtual ~Camera(void);
  string make;
  string model;
  string mode;
  ColorFilterArray cfa;
  guint black;
  guint white;
  gboolean supported;
  iPoint2D cropSize;
  iPoint2D cropPos;
  vector<BlackArea> blackAreas;
  void parseCFA( xmlDocPtr doc, xmlNodePtr cur );
  void parseBlackAreas( xmlDocPtr doc, xmlNodePtr cur );

private:
  int StringToInt(const xmlChar *in, const xmlChar *tag, const char* attribute);
  int getAttributeAsInt( xmlNodePtr cur , const xmlChar *tag, const char* attribute);
};
