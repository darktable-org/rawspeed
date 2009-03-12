#pragma once
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/xmlschemas.h>
#include "Camera.h"

class CameraMetaData
{
public:
  CameraMetaData(char *docname);
  virtual ~CameraMetaData(void);
  xmlDocPtr doc;
  xmlParserCtxtPtr ctxt; /* the parser context */
  map<string,Camera*> cameras;
  void dumpXML();
  Camera* getCamera(string make, string model);
protected:
  void dumpCameraXML(Camera* cam);
};
