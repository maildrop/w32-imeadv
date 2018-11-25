#include <tchar.h>
#include <windows.h>
#include <commctrl.h>
#include <imm.h>
#include <cstdint>
#include <cassert>

#include "w32-imeadv.h"

static void
w32_imm_wm_ime_composition( HWND hWnd , WPARAM* wParam , LPARAM *lParam )
{
  if( *lParam & GCS_RESULTSTR )
    {
      HIMC hImc = ImmGetContext( hWnd );
      if(! hImc ) return;
      const LONG byte_size = ImmGetCompositionStringW( hImc , GCS_RESULTSTR , NULL , 0 );

      switch( byte_size ){
      case IMM_ERROR_NODATA:
      case IMM_ERROR_GENERAL:
      case 0:
        break;
      default:
        {
          /* byte_size is supposed even, odd values ​​since also take ,
             and to 2 by adding a minute fraction .*/
          size_t const character_length =
            (((size_t)((ULONG)byte_size) )/sizeof( wchar_t )) + 2 ;

          LPVOID himestr = HeapAlloc( GetProcessHeap() ,
                                      HEAP_ZERO_MEMORY ,
                                      (SIZE_T)(sizeof(wchar_t) * character_length ));
          if( himestr ){
            const LONG copydata_bytes =
              ImmGetCompositionStringW( hImc , GCS_RESULTSTR , himestr , sizeof( wchar_t ) * character_length );
            assert( static_cast<size_t>(copydata_bytes) < (character_length * sizeof( wchar_t ) ));

            switch( copydata_bytes ){
            case IMM_ERROR_NODATA:
            case IMM_ERROR_GENERAL:
            case 0:
              break;
            default:
              {
                if( byte_size == copydata_bytes ){
                  wchar_t* lpStr = ( wchar_t *)himestr;
                  size_t const loop_end = (((size_t)((ULONG)byte_size) )/sizeof( wchar_t ));
                  for( size_t i = 0 ; i < loop_end ; ++i ){
                    /* considering UTF-16 surrogate pair */
                    if( L'\0' == lpStr[i] ) break;
                    uint32_t utf32_code = 0; /* UTF32 */
                    if ((lpStr[i] & 0xFC00) == 0xD800         /* This is hight surrogate */
                        && (lpStr[i + 1] & 0xFC00) == 0xDC00) /* This is low surrogate */
                      { /* (lpStr[i]) and ( lpStr[i+1] ) make surrogate pair. */
                        /*  make UTF-32 codepoint from UTF-16 surrogate pair */
                        utf32_code = 0x10000 + (((lpStr[i] & 0x3FF) << 10) | (lpStr[i + 1] & 0x3FF));
                        ++i; /* surrogate pair were processed ,so advance lpStr pointer */
                      }
                    else
                      { /* (lpStr[i]) is pointing BMP */
                        utf32_code = lpStr[i];
                      }
                    /* TODO validate utf-32 codepoint */

                    if( utf32_code )
                      SendMessageW( hWnd , WM_UNICHAR , (WPARAM) utf32_code , 0 );
                  } /* end of cracking IME Result string and queueing WM_UNICHARs. */
                }
              }
            }

            if( ! HeapFree( GetProcessHeap() , 0 , himestr ) ){
              assert( !"! HeapFree( GetProcessHeap() , 0 , himestr ) " );
            }
          }
        }
        break;
      }
      ImmReleaseContext( hWnd , hImc );
      *lParam &= (~GCS_RESULTSTR );
    } // end of if ( *lParam & GCS_RESULTSTR )
  return;
}

LRESULT (CALLBACK subclass_proc)( HWND hWnd , UINT uMsg , WPARAM wParam , LPARAM lParam ,
                                  UINT_PTR uIdSubclass, DWORD_PTR dwRefData ){
  UNREFERENCED_PARAMETER( dwRefData );
  if( WM_DESTROY == uMsg ){
    ::RemoveWindowSubclass( hWnd , subclass_proc , uIdSubclass);
  }

  if( WM_KEYDOWN == uMsg ){ 
    switch( wParam ){
    case VK_PROCESSKEY:
      { 
        const MSG msg = { hWnd , uMsg , wParam , lParam , 0 , {0} };
        TranslateMessage(&msg);
        return ::DefWindowProc( hWnd, uMsg , wParam , lParam );
      }
      break;
    default:
      break;
    }
  }else if ( WM_IME_COMPOSITION == uMsg ){
    w32_imm_wm_ime_composition( hWnd , &wParam , &lParam );
    return DefWindowProc( hWnd , uMsg , wParam , lParam );
  }else if ( WM_W32_IMEADV_SUBCLASSIFY == uMsg ) {
    HWND communication_window_handle = (HWND)(wParam);
    PostMessage( communication_window_handle , WM_W32_IMEADV_SUBCLASSIFY , (WPARAM)( hWnd ) , 0);
  }else if( WM_IME_STARTCOMPOSITION == uMsg ){

    // deny break in WM_IME_STARTCOMPOSITION , call DefWindowProc 
    // Emacs のバージョンで切り分けるという作業をしないとダメですよ
    ::DefSubclassProc( hWnd , uMsg , wParam , lParam );
    return DefWindowProc( hWnd, uMsg , wParam , lParam );
  }
  
  return DefSubclassProc (hWnd, uMsg , wParam , lParam );
}
