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
#include "common/Common.h"                    // for trimSpaces, writeLog
#include "metadata/Camera.h"                  // for Camera, Camera::Suppor...
#include "metadata/CameraMetadataException.h" // for ThrowException, ThrowCME
#include <algorithm>                          // for find_if
#include <map>                                // for map, map<>::const_iter...
#include <string>                             // for string, operator==
#include <utility>                            // for pair, move
#include <vector>                             // for vector

#ifdef HAVE_PUGIXML
#include <pugixml.hpp> // for xml_document, xml_pars...

using pugi::xml_node;
using pugi::xml_document;
using pugi::xml_parse_result;
#endif

namespace rawspeed {

#ifdef HAVE_PUGIXML
CameraMetaData::CameraMetaData(const char *docname) {
  xml_document doc;

  if (xml_parse_result result =
#if defined(__unix__) || defined(__APPLE__)
          doc.load_file(docname)
#else
          doc.load_file(pugi::as_wide(docname).c_str())
#endif
          ;
      !result) {
    ThrowCME("XML Document \"%s\" could not be parsed successfully. Error was: "
             "%s in %s",
             docname, result.description(),
             doc.child("node").attribute("attr").value());
  }

  for (xml_node camera : doc.child("Cameras").children("Camera")) {
    const auto* cam = addCamera(std::make_unique<Camera>(camera));

    if (cam == nullptr)
      continue;

    // Create cameras for aliases.
    for (auto i = 0UL; i < cam->aliases.size(); i++) {
      addCamera(std::make_unique<Camera>(cam, i));
    }
  }
}
#endif

static inline CameraId getId(const std::string& make, const std::string& model,
                             const std::string& mode) {
  CameraId id;
  id.make = trimSpaces(make);
  id.model = trimSpaces(model);
  id.mode = trimSpaces(mode);

  return id;
}

const Camera* CameraMetaData::getCamera(const std::string& make,
                                        const std::string& model,
                                        const std::string& mode) const {
  auto camera = cameras.find(getId(make, model, mode));
  return camera == cameras.end() ? nullptr : camera->second.get();
}

const Camera* CameraMetaData::getCamera(const std::string& make,
                                        const std::string& model) const {
  auto id = getId(make, model, "");

  auto iter = find_if(cameras.cbegin(), cameras.cend(),
                      [&id](decltype(*cameras.cbegin())& i) {
                        const auto& cid = i.first;
                        return tie(id.make, id.model) ==
                               tie(cid.make, cid.model);
                      });

  if (iter == cameras.end())
    return nullptr;

  return iter->second.get();
}

bool CameraMetaData::hasCamera(const std::string& make,
                               const std::string& model,
                               const std::string& mode) const {
  return getCamera(make, model, mode);
}

const Camera* __attribute__((pure))
CameraMetaData::getChdkCamera(uint32_t filesize) const {
  auto camera = chdkCameras.find(filesize);
  return camera == chdkCameras.end() ? nullptr : camera->second;
}

bool __attribute__((pure))
CameraMetaData::hasChdkCamera(uint32_t filesize) const {
  return chdkCameras.end() != chdkCameras.find(filesize);
}

const Camera* CameraMetaData::addCamera(std::unique_ptr<Camera> cam) {
  auto id = getId(cam->make, cam->model, cam->mode);
  if (cameras.end() != cameras.find(id)) {
    writeLog(
        DEBUG_PRIO::WARNING,
        "CameraMetaData: Duplicate entry found for camera: %s %s, Skipping!",
        cam->make.c_str(), cam->model.c_str());
    return nullptr;
  }
  cameras[id] = std::move(cam);

  if (std::string::npos != cameras[id]->mode.find("chdk")) {
    auto filesize_hint = cameras[id]->hints.get("filesize", std::string());
    if (filesize_hint.empty()) {
      writeLog(DEBUG_PRIO::WARNING,
               "CameraMetaData: CHDK camera: %s %s, no \"filesize\" hint set!",
               cameras[id]->make.c_str(), cameras[id]->model.c_str());
    } else {
      chdkCameras[stoi(filesize_hint)] = cameras[id].get();
      // writeLog(DEBUG_PRIO::WARNING, "CHDK camera: %s %s size:%u",
      // cameras[id]->make.c_str(), cameras[id]->model.c_str(), size);
    }
  }
  return cameras[id].get();
}

void CameraMetaData::disableMake(std::string_view make) const {
  for (const auto& [id, cam] : cameras) {
    if (cam->make == make)
      cam->supportStatus = Camera::SupportStatus::Unsupported;
  }
}

void CameraMetaData::disableCamera(std::string_view make,
                                   std::string_view model) const {
  for (const auto& [id, cam] : cameras) {
    if (cam->make == make && cam->model == model)
      cam->supportStatus = Camera::SupportStatus::Unsupported;
  }
}

} // namespace rawspeed
