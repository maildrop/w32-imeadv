#include <tchar.h>
#include <windows.h>
#include <commctrl.h>
#include <mutex>
#include <cassert>
#include "w32-imeadv.h"


// now implementation 
namespace w32_imeadv {
  BOOL initialize();
};

BOOL w32_imeadv::initialize()
{

  return TRUE;
}

BOOL w32_imeadv::subclassify_hwnd( HWND hWnd , DWORD_PTR dwRefData)
{
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
              if( msg->hwnd )
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
  PostMessage( hWnd , WM_W32_IMEADV_SUBCLASSIFY , 0 , static_cast<LPARAM>( dwRefData ) );
  return FALSE;
}
