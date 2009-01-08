#pragma once

class TiffParserException : public std::runtime_error
{
public:
  TiffParserException(const string _msg);
  //~TiffParserException(void);
//  const string msg;
};
