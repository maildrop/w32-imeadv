#include <tchar.h>
#include <windows.h>
#include <commctrl.h>
#include <mutex>
#include <tuple>
#include <cassert>
#include <string>
#include <sstream>
#include <cstdint>

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
  HWND signal_window;
} user_data = {};

namespace w32_imeadv{
  namespace implements{
    HWND& get_communication_HWND_impl();
  };
};

static LRESULT
w32_imeadv_lispy_communication_wnd_proc_impl( UserData* user_data_ptr ,
                                              HWND hWnd, UINT uMsg , WPARAM wParam , LPARAM lParam );
static LRESULT
(CALLBACK w32_imeadv_lispy_communication_wnd_proc)(HWND hWnd, UINT uMsg , WPARAM wParam , LPARAM lParam );

static LRESULT
w32_imeadv_lispy_communication_wnd_proc_impl( UserData* user_data_ptr ,
                                              HWND hWnd, UINT uMsg , WPARAM wParam , LPARAM lParam )
{
  OutputDebugStringA("w32_imeadv_lispy_communication_wnd_proc_impl");

  if( WM_W32_IMEADV_SUBCLASSIFY == uMsg ){
    OutputDebugStringA("w32_imeadv_lispy_communication_wnd_proc_impl WM_W32_IMEADV_SUBCLASSIFY message\n" );
    return 0;
  }
  
  // implementation note . この時点では、まだ、ロックがかかっていないので、user_dataの中身に触る前に、ロックをかけること
  if( WM_W32_IMEADV_NOTIFY_SIGNAL_HWND == uMsg ){
      OutputDebugStringA("w32_imeadv_lispy_communication_wnd_proc_impl WM_W32_IMEADV_NOTIFY_SIGNAL_HWND message\n");
      if( user_data_ptr ){
        std::unique_lock<decltype(user_data_ptr->mutex)> lock{ user_data_ptr->mutex };
        user_data_ptr->signal_window = (HWND)(wParam);
      }
      return 0;
  }
  switch( uMsg ){
  case WM_W32_IMEADV_NULL :
    {
      if( user_data_ptr ){
        std::unique_lock<decltype(user_data_ptr->mutex)> lock{ user_data_ptr->mutex };
        if( user_data_ptr->signal_window ){
          PostMessage( user_data_ptr->signal_window , WM_W32_IMEADV_NULL , 0 ,0 );
        }
      }
      return 0;
    }
  case WM_W32_IMEADV_OPENSTATUS_OPEN:
    {
      if( user_data_ptr ){
        std::unique_lock<decltype(user_data_ptr->mutex)> lock{ user_data_ptr->mutex };
        if( user_data_ptr->signal_window ){
          PostMessage( user_data_ptr->signal_window , WM_W32_IMEADV_OPENSTATUS_OPEN , 0 ,0 );
        }
      }
      return 0;
    }
  case WM_W32_IMEADV_OPENSTATUS_CLOSE:
    {
      if( user_data_ptr ){
        std::unique_lock<decltype(user_data_ptr->mutex)> lock{ user_data_ptr->mutex };
        if( user_data_ptr->signal_window ){
          PostMessage( user_data_ptr->signal_window , WM_W32_IMEADV_OPENSTATUS_CLOSE , 0 ,0 );
        }
      }
      return 0;
    }
  case WM_W32_IMEADV_REQUEST_COMPOSITION_FONT:
    {
      if( user_data_ptr ){
        std::unique_lock<decltype(user_data_ptr->mutex)> lock{ user_data_ptr->mutex };
        if( user_data_ptr->signal_window ){
          return SendMessage( user_data_ptr->signal_window , WM_W32_IMEADV_REQUEST_COMPOSITION_FONT , 0, 0 );
        }
      }
      return 0;
    }
  case WM_W32_IMEADV_REQUEST_RECONVERSION_STRING:
    {
      if( user_data_ptr ){
        std::unique_lock<decltype(user_data_ptr->mutex)> lock{ user_data_ptr->mutex };
        if( user_data_ptr->signal_window ){
          return SendMessage( user_data_ptr->signal_window , WM_W32_IMEADV_REQUEST_RECONVERSION_STRING, 0 ,0 );
        }
      }
      return 0;
    }
  case WM_W32_IMEADV_REQUEST_DOCUMENTFEED_STRING:
    {
      if( user_data_ptr ){
        std::unique_lock<decltype(user_data_ptr->mutex)> lock{ user_data_ptr->mutex };
        if( user_data_ptr->signal_window ){
          return SendMessage( user_data_ptr->signal_window , WM_W32_IMEADV_REQUEST_DOCUMENTFEED_STRING, 0 ,0 );
        }
      }
      return 0;
    }
  default:
    break;
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
  std::unique_lock<decltype( user_data.mutex )> lock( user_data.mutex );
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
  std::unique_lock<decltype( user_data.mutex )> lock( user_data.mutex );
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

/* get communication Window Handle without lock */
HWND& w32_imeadv::implements::get_communication_HWND_impl()
{
  OutputDebugStringA( user_data.communication_window_handle ?
                      "user_data.communication_window_handle have":
                      "user_data.communication_window_handle nil" );
  return user_data.communication_window_handle;
}

/* get communication Window Handle with lock  */
HWND w32_imeadv::get_communication_HWND()
{
  std::unique_lock<decltype( user_data.mutex )> lock( user_data.mutex );
  return w32_imeadv::implements::get_communication_HWND_impl();
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
               reinterpret_cast<WPARAM>(HWND( implements::get_communication_HWND_impl() )),
               static_cast<LPARAM>( dwRefData ) );
  return FALSE;
}

BOOL 
w32_imeadv::set_openstatus( HWND hWnd , BOOL status )
{
  if( status ){
    PostMessage( hWnd , WM_W32_IMEADV_OPENSTATUS_OPEN , 0 , 0 );
  }else{
    PostMessage( hWnd , WM_W32_IMEADV_OPENSTATUS_CLOSE , 0 , 0 );
  }
  return TRUE;
}
