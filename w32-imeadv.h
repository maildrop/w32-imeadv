#if !defined( W32_IMEADV_H_HEADER_GUARD_69b36924_1b02_4782_bf0b_b8c02d62065a )
#define W32_IMEADV_H_HEADER_GUARD_69b36924_1b02_4782_bf0b_b8c02d62065a 1

/**
   Emacs dynamic module w32-imeadv -- w32-imeadv.h 
   author: TOGURO Mikito mit@shalab.net
*/

#define WM_W32_IMEADV_START                       (WM_APP + 828)
#define WM_W32_IMEADV_NULL                        (WM_W32_IMEADV_START + 0)
#define WM_W32_IMEADV_SUBCLASSIFY                 (WM_W32_IMEADV_START + 1)
#define WM_W32_IMEADV_UNSUBCLASSIFY               (WM_W32_IMEADV_START + 2)
#define WM_W32_IMEADV_NOTIFY_SIGNAL_HWND          (WM_W32_IMEADV_START + 3)
#define WM_W32_IMEADV_GET_OPENSTATUS              (WM_W32_IMEADV_START + 4)
#define WM_W32_IMEADV_OPENSTATUS_OPEN             (WM_W32_IMEADV_START + 5)
#define WM_W32_IMEADV_OPENSTATUS_CLOSE            (WM_W32_IMEADV_START + 6)
#define WM_W32_IMEADV_REQUEST_COMPOSITION_FONT    (WM_W32_IMEADV_START + 7)
#define WM_W32_IMEADV_NOTIFY_COMPOSITION_FONT     (WM_W32_IMEADV_START + 8)
#define WM_W32_IMEADV_REQUEST_RECONVERSION_STRING (WM_W32_IMEADV_START + 9)
#define WM_W32_IMEADV_NOTIFY_RECONVERSION_STRING  (WM_W32_IMEADV_START + 10)
#define WM_W32_IMEADV_REQUEST_DOCUMENTFEED_STRING (WM_W32_IMEADV_START + 11)
#define WM_W32_IMEADV_NOTIFY_DOCUMENTFEED_STRING  (WM_W32_IMEADV_START + 12)

#define WM_W32_IMEADV_REQUEST_BACKWARD_CHAR       (WM_W32_IMEADV_START + 20) // 後で順番変えるよ
#define WM_W32_IMEADV_REQUEST_DELETE_CHAR         (WM_W32_IMEADV_START + 21) // 後で順番変えるよ

#define WM_W32_IMEADV_END                         (WM_W32_IMEADV_START + 22)

/* TOOO check Window Message Last number */
#if defined(__cplusplus) 
static_assert( WM_W32_IMEADV_END < 0xffff , "WM_W32_IMEADV_END < 0xffff");
#endif /* defined(__cplusplus) */

#if defined( __cplusplus )
extern "C"{
#endif /* defined( __cplusplus ) */

  enum W32_IMEADV_COMPOSITION_FONT_CONFIGURE_BITS{
    W32_IMEADV_FONT_CONFIGURE_BIT_FACENAME   = (1 << 0),
    W32_IMEADV_FONT_CONFIGURE_BIT_FONTHEIGHT = (1 << 1),
    W32_IMEADV_FONT_CONFIGURE_BIT_STRIKEOUT  = (1 << 2),
    W32_IMEADV_FONT_CONFIGURE_BIT_UNDERLINE  = (1 << 3),
    W32_IMEADV_FONT_CONFIGURE_BIT_ITALIC     = (1 << 4),
  };
  
  struct w32_imeadv_composition_font_configure{
    unsigned char enable_bits; // @see W32_IME_FONT_CONFIGURE_BITS
    unsigned char lfItalic;
    unsigned char lfUnserline;
    unsigned char lfStrikeOut;
    int32_t       font_height;
    wchar_t       lfFaceName[ LF_FACESIZE ]; // important ! wide char 
  };

  /* WM_W32_IMEADV_REQUEST_BACKWARD_CHAR */
  struct w32_imeadv_request_backward_char_lparam{
    HWND hWnd; // request ui window handle
    size_t num;
  };
  
  /* WM_W32_IMEADV_REQUEST_DELETE_CHAR */
  struct w32_imeadv_request_delete_cahr_lparam{
    HWND hWnd; // request ui window handle
    size_t num;
  };
  
  /* UI thread subclasss proc */
  LRESULT (CALLBACK subclass_proc)( HWND hWnd , UINT uMsg , WPARAM wParam , LPARAM lParam ,
                                    UINT_PTR uIdSubclass, DWORD_PTR dwRefData );


#if defined( __cplusplus )
}
#endif /* defined( __cplusplus ) */


