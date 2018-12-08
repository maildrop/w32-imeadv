#include <tchar.h>
#include <windows.h>
#include <iostream>
#include <sstream>
#include <string>
#include <array>
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
  switch( uMsg ){
    /* terminate this process */
  case WM_DESTROY:
    PostQuitMessage(0);
    break;
    
  case WM_W32_IMEADV_END: 
    return DestroyWindow( hWnd );

    /* private message WM_W32_IMEADV_* handlers  */
  case WM_W32_IMEADV_NULL:
    return notify_output( '*' );
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
  case WM_W32_IMEADV_REQUEST_BACKWARD_CHAR:
    return notify_output( 'b' );
  case WM_W32_IMEADV_REQUEST_DELETE_CHAR:
    return notify_output( 'd' );

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
    }
  }

  HANDLE emacsProcessHandle = 0;
  if( controlWindow ){
    DWORD processId{ 0 };
    DWORD threadId = GetWindowThreadProcessId( controlWindow , &processId );
    (void)( threadId );
    VERIFY( threadId );
    if( ! (emacsProcessHandle = OpenProcess( SYNCHRONIZE , FALSE , processId )) ){
      // OpenProcess() fail.
      DebugOutputStatic( "OpenProcesss() was failed." );
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

      if( emacsProcessHandle ){
        const std::array< HANDLE , 1 > selecter = { emacsProcessHandle };
        for(;;){
          DWORD msgWaitResult =
            MsgWaitForMultipleObjects( static_cast<DWORD>(selecter.size()) , selecter.data() ,
                                       FALSE , INFINITE , QS_ALLINPUT );
          switch( msgWaitResult ){
          case (WAIT_OBJECT_0 + 0 ): // emacsProcessHandle が signal 状態に移行した
            PostMessage( hWnd , WM_W32_IMEADV_END , 0 , 0 );
          break;
          
          case ( WAIT_OBJECT_0 + selecter.size() ):
            {
              MSG msg = {0};
              while( PeekMessageA( &msg , NULL , 0 ,0 , PM_REMOVE ) ){
                if( WM_QUIT == msg.message ){
                  goto end_of_message_pump;
                }
                ::TranslateMessage( &msg );
                ::DispatchMessageA( &msg );
              }
            }
            break;
          case WAIT_ABANDONED_0:
            DebugOutputStatic( "WAIT_ABANDONED_0" );
            goto end_of_message_pump;
          case WAIT_TIMEOUT:
            break;
          default:
            goto end_of_message_pump;
          }
        }
      }else{
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
      }
    end_of_message_pump:
      ;
    }
    VERIFY( UnregisterClass( reinterpret_cast<LPCTSTR>( windowAtom ) , hInstance) );
  }

  if( emacsProcessHandle ){
    VERIFY( CloseHandle( emacsProcessHandle ) );
  }

  return EXIT_SUCCESS;
}
