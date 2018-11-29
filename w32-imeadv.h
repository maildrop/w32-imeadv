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
#define WM_W32_IMEADV_OPENSTATUS_OPEN             (WM_W32_IMEADV_START + 4)
#define WM_W32_IMEADV_OPENSTATUS_CLOSE            (WM_W32_IMEADV_START + 5)
#define WM_W32_IMEADV_REQUEST_COMPOSITION_FONT    (WM_W32_IMEADV_START + 6)
#define WM_W32_IMEADV_NOTIFY_COMPOSITION_FONT     (WM_W32_IMEADV_START + 7)
#define WM_W32_IMEADV_REQUEST_RECONVERSION_STRING (WM_W32_IMEADV_START + 8)
#define WM_W32_IMEADV_NOTIFY_RECONVERSION_STRING  (WM_W32_IMEADV_START + 9)
#define WM_W32_IMEADV_REQUEST_DOCUMENTFEED_STRING (WM_W32_IMEADV_START + 10)
#define WM_W32_IMEADV_NOTIFY_DOCUMENTFEED_STRING  (WM_W32_IMEADV_START + 11)
#define WM_W32_IMEADV_END                         (WM_W32_IMEADV_START + 12)

/* TOOO check Window Message Last number */
#if defined(__cplusplus) 
static_assert( WM_W32_IMEADV_END < 0xffff , "WM_W32_IMEADV_END < 0xffff");
#endif /* defined(__cplusplus) */

#if defined( __cplusplus )
extern "C"{
#endif /* defined( __cplusplus ) */

  enum W32_IMEADV_COMPOSITION_FONT_CONFIGURE_BITS{
    W32_IME_FONT_CONFIGURE_BIT_FACENAME   = (1 << 0),
    W32_IME_FONT_CONFIGURE_BIT_FONTHEIGHT = (1 << 1),
    W32_IME_FONT_CONFIGURE_BIT_STRIKEOUT  = (1 << 2),
    W32_IME_FONT_CONFIGURE_BIT_UNDERLINE  = (1 << 3),
    W32_IME_FONT_CONFIGURE_BIT_ITALIC     = (1 << 4),
  };
  
  struct w32_imeadv_composition_font_configure{
    unsigned char enable_bits; // @see W32_IME_FONT_CONFIGURE_BITS
    unsigned char lfItalic;
    unsigned char lfUnserline;
    unsigned char lfStrikeOut;
    int32_t       font_height;
    wchar_t       lfFaceName[ LF_FACESIZE ]; // important ! wide char 
  };

  /* UI thread subclasss proc */
  LRESULT (CALLBACK subclass_proc)( HWND hWnd , UINT uMsg , WPARAM wParam , LPARAM lParam ,
                                    UINT_PTR uIdSubclass, DWORD_PTR dwRefData );


#if defined( __cplusplus )
}
#endif /* defined( __cplusplus ) */

#if defined( __cplusplus )
#if !defined( NDEBUG )
inline 
std::wostream&
operator<<( std::wostream& out , const w32_imeadv_composition_font_configure &config )
{
  out << L"w32_imeadv_composition_font_configure{" ;
  if( config.enable_bits & W32_IME_FONT_CONFIGURE_BIT_FACENAME ){
    out << L" family: \"" << config.lfFaceName <<  L"\", ";
  }
  if( config.enable_bits & W32_IME_FONT_CONFIGURE_BIT_FONTHEIGHT ){
    out << L" height: " << config.font_height << L", ";
  }
  out << L"}; ";
  return out;
}
#endif /* !defined( NDEBUG ) */
#endif /* defined( __cplusplus ) */


#endif /* !defined( W32_IMEADV_H_HEADER_GUARD_69b36924_1b02_4782_bf0b_b8c02d62065a ) */

