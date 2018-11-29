/* 
   わかっていたけどすげー難しい。

   emacs_env_25 のメンバーは、emacs_env* つまりemacs_env_26 を引数にとるので、
   本当にそれでいいのか？
*/

#include <tchar.h>
#include <windows.h>

#include <iostream>
#include <sstream>
#include <string>

#include <type_traits>
#include <new>
#include <memory>
#include <array>
#include <utility>
#include <algorithm>

#include <cassert>

#include "w32-imeadv.h"
#include "emacs-module.h"
#include "w32-imeadv-on-lispy-thread.h"

#define MODULE_NAME "w32-imeadv"

// なるほど UTF-8 で値がくるので注意する必要があると。
extern "C"{
  int plugin_is_GPL_compatible = 1;
};

#if 0
static BOOL filesystem_u8_to_wcs(const std::string &u8str, std::wstring &dst)
{
  int wclen = MultiByteToWideChar( CP_UTF8 , MB_ERR_INVALID_CHARS ,
                                   u8str.data() , static_cast<int>(u8str.size()) , nullptr ,0 );
  if(! wclen ){
    DWORD const lastError = GetLastError();
    if( ERROR_NO_UNICODE_TRANSLATION == lastError ){
      OutputDebugString( "ERROR_NO_UNICODE_TRANSLATION\n" );
    }
    return FALSE;
  }else{
    std::unique_ptr<wchar_t[]> buf{ new (std::nothrow) wchar_t[wclen]{} };
    if( static_cast<bool>( buf ) ){
      if( 0 < MultiByteToWideChar( CP_UTF8 , MB_ERR_INVALID_CHARS ,
                                   u8str.data() , static_cast<int>(u8str.size()) ,
                                   buf.get() ,  wclen ) ){
        dst = std::wstring(buf.get(),wclen);
        return TRUE;
      }
    }
  }
  return FALSE;
}
#endif


static BOOL filesystem_wcs_to_u8(const std::wstring &str, std::string &dst )
{
  int mblen = WideCharToMultiByte( CP_UTF8 , 0 ,
                                   str.data() , static_cast<int>( str.size() ) , nullptr , 0 ,
                                   nullptr , nullptr);
  if(! mblen ){
    return FALSE;
  }else{
    std::unique_ptr<char[]> buf{ new (std::nothrow) char[mblen]{} };
    if( static_cast<bool>( buf ) ){
      if( 0 < WideCharToMultiByte( CP_UTF8 , 0 ,
                                   str.data() , static_cast<int>( str.size() ) , buf.get() , mblen ,
                                   nullptr , nullptr) ){
        dst = std::string(buf.get(),mblen);
        return TRUE;
      }
    }
  }
  return FALSE;
}

template<typename emacs_env_t>
static inline void
message( emacs_env_t* env , const std::string& text )
{
  std::array<emacs_value , 1> args = { env->make_string( env, text.c_str() ,  text.size() ) };
  env->funcall( env , env->intern( env, u8"message" ), args.size(),  args.data() );
  return;
}

template<typename emacs_env_t>
static inline void
fset( emacs_env_t* env , emacs_value symbol, emacs_value function )
{
  std::array<emacs_value , 2> args = {symbol,function};
  env->funcall( env , env->intern( env , u8"fset" ) , args.size(),  args.data() );
  return;
}

template<typename emacs_env_t>
static inline emacs_value 
my_funcall( emacs_env_t *env , const char* proc_name )
{
  emacs_value proc_symbol = env->intern( env, proc_name );
  std::array< emacs_value  , 1 > fboundup_args = {proc_symbol};
  emacs_value fboundup_proc_value =
    env->funcall( env, env->intern(env, u8"fboundp" ),
                  fboundup_args.size() , fboundup_args.data() );
  if( env->is_not_nil( env, fboundup_proc_value ) ){
    return env->funcall( env, proc_symbol , 0 ,nullptr );
  }
  return env->intern( env , u8"nil" );
}

