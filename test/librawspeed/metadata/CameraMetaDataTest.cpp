/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2009-2014 Klaus Post
    Copyright (C) 2016 Roman Lebedev

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

#include "rawspeedconfig.h"          // for HAVE_PUGIXML, RAWSPEED_SOURCE_DIR
#include "metadata/Camera.h"         // for Camera
#include "metadata/CameraMetaData.h" // for CameraMetaData
#include <gtest/gtest.h>             // for Test, Message, TestPartResult
#include <memory>                    // for allocator, unique_ptr
#include <string>                    // for string, basic_string

using rawspeed::CameraMetaData;
using std::unique_ptr;

namespace rawspeed_test {

#ifdef HAVE_PUGIXML

static const std::string camfile(RAWSPEED_SOURCE_DIR "/data/cameras.xml");

TEST(CameraMetaDataTest, CompileTimeCanInherit) {
  struct MyCameraMetaData : public CameraMetaData {};
}

TEST(CameraMetaDataTest, CamerasXml) {
  ASSERT_NO_THROW({ CameraMetaData Data(camfile.c_str()); });

  ASSERT_NO_THROW({
    unique_ptr<CameraMetaData> Data(new CameraMetaData(camfile.c_str()));
  });
}

TEST(CameraMetaDataTest, PrefixSearch) {
  ASSERT_NO_THROW({
    CameraMetaData Data(camfile.c_str());

    const auto* d3 =
        Data.getCamera("NIKON CORPORATION", "NIKON D3", "14bit-compressed");
    ASSERT_NE(nullptr, d3);
    ASSERT_EQ("D3", d3->canonical_model);

    ASSERT_EQ(nullptr,
              Data.getCamera("NIKON CORPORATION", "NIKON D3",
                             "14bit-compressed-with-some-bogus-prefix"));
    ASSERT_EQ(nullptr, Data.getCamera("NIKON CORPORATION",
                                      "NIKON D3-with-some-bogus-prefix",
                                      "14bit-compressed"));
    ASSERT_EQ(nullptr,
              Data.getCamera("NIKON CORPORATION-with-some-bogus-prefix",
                             "NIKON D3", "14bit-compressed"));

    d3 = Data.getCamera("NIKON CORPORATION", "NIKON D3");
    ASSERT_NE(nullptr, d3);
    ASSERT_EQ("D3", d3->canonical_model);

    ASSERT_EQ(nullptr, Data.getCamera("NIKON CORPORATION",
                                      "NIKON D3-with-some-bogus-prefix"));
    ASSERT_EQ(
        nullptr,
        Data.getCamera("NIKON CORPORATION-with-some-bogus-prefix", "NIKON D3"));
    ASSERT_EQ(nullptr,
              Data.getCamera("NIKON CORPORATION-with-some-bogus-prefix",
                             "NIKON D3-with-some-bogus-prefix"));
  });
}

#endif

} // namespace rawspeed_test
