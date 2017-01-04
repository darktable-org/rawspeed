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

#include "CameraMetaData.h"
#include <gmock/gmock.h> // for InitGoogleTest, RUN_ALL_TESTS
#include <iostream>      // for operator<<, basic_ostream, basic...

using namespace RawSpeed;

std::string camfile;

int main(int argc, char **argv) {
  const std::string self(argv[0]);

  const std::size_t lastslash = self.find_last_of("/\\");
  const std::string bindir(self.substr(0, lastslash));

  camfile = bindir + "/../../data/cameras.xml";

  testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}

TEST(CameraMetaDataTest, CamerasXml) {
  ASSERT_NO_THROW({ CameraMetaData Data(camfile.c_str()); });

  ASSERT_NO_THROW({
    CameraMetaData *Data = new CameraMetaData(camfile.c_str());
    delete Data;
  });
}
