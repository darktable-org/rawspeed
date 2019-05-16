/*
    RawSpeed - RAW file decoder.

    Copyright (C) 2019 Roman Lebedev

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

// The goal here is to have an executable that *always* compiles to a different
// binary, *every* compile. This seems counter-productive, but that is needed
// to "fool" LNT into always consider the target changed.

extern const char* const unique_hash __attribute__((visibility("default"))) =
    __TIMESTAMP__;
int main(int argc, char* argv[]) { return 0; }
