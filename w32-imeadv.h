#if !defined( W32_IMEADV_H_HEADER_GUARD_69b36924_1b02_4782_bf0b_b8c02d62065a )
#define W32_IMEADV_H_HEADER_GUARD_69b36924_1b02_4782_bf0b_b8c02d62065a 1

/**
   author: TOGURO Mikito mit@shalab.net
 */
#if defined( __cplusplus )
extern "C"{
#endif /* defined( __cplusplus ) */

  /* UI thread subclasss proc */
  LRESULT (CALLBACK subclass_proc)( HWND hWnd , UINT uMsg , WPARAM wParam , LPARAM lParam ,
                                    UINT_PTR uIdSubclass, DWORD_PTR dwRefData );
  
#if defined( __cplusplus )
}
#endif /* defined( __cplusplus ) */

#endif /* !defined( W32_IMEADV_H_HEADER_GUARD_69b36924_1b02_4782_bf0b_b8c02d62065a ) */

