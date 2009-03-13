/* 
    RawSpeed - RAW file decoder.

    Copyright (C) 2009 Klaus Post

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA

    http://www.klauspost.com
*/

#include <rawstudio.h>
#include "StdAfx.h"
#include "FileReader.h"
#include "TiffParser.h"
#include "RawDecoder.h"
#include "CameraMetaData.h"
#include "rawstudio-plugin-api.h"

extern "C" {

RS_IMAGE16 *
load_rawspeed(const gchar *filename)
{
	static CameraMetaData *c = NULL;
	if (!c)
	{
		gchar *path = g_build_filename(rs_confdir_get(), "cameras.xml", NULL);
		c = new CameraMetaData(path);
		g_free(path);
	}
	RS_IMAGE16 *image = NULL;
	FileReader f((char *) filename);
	RawDecoder *d = 0;
	FileMap* m = 0;

	try
	{
		m = f.readFile();
		TiffParser t(m);
		t.parseData();
		d = t.getDecompressor();

		try
		{
			gint col, row;
			gint cpp;

			GTimer *gt = g_timer_new();
			d->decodeRaw();
			d->decodeMetaData(c);
			printf("%s: %.03f\n", filename, g_timer_elapsed(gt, NULL));
			g_timer_destroy(gt);

			for (guint i = 0; i < d->errors.size(); i++)
				printf("Error Encountered:%s\n", d->errors[i]);

			RawImage r = d->mRaw;
      r->scaleBlackWhite();

			cpp = r->getCpp();
			if (cpp == 1) 
				image = rs_image16_new(r->dim.x, r->dim.y, cpp, cpp);
			else if (cpp == 3) 
				image = rs_image16_new(r->dim.x, r->dim.y, 3, 3);
			else {
				printf("Unsupported component per pixel count");
				return NULL;
			}

			if (r->isCFA)
				image->filters = r->cfa.getDcrawFilter();

			if (r->isCFA) 
			{
				printf("DCRAW filter:%x\n",r->cfa.getDcrawFilter());
				printf("%s", r->cfa.asString().c_str());
			}
      if (cpp == 1) 
      {
        BitBlt((guchar *)(GET_PIXEL(image,0,0)),image->pitch*2,
          r->getData(0,0), r->pitch, r->dim.x*2, r->dim.y);
      } else 
      {
        for(row=0;row<image->h;row++)
        {
          gushort *inpixel = (gushort*)&r->getData()[row*r->pitch];
          gushort *outpixel = GET_PIXEL(image, 0, row);
          for(col=0;col<image->w;col++)
          {
            *outpixel++ =  *inpixel++;
            *outpixel++ =  *inpixel++;
            *outpixel++ =  *inpixel++;
            outpixel++;
          }
        }
      }
		}
		catch (RawDecoderException e)
		{
			printf("RawDecoderException: %s\n", e.what());
		}
	}
	catch (TiffParserException e)
	{
		printf("TiffParserException: %s\n", e.what());
	}

	if (d) delete d;
	if (m) delete m;

	return image;
}

} /* extern "C" */
