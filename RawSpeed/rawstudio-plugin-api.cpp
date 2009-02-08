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
#include "rawstudio-plugin-api.h"

extern "C" {

RS_IMAGE16 *
load_rawspeed(const gchar *filename)
{
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
			gint black = 0xffff;
			gint max = 0x0;
			gint shift = 0;
			gint cpp;

			GTimer *gt = g_timer_new();
			d->decodeRaw();
      d->decodeMetaData();
			printf("%s: %.03f\n", filename, g_timer_elapsed(gt, NULL));
			g_timer_destroy(gt);

			for (guint i = 0; i < d->errors.size(); i++)
				printf("Error Encountered:%s\n", d->errors[i]);

			RawImage r = d->mRaw;

			cpp = r->getCpp();

			image = rs_image16_new(r->dim.x, r->dim.y, cpp, cpp);
			BitBlt((guchar *) GET_PIXEL(image, 0, 0), image->pitch*2*cpp, r->getData(), r->pitch, r->dim.x*r->bpp, r->dim.y);

			if (cpp==1)
				image->filters = r->cfa.getDcrawFilter();

			/* Calculate black and white point */
			for(row=100;row<image->h-100;row++)
			{
				gushort *pixel = GET_PIXEL(image, 100, row);
				for(col=100;col<image->w*cpp-100;col++)
				{
					max = MAX(*pixel, max);
					black = MIN(*pixel, black);
					pixel++;
				}
			}
			
			shift = (gint) (16.0-log((gdouble) max)/log(2.0));

			/* Apply black and whitepoint */
			for(row=0;row<image->h;row++)
			{
				gushort *pixel = GET_PIXEL(image, 0, row);
				for(col=0;col<image->w*cpp;col++)
				{
					*pixel =  clampbits(((gint)*pixel-black)<<shift,16);
					pixel++;
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
