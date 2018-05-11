rawspeed |travis-ci| |appveyor-ci| OBS_ |codecov| |coverity status|

.. |travis-ci| image:: https://travis-ci.org/darktable-org/rawspeed.svg?branch=develop
    :target: https://travis-ci.org/darktable-org/rawspeed

.. |appveyor-ci| image:: https://ci.appveyor.com/api/projects/status/7pqy0gdr9mp16xu2/branch/develop?svg=true
    :target: https://ci.appveyor.com/project/LebedevRI/rawspeed/branch/develop

.. _OBS: https://build.opensuse.org/project/monitor/graphics:darktable:master

.. |codecov| image:: https://codecov.io/gh/darktable-org/rawspeed/branch/develop/graph/badge.svg
    :target: https://codecov.io/gh/darktable-org/rawspeed

.. |coverity status| image:: https://scan.coverity.com/projects/11256/badge.svg
    :target: https://scan.coverity.com/projects/darktable-org-rawspeed

================================================================================
RawSpeed Developer Information
================================================================================
What is RawSpeed?
--------------------------------------------------------------------------------

RawSpeed…

- is capable of decoding various images in RAW file format.
- is intended to provide the fastest decoding speed possible.
- supports the most common DSLR and similar class brands.
- supplies unmodified RAW data, optionally scaled to 16 bit, or normalized to 0->1 float point data.
- supplies CFA layout for all known cameras.
- provides automatic black level calculation for cameras having such information.
- optionally crops off  “junk” areas of images, containing no valid image information.
- can add support for new cameras by adding definitions to an xml file.
- decodes images from memory, not a file stream.
- is being continuously fuzzed as part of the `oss-fuzz`_ project.
- is currently tested on |rpu-button-cameras| unique cameras, on |rpu-button-samples| unique samples.
  **Please read** `this <rpu-post_>`_ **for more info on how to contribute samples!**
- open source under the `LGPL v2`_ license.

.. _oss-fuzz: https://github.com/google/oss-fuzz

.. |rpu-button-cameras| image:: https://raw.pixls.us/button-cameras.svg
    :target: https://raw.pixls.us/

.. |rpu-button-samples| image:: https://raw.pixls.us/button-samples.svg
    :target: https://raw.pixls.us/

.. _rpu-post: https://discuss.pixls.us/t/raw-samples-wanted/5420?u=lebedevri

.. _LGPL v2: https://choosealicense.com/licenses/lgpl-2.1/

RawSpeed does **NOT**…

- read metadata information, beside whitebalance information.
- do any color correction or whitebalance correction.
- de-mosaic the image.
- supply a viewable image or thumbnail.
- crop the image to the same sizes as manufactures, but supplies biggest possible images.

So RawSpeed is not intended to be a complete RAW file display library,  but only act as the first stage decoding, delivering the RAW data to your application.

Version 2, new cameras and features
--------------------------------------------------------------------------------
- Support for Sigma foveon cameras.
- Support for Fuji cameras.
- Support old Minolta, Panasonic, Sony cameras (contributed by Pedro Côrte-Real)
- Arbitrary CFA definition sizes.
- Use pugixml_ for xml parsing to avoid depending on libxml.

.. _pugixml: http://pugixml.org/

Getting Source Code
--------------------------------------------------------------------------------
You can get access to the lastest version using `from here <rawspeed_>`_. You will need to include the “RawSpeed” and “data” folder in your own project.

CMake-based build system is provided.

Background of RawSpeed
----------------------
The main objectives were to make a very fast loader that worked for 75% of the cameras out there, and was able to decode a RAW file at close to the optimal speed. The last 25% of the cameras out there could be serviced by a more generic loader, or convert their images to DNG – which as a sidenote usually compresses better than your camera.

RawSpeed is not at the moment a separate library, so you have to include it in your project directly.

Please see <https://rawspeed.org/> for documentation.
Doxygen-generated documentation is available at <https://rawspeed.org/doxygen>

Submitting Requests and Patches
--------------------------------------------------------------------------------
Please go to the `github page <rawspeed_>`_ and submit your (pull)requests and issues there.

.. _rawspeed: https://github.com/darktable-org/rawspeed
