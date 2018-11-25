#if !defined( W32_IMEADV_H_HEADER_GUARD_69b36924_1b02_4782_bf0b_b8c02d62065a )
#define W32_IMEADV_H_HEADER_GUARD_69b36924_1b02_4782_bf0b_b8c02d62065a 1

#define WM_W32_IMEADV_START         (WM_APP + 1028)
#define WM_W32_IMEADV_SUBCLASSIFY   (WM_W32_IMEADV_START + 0)
#define WM_W32_IMEADV_UNSUBCLASSIFY (WM_W32_IMEADV_START + 1)
#define WM_W32_IMEADV_END           (WM_W32_IMEADV_START + 2)

#if defined(__cplusplus)
static_assert( WM_W32_IMEADV_END < 0xffff , "WM_W32_IMEADV_END < 0xffff");
#endif /* defined(__cplusplus) */

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

#if defined( __cplusplus )
namespace w32_imeadv {
  BOOL initialize();
  BOOL finalize();
  BOOL subclassify_hwnd( HWND hWnd , DWORD_PTR dwRefData);
};
#endif /* defined( __cplusplus ) */

#endif /* !defined( W32_IMEADV_H_HEADER_GUARD_69b36924_1b02_4782_bf0b_b8c02d62065a ) */