template<typename emacs_env_t, typename ... Args>
static inline emacs_value 
my_funcall( emacs_env_t *env , const char* proc_name , Args ... args )
{
  emacs_value proc_symbol = env->intern( env, proc_name );
  std::array< emacs_value  , 1 > fboundup_args = {proc_symbol};
  emacs_value fboundup_proc_value =
    env->funcall( env, env->intern(env, u8"fboundp" ),
                  fboundup_args.size() , fboundup_args.data() );
  if( env->is_not_nil( env, fboundup_proc_value ) ){
    std::array< emacs_value , sizeof...( args ) > proc_args = { args... };
    return env->funcall( env, proc_symbol , proc_args.size() , proc_args.data() );
  }
  return env->intern( env , u8"nil" );
}

template<typename emacs_env_t>
static emacs_value
Fw32_imeadv_initialize( emacs_env *env , ptrdiff_t nargs , emacs_value args[] , void *data ) EMACS_NOEXCEPT
{
  if( w32_imeadv::initialize() ){
    return env->intern(env, u8"t");
  }else{
    return env->intern(env, u8"nil");
  }
};

template<typename emacs_env_t>
static emacs_value
Fw32_imeadv_finalize( emacs_env* env , ptrdiff_t nargs , emacs_value args[] , void *data ) EMACS_NOEXCEPT
{
  w32_imeadv::finalize();
  message( env, std::string( u8"w32-imeadv-finalize") );
  return env->intern(env, u8"t");
}

template<typename emacs_env_t>
static emacs_value
Fw32_imeadv_get_communication_hwnd( emacs_env* env, ptrdiff_t nargs , emacs_value args[] , void *data ) EMACS_NOEXCEPT
{
  emacs_value return_value;
  HWND hwnd = w32_imeadv::get_communication_HWND();
  if( hwnd ){
    return_value = env->make_integer( env , reinterpret_cast<intmax_t>(hwnd) );
  }else{
    return_value = env->intern(env, u8"nil" );
  }
  return return_value;
}

template<typename emacs_env_t>
static emacs_value
Fw32_imeadv_inject_control( emacs_env* env , ptrdiff_t nargs , emacs_value args[] , void *data ) EMACS_NOEXCEPT
{
  if( nargs != 1 ){
    return env->intern( env, u8"nil" );
  }
  auto window_id = env->extract_integer( env,  args[0] );
  HWND hWnd = reinterpret_cast<HWND>( window_id );
  if( IsWindow( hWnd ) ){
    if( w32_imeadv::subclassify_hwnd( hWnd , 0 ) )
      return env->intern(env , u8"t");
    else
      return env->intern(env , u8"nil");
  }else{
    return env->intern(env , u8"nil");
  }
}

template<typename emacs_env_t>
static emacs_value
Fw32_imeadv__default_message_input_handler ( emacs_env* env ,
                                             ptrdiff_t nargs , emacs_value args[] ,
                                             void *data ) EMACS_NOEXCEPT
{
  std::stringstream out;
  if( nargs == 2 ){
    ptrdiff_t size{0};
    env->copy_string_contents( env, args[1], NULL , &size );
    if( 0 < size ){
      std::unique_ptr< char[] > buffer{ new char[size] };
      env->copy_string_contents( env, args[1], buffer.get() , &size ) ;
      for( ptrdiff_t i = 0; i < size ; ++i ){
        switch( buffer[i] ){
        case '\0':
          goto end_of_loop;
        case '*': 
          break;
        case '0':
          my_funcall( env , u8"w32-imeadv--notify-openstatus-close" );
          break;
        case '1':
          my_funcall( env , u8"w32-imeadv--notify-openstatus-open" );
          break;
        case 'F': // Request Composition Font
          OutputDebugStringA( "dispatch font setting\n");
          // ===========================================
          // TODO いまここ
          // ===========================================
          my_funcall( env , u8"w32-imeadv--notify-composition-font" );
          SendMessage( w32_imeadv::get_communication_HWND() ,
                       WM_W32_IMEADV_NOTIFY_COMPOSITION_FONT ,
                       0 , 0 );
          
          break;
        case 'R': // Reconversion
          OutputDebugStringA(" dispatch reconversion string\n");
          break;
        case 'D': // Document Feed
          OutputDebugStringA(" dispatch documentfeed string\n");
          break;
        default:
          break;
        }
      }
    end_of_loop:
      ;
      out << buffer.get() ;
    }
    return env->intern( env , u8"t" );
  }else{
    return env->intern( env , u8"nil" );
  }
}

