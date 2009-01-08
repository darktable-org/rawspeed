#pragma once

#include <intrin.h>

#pragma intrinsic(_ReturnAddress)

class MathException : public std::runtime_error
{
public:
  MathException(const string _msg, void* _ret) : runtime_error(_msg) {
    _RPT2(0, "Math Exception: %s called from %p\n", _msg.c_str(), _ret);
  }
};
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#define THROW_MATH(x) throw MathException("Math Exception: " x " in " TOSTRING(__FILE__) " Ln " TOSTRING(__LINE__), _ReturnAddress() )

extern std::wstring toWideString( const char* pStr , int len=-1 ) ; 
inline std::wstring toWideString( const std::string& str )
{
    return toWideString(str.c_str(),str.length()) ;
}
inline std::wstring toWideString( const wchar_t* pStr , int len=-1 )
{
    return (len < 0) ? pStr : std::wstring(pStr,len) ;
}
inline std::wstring toWideString( const std::wstring& str )
{
    return str ;
}
extern std::string toNarrowString( const wchar_t* pStr , int len=-1 ) ; 
inline std::string toNarrowString( const std::wstring& str )
{
    return toNarrowString(str.c_str(),str.length()) ;
}
inline std::string toNarrowString( const char* pStr , int len=-1 )
{
    return (len < 0) ? pStr : std::string(pStr,len) ;
}
inline std::string toNarrowString( const std::string& str )
{
    return str ;
}


/* -------------------------------------------------------------------- */

std::wstring 
toWideString( const char* pStr , int len )
{
    _ASSERTE( pStr ) ; 
    _ASSERTE( len >= 0 || len == -1 ) ; 

    // figure out how many wide characters we are going to get 

    int nChars = MultiByteToWideChar( CP_ACP , 0 , pStr , len , NULL , 0 ) ; 
    if ( len == -1 )
        -- nChars ; 
    if ( nChars == 0 )
        return L"" ;

    // convert the narrow string to a wide string 

    // nb: slightly naughty to write directly into the string like this

    std::wstring buf ;
    buf.resize( nChars ) ; 
    MultiByteToWideChar( CP_ACP , 0 , pStr , len , 
        const_cast<wchar_t*>(buf.c_str()) , nChars ) ; 

    return buf ;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */

std::string 
toNarrowString( const wchar_t* pStr , int len )
{
    _ASSERTE( pStr ) ; 
    _ASSERTE( len >= 0 || len == -1 ) ; 

    // figure out how many narrow characters we are going to get 

    int nChars = WideCharToMultiByte( CP_ACP , 0 , 
             pStr , len , NULL , 0 , NULL , NULL ) ; 
    if ( len == -1 )
        -- nChars ; 
    if ( nChars == 0 )
        return "" ;

    // convert the wide string to a narrow string

    // nb: slightly naughty to write directly into the string like this

    std::string buf ;
    buf.resize( nChars ) ;
    WideCharToMultiByte( CP_ACP , 0 , pStr , len , 
          const_cast<char*>(buf.c_str()) , nChars , NULL , NULL ) ; 

    return buf ; 
}


class wruntime_error
    : public std::runtime_error
{

public:                 // --- PUBLIC INTERFACE ---


// constructors:

                        wruntime_error( const std::wstring& errorMsg ) ;
// copy/assignment:

                        wruntime_error( const wruntime_error& rhs ) ;
    wruntime_error&     operator=( const wruntime_error& rhs ) ;
// destructor:

    virtual             ~wruntime_error() ;

// exception methods:

    const std::wstring& errorMsg() const ;

private:                // --- DATA MEMBERS ---


// data members:

    std::wstring        mErrorMsg ; ///< Exception error message.
    
} ;

#ifdef _UNICODE
    #define truntime_error wruntime_error
#else 
    #define truntime_error runtime_error
#endif // _UNICODE

/* -------------------------------------------------------------------- */

wruntime_error::wruntime_error( const std::wstring& errorMsg )
    : std::runtime_error( toNarrowString(errorMsg) )
    , mErrorMsg(errorMsg)
{
    // NOTE: We give the runtime_error base the narrow version of the 
    //  error message. This is what will get shown if what() is called.
    //  The wruntime_error inserter or errorMsg() should be used to get 
    //  the wide version.
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */

wruntime_error::wruntime_error( const wruntime_error& rhs )
    : runtime_error( toNarrowString(rhs.errorMsg()) )
    , mErrorMsg(rhs.errorMsg())
{
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */

wruntime_error&
wruntime_error::operator=( const wruntime_error& rhs )
{
    // copy the wruntime_error
    runtime_error::operator=( rhs ) ; 
    mErrorMsg = rhs.mErrorMsg ; 

    return *this ; 
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */

wruntime_error::~wruntime_error()
{
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  */

const wstring& wruntime_error::errorMsg() const { return mErrorMsg ; }


typedef   bool          gboolean;
typedef   void*         gpointer;
typedef   const void*   gconstpointer;
typedef   char          gchar;
typedef   unsigned char guchar;

typedef  int           gint;
typedef  unsigned int  guint;
typedef  short           gshort;
typedef  unsigned short           gushort;
typedef  long           glong;
typedef  unsigned long           gulong;

typedef  char gint8;
typedef  unsigned char           guint8;
typedef  short           gint16;
typedef  unsigned short           guint16;
typedef  int           gint32;
typedef  unsigned int           guint32;

typedef  __int64       gint64;
typedef  unsigned __int64          guint64;

typedef float            gfloat;
typedef double            gdouble;

typedef unsigned int            gsize;
typedef signed int            gssize;
typedef gint64            goffset;
