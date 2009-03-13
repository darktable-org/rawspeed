#pragma once

void ThrowCME(const char* fmt, ...);

class CameraMetadataException :
  public std::runtime_error
{
public:
  CameraMetadataException(const string _msg);
};
