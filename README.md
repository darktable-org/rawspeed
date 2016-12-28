rawspeed [![travis-ci](https://travis-ci.org/darktable-org/rawspeed.svg?branch=develop)](https://travis-ci.org/darktable-org/rawspeed) [![appveyor-ci](https://ci.appveyor.com/api/projects/status/7pqy0gdr9mp16xu2/branch/develop?svg=true)](https://ci.appveyor.com/project/LebedevRI/rawspeed/branch/develop) [![codecov](https://codecov.io/gh/darktable-org/rawspeed/branch/develop/graph/badge.svg)](https://codecov.io/gh/darktable-org/rawspeed) [![coverity status](https://scan.coverity.com/projects/11256/badge.svg)](https://scan.coverity.com/projects/darktable-org-rawspeed)
=========

#RawSpeed Developer Information

##What is RawSpeed?

RawSpeed…

- is capable of decoding various images in RAW file format.
- is intended to provide the fastest decoding speed possible.
- supports the most common DSLR and similar class brands.
- supplies unmodified RAW data, optionally scaled to 16 bit, or normalized to 0->1 float point data.
- supplies CFA layout for all known cameras.
- provides automatic black level calculation for cameras having such information.
- optionally crops off  “junk” areas of images, containing no valid image information.
- can add support for new cameras by adding definitions to an xml file.
- ~~is extensively crash-tested on broken files~~.
- decodes images from memory, not a file stream. You can use a memory mapped file, but it is rarely faster.
- is currently tested on more than 500 unique cameras.
- open source under the LGPL v2 license.

RawSpeed does NOT…

- read metadata information, beside whitebalance information.
- do any color correction or whitebalance correction.
- de-mosaic the image.
- supply a viewable image or thumbnail.
- crop the image to the same sizes as manufactures, but supplies biggest possible images.

So RawSpeed is not intended to be a complete RAW file display library,  but only act as the first stage decoding, delivering the RAW data to your application.

##Version 2, new cameras and features
- Support for Sigma foveon cameras.
- Support for Fuji cameras.
- Support old Minolta, Panasonic, Sony cameras (contributed by Pedro Côrte-Real)
- Arbitrary CFA definition sizes.
- Use [pugixml](http://pugixml.org/) for xml parsing to avoid depending on libxml.


##Getting Source Code

You can get access to the lastest version using [from here](https://github.com/darktable-org/rawspeed). You will need to include the “RawSpeed” and “data” folder in your own project.

CMake-based build system is provided.

##Background of RawSpeed

The main objectives were to make a very fast loader that worked for 75% of the cameras out there, and was able to decode a RAW file at close to the optimal speed. The last 25% of the cameras out there could be serviced by a more generic loader, or convert their images to DNG – which as a sidenote usually compresses better than your camera.

RawSpeed is not at the moment a separate library, so you have to include it in your project directly.

##Please see [RawSpeed/README.md](RawSpeed/README.md) for usage instructions.

##Submitting Requests and Patches

Please go to the [github page](https://github.com/darktable-org/rawspeed) and submit your (pull)requests and issues there.
