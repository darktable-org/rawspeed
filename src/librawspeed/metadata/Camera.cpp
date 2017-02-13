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
#include "common/Common.h"                    // for split_string, uint32
#include "common/Point.h"                     // for iPoint2D
#include "metadata/CameraMetadataException.h" // for ThrowCME
#include <cctype>                             // for tolower
#include <cstdio>                             // for size_t
#include <map>                                // for map
#include <pugixml.hpp>                        // for xml_node, xml_attribute
#include <stdexcept>                          // for out_of_range
#include <string>                             // for string, allocator, ope...
#include <vector>                             // for vector

using namespace std;

namespace RawSpeed {

using namespace pugi;

Camera::Camera(pugi::xml_node &camera) : cfa(iPoint2D(0,0)) {
  make = canonical_make = camera.attribute("make").as_string();
  if (make.empty())
    ThrowCME(R"(Camera XML Parser: "make" attribute not found.)");

  model = canonical_model = canonical_alias = camera.attribute("model").as_string();
  // chdk cameras seem to have an empty model?
  if (!camera.attribute("model")) // (model.empty())
    ThrowCME(R"(Camera XML Parser: "model" attribute not found.)");

  canonical_id = make + " " + model;

  supported = camera.attribute("supported").as_string("yes") == string("yes");
  mode = camera.attribute("mode").as_string("");
  decoderVersion = camera.attribute("decoder_version").as_int(0);

  for (xml_node c : camera.children()) {
    parseCameraChild(c);
  }
}

Camera::Camera(const Camera* camera, uint32 alias_num) : cfa(iPoint2D(0,0))
{
  if (alias_num >= camera->aliases.size())
    ThrowCME("Camera: Internal error, alias number out of range specified.");

  *this = *camera;
  model = camera->aliases[alias_num];
  canonical_alias = camera->canonical_aliases[alias_num];
  aliases.clear();
  canonical_aliases.clear();
}

static string name(const xml_node &a) {
  return string(a.name());
}

static const map<char, CFAColor> char2enum = {
    {'g', CFA_GREEN},      {'r', CFA_RED},  {'b', CFA_BLUE},
    {'f', CFA_FUJI_GREEN}, {'c', CFA_CYAN}, {'m', CFA_MAGENTA},
    {'y', CFA_YELLOW},
};

static const map<string, CFAColor> str2enum = {
    {"GREEN", CFA_GREEN},   {"RED", CFA_RED},
    {"BLUE", CFA_BLUE},     {"FUJI_GREEN", CFA_FUJI_GREEN},
    {"CYAN", CFA_CYAN},     {"MAGENTA", CFA_MAGENTA},
    {"YELLOW", CFA_YELLOW},
};

void Camera::parseCFA(const xml_node &cur) {
  if (name(cur) != "CFA" && name(cur) != "CFA2")
    ThrowCME("parseCFA(): Not an CFA/CFA2 node!");

  cfa.setSize(iPoint2D(cur.attribute("width").as_int(0),
                       cur.attribute("height").as_int(0)));
  for (xml_node c : cur.children()) {
      if (name(c) == "ColorRow") {
        int y = c.attribute("y").as_int(-1);
        if (y < 0 || y >= cfa.getSize().y) {
          ThrowCME("Invalid y coordinate in CFA array of camera %s %s",
                   make.c_str(), model.c_str());
        }
        string key = c.child_value();
        if ((int)key.size() != cfa.getSize().x) {
          ThrowCME("Invalid number of colors in definition for row %d in "
                   "camera %s %s. Expected %d, found %zu.",
                   y, make.c_str(), model.c_str(), cfa.getSize().x, key.size());
        }
        for (size_t x = 0; x < key.size(); ++x) {
          auto c1 = key[x];
          CFAColor c2;

          try {
            c2 = char2enum.at((char)tolower(c1));
          } catch (std::out_of_range&) {
            ThrowCME("Invalid color in CFA array of camera %s %s: %c",
                     make.c_str(), model.c_str(), c1);
          }

          cfa.setColorAt(iPoint2D((int)x, y), c2);
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

        auto c1 = c.child_value();
        CFAColor c2;

        try {
          c2 = str2enum.at(c1);
        } catch (std::out_of_range&) {
          ThrowCME("Invalid color in CFA array of camera %s %s: %s",
                   make.c_str(), model.c_str(), c1);
        }

        cfa.setColorAt(iPoint2D(x, y), c2);
      }
  }
}

void Camera::parseCrop(const xml_node &cur) {
  if (name(cur) != "Crop")
    ThrowCME("parseCrop(): Not an Crop node!");

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
    ThrowCME("parseBlackAreas(): Not an BlackAreas node!");

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
    ThrowCME("parseAliases(): Not an Aliases node!");

  for (xml_node c : cur.children("Alias")) {
    aliases.emplace_back(c.child_value());
    canonical_aliases.emplace_back(
        c.attribute("id").as_string(c.child_value()));
  }
}

void Camera::parseHints(const xml_node &cur) {
  if (name(cur) != "Hints")
    ThrowCME("parseHints(): Not an Hints node!");

  for (xml_node c : cur.children("Hint")) {
    string name = c.attribute("name").as_string();
    if (name.empty())
      ThrowCME("CameraMetadata: Could not find name for hint for %s %s camera.",
               make.c_str(), model.c_str());

    string value = c.attribute("value").as_string();

    hints.add(name, value);
  }
}

void Camera::parseID(const xml_node &cur) {
  if (name(cur) != "ID")
    ThrowCME("parseID(): Not an ID node!");

  string id_make = cur.attribute("make").as_string();
  if (id_make.empty())
    ThrowCME("CameraMetadata: Could not find make for ID for %s %s camera.",
             make.c_str(), model.c_str());

  string id_model = cur.attribute("model").as_string();
  if (id_model.empty())
    ThrowCME("CameraMetadata: Could not find model for ID for %s %s camera.",
             make.c_str(), model.c_str());

  canonical_make = id_make;
  canonical_model = id_model;
  canonical_alias = id_model;
  canonical_id = cur.child_value();
}

void Camera::parseSensor(const xml_node &cur) {
  if (name(cur) != "Sensor")
    ThrowCME("parseSensor(): Not an Sensor node!");

  auto stringToListOfInts = [this, &cur](const char *attribute) {
    vector<int> ret;
    try {
      for (const string& s : splitString(cur.attribute(attribute).as_string()))
        ret.push_back(stoi(s));
    } catch (...) {
      ThrowCME("Error parsing attribute %s in tag %s, in camera %s %s.",
               attribute, cur.name(), make.c_str(), model.c_str());
    }
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
  }
}

const CameraSensorInfo* Camera::getSensorInfo( int iso )
{
  if (sensorInfo.empty()) {
    ThrowCME(
        "getSensorInfo(): Camera '%s' '%s', mode '%s' has no <Sensor> entries.",
        make.c_str(), model.c_str(), mode.c_str());
  }

  // If only one, just return that
  if (sensorInfo.size() == 1)
    return &sensorInfo.front();

  vector<CameraSensorInfo*> candidates;
  for (auto& i : sensorInfo) {
    if (i.isIsoWithin(iso))
      candidates.push_back(&i);
  }

  if (candidates.size() == 1)
    return candidates.front();

  for (auto i : candidates) {
    if (!i->isDefault())
      return i;
  }

  // Several defaults??? Just return first
  return candidates.front();
}

} // namespace RawSpeed
