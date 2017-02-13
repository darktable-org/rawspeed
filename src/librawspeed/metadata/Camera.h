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

#pragma once

#include "common/Common.h"             // for uint32
#include "common/Point.h"              // for iPoint2D
#include "metadata/BlackArea.h"        // for BlackArea
#include "metadata/CameraSensorInfo.h" // for CameraSensorInfo
#include "metadata/ColorFilterArray.h" // for ColorFilterArray
#include <map>                         // for map, _Rb_tree_const_iterator
#include <sstream>                     // for istringstream, basic_istream
#include <string>                      // for string, basic_string, operator>>
#include <utility>                     // for pair
#include <vector>                      // for vector

namespace pugi {
class xml_node;
} // namespace pugi

namespace RawSpeed {

class Hints
{
  std::map<std::string, std::string> data;
public:
  void add(const std::string& key, const std::string& value)
  {
    data.insert({key, value});
  }

  bool has(const std::string& key) const
  {
    return data.find(key) != data.end();
  }

  template <typename T>
  T get(const std::string& key, T defaultValue) const
  {
    auto hint = data.find(key);
    if (hint != data.end() && !hint->second.empty()) {
      std::istringstream iss(hint->second);
      iss >> defaultValue;
    }
    return defaultValue;
  }

  bool get(const std::string& key, bool defaultValue) const {
    auto hint = data.find(key);
    if (hint == data.end())
      return defaultValue;
    return "true" == hint->second;
  }
};

class Camera
{
public:
  Camera(pugi::xml_node &camera);
  Camera(const Camera* camera, uint32 alias_num);
  const CameraSensorInfo* getSensorInfo(int iso);
  std::string make;
  std::string model;
  std::string mode;
  std::string canonical_make;
  std::string canonical_model;
  std::string canonical_alias;
  std::string canonical_id;
  std::vector<std::string> aliases;
  std::vector<std::string> canonical_aliases;
  ColorFilterArray cfa;
  bool supported;
  iPoint2D cropSize;
  iPoint2D cropPos;
  std::vector<BlackArea> blackAreas;
  std::vector<CameraSensorInfo> sensorInfo;
  int decoderVersion;
  Hints hints;
protected:
  void parseCFA(const pugi::xml_node &node);
  void parseCrop(const pugi::xml_node &node);
  void parseBlackAreas(const pugi::xml_node &node);
  void parseAliases(const pugi::xml_node &node);
  void parseHints(const pugi::xml_node &node);
  void parseID(const pugi::xml_node &node);
  void parseSensor(const pugi::xml_node &node);

  void parseCameraChild(const pugi::xml_node &node);
};

} // namespace RawSpeed