/** 
    Debug macros.
 */
#if !defined(DEBUG_STRING00)
#define DEBUG_STRING00( exp ) # exp 
#endif /* !defined(DEBUG_STRING00) */
#if !defined(DEBUG_STRING0)
#define DEBUG_STRING0( exp ) DEBUG_STRING00( exp )
#endif /* !defined(DEBUG_STRING0) */
#if !defined( DEBUG_STRING )
#define DEBUG_STRING( exp ) exp  " (@" __FILE__ ",L." DEBUG_STRING0( __LINE__ ) ")"
#endif /* !defined( DEBUG_STRING ) */

#if !defined( DebugOutputStatic )
#define DebugOutputStatic( exp ) do{ OutputDebugString( TEXT(DEBUG_STRING( exp )) TEXT("\n") ); }while( false )
#endif /* !defined( DebugOutputStatic ) */

/* simple VERIFY macros */
#if !defined( VERIFY )
# if defined( NDEBUG )
#if defined( __cplusplus )
#define VERIFY( exp ) do{ std::ignore = ( exp ); }while( false )
#else /* defined( __cplusplus ) */
#define VERIFY( exp ) (void)( exp ) 
#endif /* defined( __cplusplus ) */
# else /* defined( NDEBUG ) */ 
#define VERIFY( exp ) do{ assert( exp ); }while( false )
# endif /* defined( NDEBUG ) */
#endif /* !defined( VERIFY ) */



#if defined( __cplusplus )

#if !defined( NDEBUG )
inline 
std::wostream&
operator<<( std::wostream& out , const w32_imeadv_composition_font_configure &config )
{
  out << L"w32_imeadv_composition_font_configure{" ;
  if( config.enable_bits & W32_IMEADV_FONT_CONFIGURE_BIT_FACENAME ){
    out << L" family: \"" << config.lfFaceName <<  L"\", ";
  }
  if( config.enable_bits & W32_IMEADV_FONT_CONFIGURE_BIT_FONTHEIGHT ){
    out << L" height: " << config.font_height << L", ";
  }
  out << L"}; ";
  return out;
}
#endif /* !defined( NDEBUG ) */

namespace imeadv{
  template<typename type_t>
  constexpr inline ptrdiff_t
  byte_offset_of(const type_t* base ,const type_t *off )
  {
    static_assert( 1 == sizeof(const int8_t ) , "1 == sizeof( int8_t )" );
    return (static_cast<const int8_t*>( static_cast<const void*>( off ))
            - static_cast<const int8_t*>( static_cast<const void*>( base )));
  }
  
  inline const char16_t*
  advance_consider_surroage( const char16_t * p , size_t i){
    for( size_t j = 0 ; j < i && *p != L'\0' ; ++j, ++p ){
      if( ( 0xd800 <= *p && *p < 0xdc00 ) /* High Surrogate */ ){
        if( (0xdc00 <= *(p+1) && *(p+1) < 0xE000 ) /* Low Surroage */ ){
          ++p;
        }else{
          // illegal encoding 
        }
      }
    }
    return p;
  }

#ifdef _WIN32
  template<size_t N>
  const wchar_t*
  advance_surroage( const wchar_t (&p)[N] , size_t i ){
    static_assert(sizeof( char16_t ) == sizeof( wchar_t ) && 
                  std::is_same<typename std::is_unsigned<char16_t>::type ,
                  typename std::is_unsigned<wchar_t>::type >::value ,"");
    return reinterpret_cast<const wchar_t*>(advance_consider_surroage( reinterpret_cast<const char16_t *>(&p[0]) , i ));
  }
#endif /* _WIN32 */

  // LPARAM 
  struct NotifyReconversionString{
    intmax_t pos;
    intmax_t begin;
    intmax_t end;
    intmax_t first_half_num;
    intmax_t later_half_num;
    std::wstring first_half;
    std::wstring later_half;
  };
  
} /* end of namespace imeadv */

#endif /* defined( __cplusplus ) */


#endif /* !defined( W32_IMEADV_H_HEADER_GUARD_69b36924_1b02_4782_bf0b_b8c02d62065a ) */

