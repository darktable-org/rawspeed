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
#include "common/Common.h"                    // for uint32, trimSpaces
#include "metadata/Camera.h"                  // for Camera
#include "metadata/CameraMetadataException.h" // for ThrowCME
#include <algorithm>                          // for find_if
#include <map>                                // for _Rb_tree_iterator, map
#include <pugixml.hpp>                        // for xml_document, xml_pars...
#include <string>                             // for string, operator==
#include <utility>                            // for pair
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

static inline CameraId getId(const string& make, const string& model,
                             const string& mode) {
  CameraId id;
  id.make = trimSpaces(make);
  id.model = trimSpaces(model);
  id.mode = trimSpaces(mode);

  return id;
}

Camera* CameraMetaData::getCamera(const string& make, const string& model,
                                  const string& mode) const {
  auto camera = cameras.find(getId(make, model, mode));
  return camera == cameras.end() ? nullptr : camera->second;
}

Camera* CameraMetaData::getCamera(const string& make,
                                  const string& model) const {
  auto id = getId(make, model, "");

  auto iter = find_if(cameras.begin(), cameras.end(),
                      [id](decltype(*cameras.begin())& i) -> bool {
                        const auto cid = i.first;
                        return tie(id.make, id.model) ==
                               tie(cid.make, cid.model);
                      });

  if (iter == cameras.end())
    return nullptr;

  return iter->second;
}

bool CameraMetaData::hasCamera(const string& make, const string& model,
                               const string& mode) const {
  return getCamera(make, model, mode);
}

Camera* CameraMetaData::getChdkCamera(uint32 filesize) const {
  auto camera = chdkCameras.find(filesize);
  return camera == chdkCameras.end() ? nullptr : camera->second;
}

bool CameraMetaData::hasChdkCamera(uint32 filesize) const {
  return chdkCameras.end() != chdkCameras.find(filesize);
}

bool CameraMetaData::addCamera( Camera* cam )
{
  auto id = getId(cam->make, cam->model, cam->mode);
  if (cameras.end() != cameras.find(id)) {
    writeLog(DEBUG_PRIO_WARNING, "CameraMetaData: Duplicate entry found for camera: %s %s, Skipping!\n", cam->make.c_str(), cam->model.c_str());
    delete cam;
    return false;
  }
  cameras[id] = cam;

  if (string::npos != cam->mode.find("chdk")) {
    auto filesize_hint = cam->hints.get("filesize", string());
    if (filesize_hint.empty()) {
      writeLog(DEBUG_PRIO_WARNING, "CameraMetaData: CHDK camera: %s %s, no \"filesize\" hint set!\n", cam->make.c_str(), cam->model.c_str());
    } else {
      chdkCameras[stoi(filesize_hint)] = cam;
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
