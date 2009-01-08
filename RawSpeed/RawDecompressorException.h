#pragma once

void ThrowRDE(const char* fmt, ...);

class RawDecompressorException : public std::runtime_error
{
public:
  RawDecompressorException(const string _msg) : runtime_error(_msg) {
    _RPT1(0, "RawDecompressor Exception: %s\n", _msg.c_str());
  }
};