CC=g++
CFLAGS=-fPIC -Wall -O4 `pkg-config --cflags rawstudio-1.1`
LDFLAGS=`pkg-config --libs rawstudio-1.1`
INSTALLPATH=`pkg-config --variable=plugindir rawstudio-1.1`

CPP_SOURCES= rawstudio-plugin-api.cpp \
	ArwDecoder.cpp \
	BitPumpJPEG.cpp \
	BitPumpMSB.cpp \
	BitPumpPlain.cpp \
	ByteStream.cpp \
	ColorFilterArray.cpp \
	Common.cpp \
	Cr2Decoder.cpp \
	DngDecoder.cpp \
	DngDecoderSlices.cpp \
	FileIOException.cpp \
	FileMap.cpp \
	FileReader.cpp \
	LJpegDecompressor.cpp \
	LJpegPlain.cpp \
	NefDecoder.cpp \
	NikonDecompressor.cpp \
	OrfDecoder.cpp \
	PefDecoder.cpp \
	PentaxDecompressor.cpp \
	RawDecoder.cpp \
	RawDecoderException.cpp \
	RawImage.cpp \
	Rw2Decoder.cpp \
	StdAfx.cpp \
	TiffEntryBE.cpp \
	TiffEntry.cpp \
	TiffIFDBE.cpp \
	TiffIFD.cpp \
	TiffParser.cpp \
	TiffParserException.cpp \
	TiffParserOlympus.cpp
CPP_OBJECTS=$(CPP_SOURCES:.cpp=.o)

all: $(CPP_OBJECTS) $(CPP_SOURCES) load-rawspeed.so

load-rawspeed.o: rawstudio-plugin.c
	gcc -c $(CFLAGS) $< -o $@

.cpp.o:
	$(CC) -c $(CFLAGS) $< -o $@

load-rawspeed.so: $(CPP_OBJECTS) load-rawspeed.o
	g++ $(CFLAGS) $(LDFLAGS) -shared -o load-rawspeed.so $(CPP_OBJECTS) load-rawspeed.o -lc

install: load-rawspeed.so
	cp -a load-rawspeed.so $(INSTALLPATH)

clean:
	rm -f *.o *.so
