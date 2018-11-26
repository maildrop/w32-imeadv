#include <tchar.h>
#include <windows.h>
#include <commctrl.h>
#include <mutex>
#include <tuple>
#include <cassert>
#include <string>
#include <sstream>

#include "w32-imeadv.h"
#include "emacs-module.h"
#include "w32-imeadv-on-lispy-thread.h"

// now implementation 
namespace w32_imeadv {
};

static struct UserData{
  std::mutex mutex;
  ATOM windowAtom;
  HWND communication_window_handle;
} user_data = {};

static LRESULT
w32_imeadv_lispy_communication_wnd_proc_impl( UserData* user_data ,
                                              HWND hWnd, UINT uMsg , WPARAM wParam , LPARAM lParam );
static LRESULT
(CALLBACK w32_imeadv_lispy_communication_wnd_proc)(HWND hWnd, UINT uMsg , WPARAM wParam , LPARAM lParam );

static LRESULT
w32_imeadv_lispy_communication_wnd_proc_impl( UserData* user_data ,
                                              HWND hWnd, UINT uMsg , WPARAM wParam , LPARAM lParam )
{
  OutputDebugStringA("w32_imeadv_lispy_communication_wnd_proc_impl");
  if( ! user_data ){
    OutputDebugStringA( "user_data is nullptr" );
    return 0;
  }
  if( WM_W32_IMEADV_SUBCLASSIFY == uMsg ){
    OutputDebugStringA("w32_imeadv_lispy_communication_wnd_proc_impl message\n" );
    return 0;
  }
  return ::DefWindowProc(hWnd, uMsg, wParam , lParam);
}

static LRESULT
(CALLBACK w32_imeadv_lispy_communication_wnd_proc)(HWND hWnd, UINT uMsg , WPARAM wParam , LPARAM lParam )
{
  if( WM_CREATE == uMsg ){
    CREATESTRUCTA* createstruct = reinterpret_cast<CREATESTRUCTA*>( lParam );
    assert( createstruct->lpCreateParams );
    if( createstruct ){
      SetWindowLongPtr( hWnd , GWLP_USERDATA , reinterpret_cast<LONG_PTR>( createstruct->lpCreateParams ));
    }
  }

  UserData* user_data = reinterpret_cast<UserData*>( GetWindowLongPtr( hWnd,  GWLP_USERDATA ) );

#if !defined( NDEBUG )
  if( WM_CREATE == uMsg ){
    assert( reinterpret_cast<LONG_PTR>(user_data) ==
            reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTA*>(lParam)->lpCreateParams) );
  }
#endif /* !defined( NDEBUG ) */

  if( WM_DESTROY == uMsg ){
    SetWindowLongPtr( hWnd , GWLP_USERDATA , 0 );
  }
  return w32_imeadv_lispy_communication_wnd_proc_impl( user_data , hWnd , uMsg , wParam , lParam );
}

BOOL w32_imeadv::initialize()
{
  if( user_data.communication_window_handle ){
    return TRUE;
  }
  HINSTANCE hInstance = GetModuleHandle( NULL );
  if( !user_data.windowAtom ){
    WNDCLASSEX wndClassEx = {};
    wndClassEx.cbSize        = sizeof( WNDCLASSEX );
    wndClassEx.style         = 0;
    wndClassEx.lpfnWndProc   = w32_imeadv_lispy_communication_wnd_proc;
    wndClassEx.cbClsExtra    = 0;
    wndClassEx.cbWndExtra    = 0;
    wndClassEx.hInstance     = hInstance;
    wndClassEx.hIcon         = 0;
    wndClassEx.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wndClassEx.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    wndClassEx.lpszMenuName  = 0;
    wndClassEx.lpszClassName = "EmacsIMM32CommunicationWindowClassA";
    wndClassEx.hIconSm       = 0;
    user_data.windowAtom = ::RegisterClassExA( &wndClassEx );
  }
  if( user_data.windowAtom ){
    user_data.communication_window_handle =
      CreateWindowExA(0,reinterpret_cast<LPCSTR>( user_data.windowAtom ) ,
                      "EmacsIMM32CommunicationWindow",
                      WS_OVERLAPPEDWINDOW,
                      CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                      HWND_MESSAGE, NULL, hInstance, &user_data);
  }
  return (user_data.communication_window_handle) ? TRUE : FALSE;
}

BOOL w32_imeadv::finalize()
{
  if( user_data.communication_window_handle ){
    if( ! DestroyWindow( user_data.communication_window_handle ) ){
      
    }
    user_data.communication_window_handle = NULL;
  }
  
  if( user_data.windowAtom ){
    UnregisterClass( reinterpret_cast<LPCTSTR>(user_data.windowAtom) , GetModuleHandle( nullptr ));
    user_data.windowAtom = 0;
  }
  return TRUE;
}

BOOL w32_imeadv::subclassify_hwnd( HWND hWnd , DWORD_PTR dwRefData)
{
  OutputDebugStringA( "w32_imeadv::subclassify_hwnd\n" );
  struct {
    std::mutex mutex{};
    HHOOK subclassify_hook = 0;
  } static hook_parameter{};
  static std::mutex this_function{};

  std::unique_lock<decltype( this_function )> this_function_lock{ this_function };

  if( !hWnd ) return FALSE;
  if( !IsWindow( hWnd ) ) return FALSE;

  // inject UI thread
  LRESULT (CALLBACK *getMsgProc)( int, WPARAM ,LPARAM) =
    []( int code , WPARAM wParam , LPARAM lParam )->LRESULT
    {
      std::unique_lock<decltype(hook_parameter.mutex)> lock( hook_parameter.mutex );
      if( code < 0 )
        return ::CallNextHookEx( hook_parameter.subclassify_hook , code , wParam , lParam );
      else
        {
          MSG* msg = reinterpret_cast<MSG*>(lParam);
          if( msg )
            do{
              if(! msg->hwnd )
                break;
              if( msg->message == WM_W32_IMEADV_SUBCLASSIFY )
                {
                  // TODO implement this point
                  assert( GetCurrentThreadId() == GetWindowThreadProcessId( msg->hwnd , nullptr ));

                  SetWindowSubclass( msg->hwnd ,
                                     subclass_proc ,
                                     reinterpret_cast<UINT_PTR>( subclass_proc ),
                                     static_cast<DWORD_PTR>( msg->lParam ) );
                  
                  auto nexthook_result = ::CallNextHookEx( hook_parameter.subclassify_hook , code , wParam , lParam );
                  if( ! UnhookWindowsHookEx( hook_parameter.subclassify_hook ) ){
#if !defined( NDEBUG )
                    OutputDebugStringA( "UnhookWindowsHookEx faild" );
#endif /* !defined( NDEBUG ) */
                  }
                  hook_parameter.subclassify_hook = 0;
                  return nexthook_result;
                }
            }while( false );
          return ::CallNextHookEx( hook_parameter.subclassify_hook , code , wParam , lParam );
        }
    };

  DWORD target_input_thread_id = GetWindowThreadProcessId ( hWnd , nullptr );

  {
    std::unique_lock<decltype( hook_parameter.mutex )> lock( hook_parameter.mutex );
    hook_parameter.subclassify_hook =
      SetWindowsHookEx( WH_GETMESSAGE , getMsgProc , GetModuleHandle( NULL ) , target_input_thread_id );
  }
  PostMessage( hWnd , WM_W32_IMEADV_SUBCLASSIFY ,
              reinterpret_cast<WPARAM>(user_data.communication_window_handle) , static_cast<LPARAM>( dwRefData ) );
  return FALSE;
}
