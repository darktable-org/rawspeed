rawspeed
========

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
- is extensively crash-tested on broken files.
- decodes images from memory, not a file stream. You can use a memory mapped file, but it is rarely faster.
- is currently tested on more than 150 unique cameras.
- can add support for new cameras by updating an xml file.
- open source under the LGPL v2 license.

RawSpeed does NOT…

- read any metadata information not related to decoding the image.
- do any color correction or whitebalance correction.
- de-mosaic the image.
- supply a viewable image or thumbnail.
- crop the image to the same sizes as manufactures, but supplies biggest possible images.

So RawSpeed is not intended to be a complete RAW file display library,  but only act as the first stage decoding, delivering the RAW data to your application.



##Getting Source Code

You can get access to the lastest version using subversion here. You will need to include the “RawSpeed” and “data” folder in your own project.

This includes a Microsoft Visual Studio project to build a test application. The test application uses libgfl to output 16 bit images. This library is not required for your own implementation though.

To see a GCC-based implementation, you can check out this directory, which is the implementation Rawstudio uses to load the images. This also includes an automake file to set up. You can also have a look at the darktable implementation, for which there is a Cmake based build file.


##Background of RawSpeed

So my main objectives were to make a very fast loader that worked for 75% of the cameras out there, and was able to decode a RAW file at close to the optimal speed. The last 25% of the cameras out there could be serviced by a more generic loader, or convert their images to DNG – which as a sidenote usually compresses better than your camera.

RawSpeed is not at the moment a separate library, so you have to include it in your project directly.


##Include files

All needed headers are available by including “RawSpeed-API.h”. You must have the pthread library and headers installed and available.

RawSpeed uses pthreads and libxml2, which is the only external requirements beside standard C/C++ libraries.

You must implement a single function: “int rawspeed_get_number_of_processor_cores();”, which should return the maximum number of threads that should be used for decoding, if multithreaded decoding is possible.

Everything is encapsulated on a “RawSpeed” namespace. To avoid clutter the examples below assume you have a “using namespace RawSpeed;” before using the code.

##The Camera Definition file

This file describes basic information about different cameras, so new cameras can be supported without code changes. See the separate documentation on the Camera Definition File.

The camera definitions are read into the CameraMetaData object, which you can retain for re-use later. You initialize this data by doing

```
static CameraMetaData *metadata = NULL;
if (NULL == metadata)
{
  try {
    metadata = new CameraMetaData("path_to_cameras.xml");
  } catch (CameraMetadataException &e) {
    // Reading metadata failed. e.what() will contain error message.
  }
}
```

The memory impact of this object is quite small, so you don’t have to free it every time. You can however delete and re-create it, if you know the metadata file has been updated.



##Using RawSpeed

You need to have the file data in a FileMap object. This can either be created by supplying the file content in memory using FileMap(buffer_pointer, size_of_buffer), or use a “FileReader” object to read the content of a file, like this:

```
FileReader reader(filename);
FileMap* map = NULL;
try {
  map = reader.readFile();
} catch (FileIOException &e) {
  // Handle errors
}
```

The next step is to start decoding. The first step is to get a decoder:

```
RawParser parser(map);
RawDecoder *decoder = parser.getDecoder();
```

This will do basic parsing of the file, and return a decoder that is capable of decoding the image. If no decoder can be found or another error occurs a “RawDecoderException” object will be thrown. The next step is to determine whether the specific camera is supported:

```
decoder->failOnUnknown = FALSE;
decoder->checkSupport(metadata);
```

The “failOnUnknown” property will indicate whether the decoder should refuse to decode unknown cameras. Otherwise RawSpeed will only refuse to decode the image, if it is confirmed that the camera type cannot be decoded correctly. If the image isn’t supported a “RawDecoderException” will be thrown.

Reaching this point should not take very long in terms of CPU time, so the support check is very quick, if file data is quickly available. Next we decode the image:

```
decoder->decodeRaw();
decoder->decodeMetaData(metadata);
RawImage raw = decoder->mRaw;
```

This will decode the image, and apply metadata information. The RawImage is at this point completely untouched Raw data, however the image has been cropped to the active image area in decodeMetaData. Error reporting is: If a fatal error occurs a RawDecoderException is thrown.

Non-fatal errors are pushed into a "vector" array in the decoder object called "errors". With these types of errors, there WILL be a raw image available, but it will likely contain junk sections in undecodable parts. However, as much as it was possible to decode will be available. So treat these messages as warnings.
Another thing to note here is that the RawImage object is automatically refcounted, so you can pass the object around  without worrying about the image being freed before all instances are out of scope.

```
raw->scaleBlackWhite();
```

This will apply the black/white scaling to the image, so the data is normalized into the 0->65535 range no matter what the sensor adjustment is (for 16 bit images). This function does no throw any errors. Now you can retrieve information about the image:

```
int components_per_pixel = raw->getCpp();
RawImageType type = raw->getDataType();
bool is_cfa = r->isCFA;
if (TRUE == is_cfa)
  ColorFilterArray cfa = raw->cfa;
```

Components per pixel indicates how many components are present per pixel. Common values are 1 on CFA images, and 3, found in some DNG images for instance.

The RawImageType can be TYPE_USHORT16 (most common) which indicates unsigned 16 bit data or TYPE_FLOAT32 (found in some DNGs)

