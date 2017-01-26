/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/

#include "metadata/CameraMetaData.h"
#include "common/Common.h"                    // for uint32, TrimSpaces
#include "metadata/Camera.h"                  // for Camera
#include "metadata/CameraMetadataException.h" // for ThrowCME
#include <map>                                // for map, _Rb_tree_iterator
#include <pugixml.hpp>                        // for xml_document, xml_pars...
#include <string>                             // for string, allocator, bas...
#include <utility>                            // for move, pair
#include <vector>                             // for vector

using namespace std;

namespace RawSpeed {

using namespace pugi;

CameraMetaData::CameraMetaData() = default;

CameraMetaData::CameraMetaData(const char *docname) {
  xml_document doc;
  xml_parse_result result = doc.load_file(docname);

  if (!result) {
    ThrowCME("CameraMetaData: XML Document could not be parsed successfully. Error was: %s in %s",
      result.description(), doc.child("node").attribute("attr").value());
  }

  for (xml_node camera : doc.child("Cameras").children("Camera")) {
    auto *cam = new Camera(camera);

    if (!addCamera(cam))
      continue;

    // Create cameras for aliases.
    for (uint32 i = 0; i < cam->aliases.size(); i++) {
      addCamera(new Camera(cam, i));
    }
  }
}

CameraMetaData::~CameraMetaData() {
  auto i = cameras.begin();
  for (; i != cameras.end(); ++i) {
    delete((*i).second);
  }
}

static inline string getId(string make, string model, string mode)
{
  TrimSpaces(make);
  TrimSpaces(model);
  TrimSpaces(mode);

  return make + model + mode;
}

Camera* CameraMetaData::getCamera(string make, string model, string mode) {
  auto camera =
      cameras.find(getId(std::move(make), std::move(model), std::move(mode)));
  return camera == cameras.end() ? nullptr : camera->second;
}

Camera* CameraMetaData::getCamera(string make, string model) {
  string id = getId(std::move(make), std::move(model), "");

  // do a prefix match, i.e. the make and model match, but not mode.
  auto iter = cameras.lower_bound(id);

  if (iter == cameras.find(id))
    return nullptr;

  return iter->second;
}

bool CameraMetaData::hasCamera(string make, string model, string mode) {
  return getCamera(std::move(make), std::move(model), std::move(mode));
}

Camera* CameraMetaData::getChdkCamera(uint32 filesize) {
  auto camera = chdkCameras.find(filesize);
  return camera == chdkCameras.end() ? nullptr : camera->second;
}

bool CameraMetaData::hasChdkCamera(uint32 filesize) {
  return chdkCameras.end() != chdkCameras.find(filesize);
}

bool CameraMetaData::addCamera( Camera* cam )
{
  string id = getId(cam->make, cam->model, cam->mode);
  if (cameras.end() != cameras.find(id)) {
    writeLog(DEBUG_PRIO_WARNING, "CameraMetaData: Duplicate entry found for camera: %s %s, Skipping!\n", cam->make.c_str(), cam->model.c_str());
    delete cam;
    return false;
  }
  cameras[id] = cam;

  if (string::npos != cam->mode.find("chdk")) {
    auto filesize_hint = cam->hints.find("filesize");
    if (filesize_hint == cam->hints.end() || filesize_hint->second.empty()) {
      writeLog(DEBUG_PRIO_WARNING, "CameraMetaData: CHDK camera: %s %s, no \"filesize\" hint set!\n", cam->make.c_str(), cam->model.c_str());
    } else {
      chdkCameras[stoi(filesize_hint->second)] = cam;
      // writeLog(DEBUG_PRIO_WARNING, "CHDK camera: %s %s size:%u\n", cam->make.c_str(), cam->model.c_str(), size);
    }
  }
  return true;
}

void CameraMetaData::disableMake(const string &make) {
  for (auto cam : cameras) {
    if (cam.second->make == make)
      cam.second->supported = false;
  }
}

void CameraMetaData::disableCamera(const string &make, const string &model) {
  for (auto cam : cameras) {
    if (cam.second->make == make && cam.second->model == model)
      cam.second->supported = false;
  }
}

} // namespace RawSpeed
