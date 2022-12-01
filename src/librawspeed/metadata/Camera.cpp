/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2017 Axel Waggershauser

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

#include "metadata/Camera.h"
#include "adt/Point.h"                        // for iPoint2D
#include "common/Common.h"                    // for splitString
#include "metadata/CameraMetadataException.h" // for ThrowException, ThrowCME
#include <algorithm>                          // for max, fill, transform
#include <cctype>                             // for tolower
#include <cstdio>                             // for size_t
#include <map>                                // for map
#include <stdexcept>                          // for out_of_range
#include <string>                             // for string, operator==
#include <string_view>                        // for operator==, basic_stri...
#include <vector>                             // for vector

#ifdef HAVE_PUGIXML
#include <pugixml.hpp> // for xml_node, xml_attribute

using pugi::xml_node;
#endif

using std::vector;

using std::map;

namespace rawspeed {

#ifdef HAVE_PUGIXML
Camera::Camera(const pugi::xml_node& camera) : cfa(iPoint2D(0, 0)) {
  make = canonical_make = camera.attribute("make").as_string();
  if (make.empty())
    ThrowCME(R"("make" attribute not found.)");

  model = canonical_model = canonical_alias = camera.attribute("model").as_string();
  // chdk cameras seem to have an empty model?
  if (!camera.attribute("model")) // (model.empty())
    ThrowCME(R"("model" attribute not found.)");

  canonical_id = make + " " + model;

  supportStatus = [&camera]() {
    const std::string_view v = camera.attribute("supported").as_string("yes");
    if (v == "yes")
      return Camera::SupportStatus::Supported;
    if (v == "no")
      return Camera::SupportStatus::Unsupported;
    if (v == "no-samples")
      return Camera::SupportStatus::NoSamples;
    ThrowCME("Attribute 'supported' has unknown value.");
  }();
  mode = camera.attribute("mode").as_string("");
  decoderVersion = camera.attribute("decoder_version").as_int(0);

  for (xml_node c : camera.children()) {
    parseCameraChild(c);
  }
}
#endif

Camera::Camera(const Camera* camera, uint32_t alias_num) : cfa(iPoint2D(0, 0)) {
  if (alias_num >= camera->aliases.size())
    ThrowCME("Internal error, alias number out of range specified.");

  *this = *camera;
  model = camera->aliases[alias_num];
  canonical_alias = camera->canonical_aliases[alias_num];
  aliases.clear();
  canonical_aliases.clear();
}

#ifdef HAVE_PUGIXML
static std::string name(const xml_node& a) { return a.name(); }
#endif

const map<char, CFAColor> Camera::char2enum = {
    {'g', CFAColor::GREEN},  {'r', CFAColor::RED},
    {'b', CFAColor::BLUE},   {'f', CFAColor::FUJI_GREEN},
    {'c', CFAColor::CYAN},   {'m', CFAColor::MAGENTA},
    {'y', CFAColor::YELLOW},
};

const map<std::string, CFAColor, std::less<>> Camera::str2enum = {
    {"GREEN", CFAColor::GREEN},   {"RED", CFAColor::RED},
    {"BLUE", CFAColor::BLUE},     {"FUJI_GREEN", CFAColor::FUJI_GREEN},
    {"CYAN", CFAColor::CYAN},     {"MAGENTA", CFAColor::MAGENTA},
    {"YELLOW", CFAColor::YELLOW},
};

#ifdef HAVE_PUGIXML
void Camera::parseCFA(const xml_node &cur) {
  if (name(cur) != "CFA" && name(cur) != "CFA2")
    ThrowCME("Not an CFA/CFA2 node!");

  cfa.setSize(iPoint2D(cur.attribute("width").as_int(0),
                       cur.attribute("height").as_int(0)));
  for (xml_node c : cur.children()) {
      if (name(c) == "ColorRow") {
        int y = c.attribute("y").as_int(-1);
        if (y < 0 || y >= cfa.getSize().y) {
          ThrowCME("Invalid y coordinate in CFA array of camera %s %s",
                   make.c_str(), model.c_str());
        }
        std::string key = c.child_value();
        if (static_cast<int>(key.size()) != cfa.getSize().x) {
          ThrowCME("Invalid number of colors in definition for row %d in "
                   "camera %s %s. Expected %d, found %zu.",
                   y, make.c_str(), model.c_str(), cfa.getSize().x, key.size());
        }
        for (size_t x = 0; x < key.size(); ++x) {
          auto c1 = key[x];
          CFAColor c2;

          try {
            c2 = char2enum.at(static_cast<char>(tolower(c1)));
          } catch (const std::out_of_range&) {
            ThrowCME("Invalid color in CFA array of camera %s %s: %c",
                     make.c_str(), model.c_str(), c1);
          }

          cfa.setColorAt(iPoint2D(static_cast<int>(x), y), c2);
        }
      } else if (name(c) == "Color") {
        int x = c.attribute("x").as_int(-1);
        if (x < 0 || x >= cfa.getSize().x) {
          ThrowCME("Invalid x coordinate in CFA array of camera %s %s",
                   make.c_str(), model.c_str());
        }

        int y = c.attribute("y").as_int(-1);
        if (y < 0 || y >= cfa.getSize().y) {
          ThrowCME("Invalid y coordinate in CFA array of camera %s %s",
                   make.c_str(), model.c_str());
        }

        const auto* c1 = c.child_value();
        CFAColor c2;

        try {
          c2 = str2enum.at(c1);
        } catch (const std::out_of_range&) {
          ThrowCME("Invalid color in CFA array of camera %s %s: %s",
                   make.c_str(), model.c_str(), c1);
        }

        cfa.setColorAt(iPoint2D(x, y), c2);
      }
  }
}

void Camera::parseCrop(const xml_node &cur) {
  if (name(cur) != "Crop")
    ThrowCME("Not an Crop node!");

  cropSize.x = cur.attribute("width").as_int(0);
  cropSize.y = cur.attribute("height").as_int(0);
  cropPos.x = cur.attribute("x").as_int(0);
  cropPos.y = cur.attribute("y").as_int(0);

  if (cropPos.x < 0)
    ThrowCME("Negative X axis crop specified in camera %s %s", make.c_str(),
             model.c_str());
  if (cropPos.y < 0)
    ThrowCME("Negative Y axis crop specified in camera %s %s", make.c_str(),
             model.c_str());
}

void Camera::parseBlackAreas(const xml_node &cur) {

  if (name(cur) != "BlackAreas")
    ThrowCME("Not an BlackAreas node!");

  for (xml_node c : cur.children()) {
    if (name(c) == "Vertical") {
      int x = c.attribute("x").as_int(-1);
      if (x < 0) {
        ThrowCME(
            "Invalid x coordinate in vertical BlackArea of in camera %s %s",
            make.c_str(), model.c_str());
      }

      int w = c.attribute("width").as_int(-1);
      if (w < 0) {
        ThrowCME("Invalid width in vertical BlackArea of in camera %s %s",
                 make.c_str(), model.c_str());
      }

      blackAreas.emplace_back(x, w, true);

    } else if (name(c) == "Horizontal") {

      int y = c.attribute("y").as_int(-1);
      if (y < 0) {
        ThrowCME("Invalid y coordinate in horizontal BlackArea of camera %s %s",
                 make.c_str(), model.c_str());
      }

      int h = c.attribute("height").as_int(-1);
      if (h < 0) {
        ThrowCME("Invalid height in horizontal BlackArea of camera %s %s",
                 make.c_str(), model.c_str());
      }

      blackAreas.emplace_back(y, h, false);
    }
  }
}

void Camera::parseAliases(const xml_node &cur) {
  if (name(cur) != "Aliases")
    ThrowCME("Not an Aliases node!");

  for (xml_node c : cur.children("Alias")) {
    aliases.emplace_back(c.child_value());
    canonical_aliases.emplace_back(
        c.attribute("id").as_string(c.child_value()));
  }
}

void Camera::parseHints(const xml_node &cur) {
  if (name(cur) != "Hints")
    ThrowCME("Not an Hints node!");

  for (xml_node c : cur.children("Hint")) {
    std::string name = c.attribute("name").as_string();
    if (name.empty())
      ThrowCME("Could not find name for hint for %s %s camera.", make.c_str(),
               model.c_str());

    std::string value = c.attribute("value").as_string();

    hints.add(name, value);
  }
}

void Camera::parseID(const xml_node &cur) {
  if (name(cur) != "ID")
    ThrowCME("Not an ID node!");

  canonical_make = cur.attribute("make").as_string();
  if (canonical_make.empty())
    ThrowCME("Could not find make for ID for %s %s camera.", make.c_str(),
             model.c_str());

  canonical_alias = canonical_model = cur.attribute("model").as_string();
  if (canonical_model.empty())
    ThrowCME("Could not find model for ID for %s %s camera.", make.c_str(),
             model.c_str());

  canonical_id = cur.child_value();
}

void Camera::parseSensor(const xml_node &cur) {
  if (name(cur) != "Sensor")
    ThrowCME("Not an Sensor node!");

  auto stringToListOfInts = [&cur](const char* attribute) {
    vector<int> ret;
    for (const std::string& s :
         splitString(cur.attribute(attribute).as_string()))
      ret.push_back(stoi(s));
    return ret;
  };

  int min_iso = cur.attribute("iso_min").as_int(0);
  int max_iso = cur.attribute("iso_max").as_int(0);
  int black = cur.attribute("black").as_int(-1);
  int white = cur.attribute("white").as_int(65536);

  vector<int> black_colors = stringToListOfInts("black_colors");
  vector<int> iso_list = stringToListOfInts("iso_list");
  if (!iso_list.empty()) {
    for (int iso : iso_list) {
      sensorInfo.emplace_back(black, white, iso, iso, black_colors);
    }
  } else {
    sensorInfo.emplace_back(black, white, min_iso, max_iso, black_colors);
  }
}

void Camera::parseColorMatrix(const xml_node& cur) {
  if (name(cur) != "ColorMatrix")
    ThrowCME("Not an ColorMatrix node!");

  unsigned planes = cur.attribute("planes").as_uint(~0U);
  if (planes == ~0U)
    ThrowCME("Color matrix has unknown number of planes!");

  static constexpr int NumColsPerPlane = 3;
  color_matrix.resize(NumColsPerPlane * planes, NotARational<int>(0, 0));

  for (xml_node ColorMatrixRow : cur.children("ColorMatrixRow")) {
    if (name(ColorMatrixRow) != "ColorMatrixRow")
      ThrowCME("Not an ColorMatrixRow node!");

    auto plane = ColorMatrixRow.attribute("plane").as_uint(~0U);
    if (plane == ~0U || plane >= planes)
      ThrowCME("Color matrix row is for unknown plane!");

    const std::vector<std::string> ColsOfRow =
        splitString(ColorMatrixRow.text().as_string());

    if (ColsOfRow.size() != NumColsPerPlane)
      ThrowCME("Color matrix row has incorrect number of columns!");

    std::transform(ColsOfRow.begin(), ColsOfRow.end(),
                   color_matrix.begin() + NumColsPerPlane * plane,
                   [](const std::string& Col) -> NotARational<int> {
                     return {std::stoi(Col), 10'000};
                   });
  }
}

void Camera::parseColorMatrices(const xml_node& cur) {
  if (name(cur) != "ColorMatrices")
    ThrowCME("Not an ColorMatrices node!");

  for (xml_node ColorMatrix : cur.children("ColorMatrix"))
    parseColorMatrix(ColorMatrix);
}

void Camera::parseCameraChild(const xml_node &cur) {
  if (name(cur) == "CFA" || name(cur) == "CFA2") {
    parseCFA(cur);
  } else if (name(cur) == "Crop") {
    parseCrop(cur);
  } else if (name(cur) == "BlackAreas") {
    parseBlackAreas(cur);
  } else if (name(cur) == "Aliases") {
    parseAliases(cur);
  } else if (name(cur) == "Hints") {
    parseHints(cur);
  } else if (name(cur) == "ID") {
    parseID(cur);
  } else if (name(cur) == "Sensor") {
    parseSensor(cur);
  } else if (name(cur) == "ColorMatrices") {
    parseColorMatrices(cur);
  }
}
#endif

const CameraSensorInfo* Camera::getSensorInfo(int iso) const {
  if (sensorInfo.empty()) {
    ThrowCME("Camera '%s' '%s', mode '%s' has no <Sensor> entries.",
             make.c_str(), model.c_str(), mode.c_str());
  }

  // If only one, just return that
  if (sensorInfo.size() == 1)
    return &sensorInfo.front();

  vector<const CameraSensorInfo*> candidates;
  for (const auto& i : sensorInfo) {
    if (i.isIsoWithin(iso))
      candidates.push_back(&i);
  }

  if (candidates.size() == 1)
    return candidates.front();

  for (const auto* i : candidates) {
    if (!i->isDefault())
      return i;
  }

  // Several defaults??? Just return first
  return candidates.front();
}

} // namespace rawspeed
