#include <tchar.h>
#include <windows.h>
#include <iostream>
#include <sstream>
#include <string>
#include <new>
#include <memory>
#include <array>
#include <tuple>
#include <cstdlib>
#include <cstring>
#include <cassert>

#include "emacs-module.h"
#include "w32-imeadv.h"

extern "C" __declspec( dllexport ) void CALLBACK 
EntryPointW( HWND hWnd , HINSTANCE hInstance , LPWSTR lpszCmdLine , int nCmd );
extern "C" __declspec( dllexport ) void CALLBACK
EtnryPoint( HWND hWnd , HINSTANCE hInstance , LPSTR lpszCmdLine,  int nCmd );
static int
notify_output( int c );
static LRESULT
(CALLBACK imm32_input_proxy_wnd_proc)( HWND hWnd , UINT uMsg , WPARAM wParam, LPARAM lParam );

static const wchar_t msgbox_title[] = L"Error Report (w32-imeadv.dll)";

static int
notify_output( int c )
{
  putchar( c );
  fflush( stdout );
  return 1; 
}

static LRESULT
(CALLBACK imm32_input_proxy_wnd_proc)( HWND hWnd , UINT uMsg , WPARAM wParam, LPARAM lParam )
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

extern "C" __declspec( dllexport ) void CALLBACK 
EntryPointW( HWND rundll_hWnd , HINSTANCE hInstance , LPWSTR lpszCmdLine , int nCmd )
{
  std::ignore = nCmd;
  int argc(0);
  LPWSTR *argv = CommandLineToArgvW( lpszCmdLine , &argc );
  if( argv )
    {
      do{
        if( argc <= 0 ){
          break;
        }
        assert( 0 < argc );
        unsigned long long wnd_handle_integer{0};
        if( (! swscanf( argv[0] , L"%I64u" , &wnd_handle_integer ))
            ||  wnd_handle_integer == 0 ){
          ::MessageBoxW( rundll_hWnd ,
                         L"実行にはウィンドウハンドルが必要です。" ,
                         msgbox_title,
                         MB_OK | MB_ICONERROR);
          break;
        }
        // sizeof( unsigned long long ) more than sizeof( HWND ) and same signed. 
        static_assert( sizeof( decltype( wnd_handle_integer ) ) >= sizeof( HWND ),
                       "sizeof( decltype( wnd_handle_integer ) ) >= sizeof( HWND )" );
        
        HWND const control_window{reinterpret_cast<HWND>( wnd_handle_integer )};
        if(! IsWindow( control_window ) ){
          ::MessageBoxW( rundll_hWnd ,
                         L"引数に与えられた整数は、有効なウィンドウハンドルではありません" ,
                         msgbox_title ,
                         MB_OK | MB_ICONERROR);
          break;
        }

        // Do something;
        HANDLE emacsProcessHandle = 0;
        if( control_window ){
          DWORD processId{ 0 };
          DWORD threadId = GetWindowThreadProcessId( control_window , &processId );
          (void)( threadId );
          VERIFY( threadId );
          if( ! (emacsProcessHandle = OpenProcess( SYNCHRONIZE , FALSE , processId )) ){
            // OpenProcess() fail.
            DebugOutputStatic( "OpenProcesss() was failed." );
          }
        }
        
        WNDCLASSEX wndClassEx    = {};
        {
          wndClassEx.cbSize        = sizeof( WNDCLASSEX );
          wndClassEx.style         = 0;
          wndClassEx.lpfnWndProc   = imm32_input_proxy_wnd_proc;
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
                            rundll_hWnd, NULL, hInstance, nullptr);    
          if( hWnd ){
            VERIFY( PostMessageA( control_window , WM_W32_IMEADV_NOTIFY_SIGNAL_HWND , (WPARAM)(hWnd), 0 ) );
            
            if( emacsProcessHandle ){
              const std::array< HANDLE , 1 > selecter = { emacsProcessHandle };
              for(;;){
                DWORD msgWaitResult =
                  MsgWaitForMultipleObjectsEx( static_cast<DWORD>(selecter.size()) , selecter.data() ,
                                               INFINITE , QS_ALLINPUT , MWMO_INPUTAVAILABLE );
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
        
      }while( false );
      
      if( LocalFree( argv ) ){
        ::MessageBoxW( rundll_hWnd ,
                       L"LocalFree() Error" ,
                       msgbox_title,
                       MB_OK | MB_ICONERROR );
      }
    }
  return;
}

extern "C" __declspec( dllexport ) void CALLBACK
EtnryPoint( HWND hWnd , HINSTANCE hInstance , LPSTR lpszCmdLine,  int nCmd )
{
  const size_t cmdline_len = strlen( lpszCmdLine );
  const int store_size = MultiByteToWideChar( CP_ACP , 0 ,
                                              lpszCmdLine , static_cast<int>(cmdline_len) ,
                                              nullptr , 0 );
  if( 0 < store_size ){
    std::unique_ptr<wchar_t[]> buf{ new (std::nothrow) wchar_t[ store_size + 1 ] };
    if( static_cast<bool>(buf) ){
      if( store_size ==
          MultiByteToWideChar(CP_ACP,0,
                              lpszCmdLine , static_cast<int>(cmdline_len) ,
                              buf.get() , store_size ) ){
        buf[store_size] = L'\0';
        EntryPointW( hWnd, hInstance , buf.get() , nCmd );
      }
    }
  }
  return;
}
