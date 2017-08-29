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
#include <string>                             // for string, operator==
#include <utility>                            // for pair
#include <vector>                             // for vector

#ifdef HAVE_PUGIXML
#include <pugixml.hpp> // for xml_document, xml_pars...
using pugi::xml_node;
using pugi::xml_document;
using pugi::xml_parse_result;
#endif

using std::string;

namespace rawspeed {

#ifdef HAVE_PUGIXML
CameraMetaData::CameraMetaData(const char *docname) {
  xml_document doc;

#if defined(__unix__) || defined(__APPLE__)
  xml_parse_result result = doc.load_file(docname);
#else
  xml_parse_result result = doc.load_file(pugi::as_wide(docname).c_str());
#endif

  if (!result) {
    ThrowCME(
        "XML Document could not be parsed successfully. Error was: %s in %s",
        result.description(), doc.child("node").attribute("attr").value());
  }

  for (xml_node camera : doc.child("Cameras").children("Camera")) {
    const auto* cam = addCamera(std::make_unique<Camera>(camera));

    if (cam == nullptr)
      continue;

    // Create cameras for aliases.
    for (uint32 i = 0; i < cam->aliases.size(); i++) {
      addCamera(std::make_unique<Camera>(cam, i));
    }
  }
}
#endif

static inline CameraId getId(const string& make, const string& model,
                             const string& mode) {
  CameraId id;
  id.make = trimSpaces(make);
  id.model = trimSpaces(model);
  id.mode = trimSpaces(mode);

  return id;
}

const Camera* CameraMetaData::getCamera(const string& make, const string& model,
                                        const string& mode) const {
  auto camera = cameras.find(getId(make, model, mode));
  return camera == cameras.end() ? nullptr : camera->second.get();
}

const Camera* CameraMetaData::getCamera(const string& make,
                                        const string& model) const {
  auto id = getId(make, model, "");

  auto iter = find_if(cameras.cbegin(), cameras.cend(),
                      [&id](decltype(*cameras.cbegin())& i) -> bool {
                        const auto& cid = i.first;
                        return tie(id.make, id.model) ==
                               tie(cid.make, cid.model);
                      });

  if (iter == cameras.end())
    return nullptr;

  return iter->second.get();
}

bool CameraMetaData::hasCamera(const string& make, const string& model,
                               const string& mode) const {
  return getCamera(make, model, mode);
}

const Camera* __attribute__((pure))
CameraMetaData::getChdkCamera(uint32 filesize) const {
  auto camera = chdkCameras.find(filesize);
  return camera == chdkCameras.end() ? nullptr : camera->second;
}

bool __attribute__((pure))
CameraMetaData::hasChdkCamera(uint32 filesize) const {
  return chdkCameras.end() != chdkCameras.find(filesize);
}

const Camera* CameraMetaData::addCamera(std::unique_ptr<Camera> cam) {
  auto id = getId(cam->make, cam->model, cam->mode);
  if (cameras.end() != cameras.find(id)) {
    writeLog(
        DEBUG_PRIO_WARNING,
        "CameraMetaData: Duplicate entry found for camera: %s %s, Skipping!",
        cam->make.c_str(), cam->model.c_str());
    return nullptr;
  }
  cameras[id] = std::move(cam);

  if (string::npos != cameras[id]->mode.find("chdk")) {
    auto filesize_hint = cameras[id]->hints.get("filesize", string());
    if (filesize_hint.empty()) {
      writeLog(DEBUG_PRIO_WARNING,
               "CameraMetaData: CHDK camera: %s %s, no \"filesize\" hint set!",
               cameras[id]->make.c_str(), cameras[id]->model.c_str());
    } else {
      chdkCameras[stoi(filesize_hint)] = cameras[id].get();
      // writeLog(DEBUG_PRIO_WARNING, "CHDK camera: %s %s size:%u",
      // cameras[id]->make.c_str(), cameras[id]->model.c_str(), size);
    }
  }
  return cameras[id].get();
}

void CameraMetaData::disableMake(const string &make) {
  for (const auto& cam : cameras) {
    if (cam.second->make == make)
      cam.second->supported = false;
  }
}

void CameraMetaData::disableCamera(const string &make, const string &model) {
  for (const auto& cam : cameras) {
    if (cam.second->make == make && cam.second->model == model)
      cam.second->supported = false;
  }
}

} // namespace rawspeed
