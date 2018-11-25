#include <tchar.h>
#include <windows.h>
#include <commctrl.h>
#include <imm.h>

#include "w32-imeadv.h"

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
  }

  return DefSubclassProc (hWnd, uMsg , wParam , lParam );
}