template<typename emacs_env_t>
static emacs_value
Fw32_imeadv_set_openstatus_open( emacs_env* env ,
                                 ptrdiff_t nargs , emacs_value args[] ,
                                 void *data ) EMACS_NOEXCEPT
{
  if( 1 != nargs )
    return env->intern( env, u8"nil" );
  
  HWND hWnd = reinterpret_cast<HWND>(env->extract_integer( env,  args[0] ));
  if( IsWindow( hWnd ) ){
    w32_imeadv::set_openstatus( hWnd , TRUE );
    return env->intern( env, u8"t" );
  }
  return env->intern( env, u8"nil" );
}

template<typename emacs_env_t>
static emacs_value
Fw32_imeadv_set_openstatus_close( emacs_env* env,
                                  ptrdiff_t nargs , emacs_value args[] ,
                                  void *data ) EMACS_NOEXCEPT
{

  if( 1 != nargs )
    return env->intern( env , u8"nil" );
    
  HWND hWnd = reinterpret_cast<HWND>(env->extract_integer( env,  args[0] ));
  if( IsWindow( hWnd ) ){
    w32_imeadv::set_openstatus( hWnd , FALSE );
    return env->intern( env, u8"t" );
  }
  return env->intern( env, u8"nil" );
}

template<typename emacs_env_t>
static emacs_value
Fw32_imeadv__notify_openstatus_open( emacs_env* env,
                                    ptrdiff_t nargs , emacs_value[],
                                    void *data ) EMACS_NOEXCEPT
{
  if( 0 == nargs )
    {
      message( env , std::string( u8"Open" ));
      return env->intern( env, u8"t" );
    }
  return env->intern( env , u8"nil" );
}

template<typename emacs_env_t>
static emacs_value
Fw32_imeadv__notify_openstatus_close( emacs_env* env,
                                      ptrdiff_t nargs , emacs_value[],
                                      void *)
{
  if( 0 == nargs )
    {
      message( env, std::string( u8"Close" ));
      return env->intern( env , u8"t" );
    }
  return env->intern( env, u8"nil" );
}

template<typename eamcs_env_t>
static emacs_value
Fw32_imeadv__get_module_filename( emacs_env *env ,
                                  ptrdiff_t nargs , emacs_value[] ,
                                  void *) EMACS_NOEXCEPT
{
  HMODULE module_handle = GetModuleHandle(MODULE_NAME);
  if( module_handle ){
    std::unique_ptr<wchar_t[]> buf{ new (std::nothrow) wchar_t[MAX_PATH]{} };
    if( buf ){
      GetModuleFileNameW( module_handle , buf.get() , MAX_PATH );
      // 問題は、ココは、UTF-8 にしないといけない
      // TODO :
      std::string path{};
      if( filesystem_wcs_to_u8( std::wstring( buf.get() ), path ) ){
        return env->make_string( env , path.data() , path.length() );
      }
    }
  }
  
  return env->intern( env , u8"nil" );
}

