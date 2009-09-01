/*
 * Copyright (C) 2006-2009 Anders Brander <anders@brander.dk> and
 * Anders Kvist <akv@lnxbx.dk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <rawstudio.h>
#include "rawstudio-plugin-api.h"

G_MODULE_EXPORT void
rs_plugin_load(RSPlugin *plugin)
{
	rs_filetype_register_loader(".arw", "Sony", load_rawspeed, 5);
	rs_filetype_register_loader(".cr2", "Canon CR2", load_rawspeed, 5);
	rs_filetype_register_loader(".dng", "Adobe Digital Negative", load_rawspeed, 5);
	rs_filetype_register_loader(".nef", "Nikon NEF", load_rawspeed, 5);
	rs_filetype_register_loader(".orf", "Olympus", load_rawspeed, 5);
	rs_filetype_register_loader(".pef", "Pentax raw", load_rawspeed, 5);
	rs_filetype_register_loader(".rw2", "Panasonic raw", load_rawspeed, 5);
}
