#include <tchar.h>
#include <windows.h>
#include <iostream>
#include <sstream>
#include <string>
#include <cassert>

#include "w32-imeadv.h"

#ifdef _MSC_VER
#pragma comment(lib,"user32.lib")
#endif /* defined( _MSC_VER ) */

static int
notify_output( int c )
{
  putchar( c );
  fflush( stdout );
  return 1; 
}

static LRESULT
(CALLBACK wnd_proc)( HWND hWnd , UINT uMsg , WPARAM wParam, LPARAM lParam );

static LRESULT
(CALLBACK wnd_proc)( HWND hWnd , UINT uMsg , WPARAM wParam, LPARAM lParam )
{
  if( WM_DESTROY == uMsg ){
    PostQuitMessage(0);
  }
  switch( uMsg ){
  case WM_W32_IMEADV_NULL:
    return notify_output( '*' );
  case WM_W32_IMEADV_END:
    return DestroyWindow( hWnd );
  case WM_W32_IMEADV_OPENSTATUS_OPEN :
    return notify_output( '1' );
  case WM_W32_IMEADV_OPENSTATUS_CLOSE :
    return notify_output( '0' );
  case WM_W32_IMEADV_REQUEST_RECONVERSION_STRING:
    return notify_output( 'R' );
  case WM_W32_IMEADV_REQUEST_DOCUMENTFEED_STRING:
    return notify_output( 'D' );
  case WM_W32_IMEADV_REQUEST_COMPOSITION_FONT:
    return notify_output( 'F' );
  default:
    break;
  }
  return ::DefWindowProc( hWnd , uMsg , wParam , lParam );
}

int main(int argc,char* argv[])
{
  HWND controlWindow = 0;
  if( 1 < argc ){
    HWND hWnd = reinterpret_cast<HWND>(static_cast<intptr_t>( atoi( argv[1] )));
    if( hWnd ){
      if( IsWindow( hWnd ) ){
        controlWindow = hWnd;
      }else{
        controlWindow = 0;
      }
#if !defined( NDEBUG )
      {
        std::stringstream out{};
        if( IsWindow( hWnd ) ){
          out << reinterpret_cast<intptr_t>(hWnd) << " IsWindow" << std::endl;
        }else{
          out << reinterpret_cast<intptr_t>(hWnd) << " IsWindow fail" << std::endl;
        }
        OutputDebugStringA( out.str().c_str() );
      }
#endif /* !defined( NDEBUG ) */
    }
  }
  
  HINSTANCE hInstance = GetModuleHandle( NULL );

  WNDCLASSEX wndClassEx    = {};
  {
    wndClassEx.cbSize        = sizeof( WNDCLASSEX );
    wndClassEx.style         = 0;
    wndClassEx.lpfnWndProc   = wnd_proc;
    wndClassEx.cbClsExtra    = 0;
    wndClassEx.cbWndExtra    = 0;
    wndClassEx.hInstance     = hInstance;
    wndClassEx.hIcon         = 0;
    wndClassEx.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wndClassEx.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wndClassEx.lpszMenuName  = 0;
    wndClassEx.lpszClassName = "EmacsIMM32EventProxyWindowClassA";
    wndClassEx.hIconSm       = 0;
  }
  
  ATOM windowAtom  = ::RegisterClassExA( &wndClassEx );
  if( windowAtom ){
    HWND hWnd = 
      CreateWindowExA(0,reinterpret_cast<LPCSTR>( windowAtom ) ,
                      "EmacsIMM32EventProxyWindow",
                      WS_OVERLAPPEDWINDOW,
                      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                      HWND_MESSAGE, NULL, hInstance, nullptr);    
    if( hWnd ){
      if( controlWindow ){
        if( IsWindow( controlWindow ) ){
          VERIFY( PostMessageA( controlWindow , WM_W32_IMEADV_NOTIFY_SIGNAL_HWND , (WPARAM)(hWnd), 0 ) );
        }
      }
      
      for(;;){
        MSG msg = {0};
        switch( GetMessageA( &msg , NULL , 0 , 0 ) ){
        case -1:
        case 0:
          goto end_of_message_pump;
        default:
          TranslateMessage( &msg );
          DispatchMessageA( &msg );
        }
      }
    end_of_message_pump:
      ;
    }
    VERIFY( UnregisterClass( reinterpret_cast<LPCTSTR>( windowAtom ) , hInstance) );
  }

  return EXIT_SUCCESS;
}