template<typename emacs_env_t>
static inline int emacs_module_init_impl( emacs_env_t* env ) noexcept
{
  assert( env );
  fset( env,
        env->intern( env , u8"w32-imeadv--get-module-filename" ),
        (env->make_function( env, 0, 0 , Fw32_imeadv__get_module_filename<emacs_env_t>,
                             u8"get module filename" , NULL )));
  fset( env,
        env->intern( env , u8"w32-imeadv-initialize" ), 
        (env->make_function( env, 0 ,1 , Fw32_imeadv_initialize<emacs_env_t> ,
                             u8"initialize w32-imeadv" , NULL )) );
  fset( env,
        env->intern( env , u8"w32-imeadv-finalize" ),
        (env->make_function( env, 0 ,0 , Fw32_imeadv_finalize<emacs_env_t> ,
                             u8"finalize w32-imeadv" , NULL )));
  fset( env,
        env->intern( env , u8"w32-imeadv-inject-control" ),
        (env->make_function( env ,1 ,1 , Fw32_imeadv_inject_control<emacs_env_t>,
                             u8"inject window"
                             "ウィンドウをサブクラス化して、IMM32の制御を行います", NULL )));
  fset( env,
        env->intern( env , u8"w32-imeadv-get-communication-hwnd" ),
        (env->make_function( env , 0 , 0 , Fw32_imeadv_get_communication_hwnd<emacs_env_t>,
                             u8"get comminication window handle ", NULL )));

  fset( env,
        env->intern( env , u8"w32-imeadv--defualt-message-input-handler"),
        (env->make_function( env , 2, 2 , Fw32_imeadv__default_message_input_handler<emacs_env_t>,
                             u8"signal input handler" , NULL )));

  fset( env,
        env->intern( env , u8"w32-imeadv-set-openstatus-open" ),
        (env->make_function( env, 1, 1 , Fw32_imeadv_set_openstatus_open<emacs_env_t> ,
                             u8"open IME require window-id\n"
                             "IME を開きます。引数には、フレームのHWNDが必要です。\n"
                             "HWND は frame-parameterの'window-id がそれに対応します\n"
                             "\n"
                             "例えば現在のフレームのIMEを開くためには、\n"
                             "(w32-imeadv-set-openstatus-open (string-to-number (frame-parameter (selected-frame)'window-id)) )\n"
                             "とします。\n", NULL )));
  fset( env,
        env->intern( env , u8"w32-imeadv-set-openstatus-close" ),
        (env->make_function( env, 1, 1, Fw32_imeadv_set_openstatus_close<emacs_env_t> ,
                             u8"close IME require window-id\n"
                             "IMEを閉じます引数にはフレームのHWNDが必要です。\n"
                             "HWND は frame-parameterの'window-id がそれに対応します\n"
                             "\n"
                             "例えば現在のフレームのIMEを閉じるためには、\n"
                             "(w32-imeadv-set-openstatus-close (string-to-number (frame-parameter (selected-frame)'window-id)) )\n"
                             "とします\n", NULL )));
  fset( env,
        env->intern( env , u8"w32-imeadv--notify-openstatus-open"),
        (env->make_function( env, 0 ,0 ,Fw32_imeadv__notify_openstatus_open<emacs_env_t>,
                             u8"IMEが開かれたときに呼び出される関数です。",NULL )));
  fset( env,
        env->intern( env , u8"w32-imeadv--notify-openstatus-close" ),
        (env->make_function( env, 0, 0 ,Fw32_imeadv__notify_openstatus_close<emacs_env_t>,
                             u8"IMEが閉じられた時に呼び出される関数です。",NULL )));
  
  std::array<emacs_value,1> provide_args =  { env->intern( env , u8"w32-imeadv" ) };
  env->funcall( env,
                env->intern( env , u8"provide" ) ,
                provide_args.size() ,
                provide_args.data() );
  return 0;
}

int emacs_module_init (struct emacs_runtime *ert) EMACS_NOEXCEPT
{
  if( 0 < ert->size && 
      sizeof( emacs_runtime ) <= static_cast<size_t>(ert->size) )
    {
      emacs_env* env = ert->get_environment( ert );
      if( 0 < env->size &&
          sizeof( emacs_env ) <= static_cast<size_t>(env->size) ){
        return emacs_module_init_impl( reinterpret_cast<emacs_env*>(ert->get_environment( ert )) );
      }else{
        OutputDebugString( "emacs_env size is invalid\n");
      }
    }
  else
    OutputDebugString( "emacs_runtime size is invalid\n");
  return 0;
}
