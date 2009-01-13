#pragma once

void ThrowRDE(const char* fmt, ...);

class RawDecoderException : public std::runtime_error
{
public:
  RawDecoderException(const string _msg) : runtime_error(_msg) {
    _RPT1(0, "RawDecompressor Exception: %s\n", _msg.c_str());
  }
};