The isCFA indicates whether the image has all components per pixel, or if it was taken with a colorfilter array. This usually corresponds to the number of components per pixel (1 on CFA, 3 on non-CFA).

The ColorfilterArray contains information about the placement of colors in the CFA:

```
int dcraw_filter = raw->cfa.getDcrawFilter();
CFAColor c = raw->cfa.getColorAt(0,0);
```

To get this information as a dcraw compatible filter information, you can use getDcrawFilter() function.

You can also use getColorAt(x, y) to get a single color information. Note that unlike dcraw, RawSpeed only supports 2×2 patterns, so you can reuse this information. CFAColor can be CFA_RED, CFA_GREEN, CFA_BLUE for instance.

Finally information about the image itself:

```
unsigned char* data = raw->getData(0,0);
int width = raw->dim.x;
int height = raw->dim.y;
int pitch_in_bytes = raw->pitch;
```

The getData(x, y) function will give you a pointer to the Raw data at pixel x, y. This is the coordinate after crop, so you can start copying data right away. Don’t use this function on every pixel, but instead increment the pointer yourself. The width and height gives you the size of the image in pixels – again after crop.

Pitch is the number of bytes between lines, since this is usually NOT width * components_per_pixel * bytes_per_component. So in this instance, to calculate a pointer at line y, use &data[y * raw->pitch] for instance.

Finally to clean up, use:

```
delete map;
delete decoder;
```

Actually the map and decoder can be deallocated once the metadata has been decoded. The RawImage will automatically be deallocated when it goes out of scope and the decoder has been deallocated. After that all data pointers that have been retrieved will no longer be usable.

##Tips & Tricks

You will most likely find that a relatively long time is spent actually reading the file. The biggest trick to speeding up raw reading is to have some sort of prefetching going on while the file is being decoded. This is the main reason why RawSpeed decodes from memory, and doesn’t use direct file reads while decoding.

The simplest solution is to start a thread that simply reads the file, and rely on the system cache to cache the data. This is fairly simple and works in 99% of all cases. So if you are doing batch processing simply start a process reading the next file, when the current image starts decoding. This will ensure that your file is read linearly, which gives the highest possible throughput.

A more complex option is to read the file to a memory portion, which is then given to RawSpeed to decode. This might be a few milliseconds faster in the best case, but I have found no practical difference between that and simply relying on system caching.

You might want to try out memory mapped files. However this approach has in practical tests shown to be just as fast in best cases (when file is cached), or slower (uncached files).

##Bad pixel elimination

A few cameras will mark bad pixels within their RAW files in various ways. For the camera we know how to this will be picked up by RawSpeed. By default these pixels are eliminated by 4-way interpolating to the closest valid pixels in an on-axis search from the current pixel.

If you want to do bad pixel interpolation yourself you can set:

```
RawDecoder.interpolateBadPixels = FALSE;
```

Before calling the decoder. This will disable the automatic interpolation of bad pixels. You can retrieve the bad pixels by using:

```
std::vector<uint32> RawImage->mBadPixelPositions;
```

This is a vector that contains the positions of the detected bad pixels in the image. The positions are stored as x | (y << 16), so maximum pixel position is 65535, which also corresponds with the limit of the image sizes within RawSpeed. you can loop through all bad pixels with a loop like this:

```
for (vector<uint32>::iterator i=mBadPixelPositions.begin(); i != mBadPixelPositions.end(); i++)  {
    uint32 pos_x = (*i)&0xffff;
    uint32 pos_y = (*i)>>16;
    ushort16* pix = (ushort16*)getDataUncropped(pos_x, pos_y);
}
```

This however may not be most optimal format for you. You can also call RawImage->transferBadPixelsToMap(). This will create a bit-mask for you with all bad pixels. Each byte correspond to 8 pixels with the least significant bit for the leftmost pixel. To set position x,y this operation is used:

```
RawImage->mBadPixelMap[(x >> 8) + y * mBadPixelMapPitch] |=  1 << (x & 7);
```

This enables you to quickly search through the array. If you for instance cast the array to integers you can check 32 pixels at the time.

Note that all positions are uncropped image positions. Also note that if you keep the interpolation enabled you can still retrieve the mBadPixelMap, but the mBadPixelPositions will be cleared.

##Updating Camera Support

If you implement an autoupdate feature, you simply update “cameras.xml” and delete and re-create the CameraMetaData object.

There might of course be some specific cameras that require code-changes to work properly. However, there is a versioning check inplace, whereby cameras requirering a specific code version to decode properly will be marked as such.

That means you should safely be able to update cameras.xml to a newer version, and cameras requiring a code update will then simply refuse to open.


##Format Specific Notes

Canon reduced resolution Raws (mRaw/sRaw) are returned as RGB with 3 component per pixel without whitebalance compensation, so color balance should match ordinary CR2 images. The subsampled color components are linearly interpolated.


##Memory Usage

RawSpeed will need:

* Size of Raw File.
* Image width * image height * 2 for ordinary Raw images with 16 bit output.
* Image width * image height * 4 for float point images with float point output .
* Image width * image height * 6 for ordinary Raw images with float point output.
* Image width * image height / 8 for images with bad pixels.

##Submitting Requests and Patches

Please go to the Rawstudio Bugzilla and submit your requests there.
