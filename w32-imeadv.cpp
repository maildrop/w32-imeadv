/* 
   Eamcs Dynamic Module のインターフェースを取り持つコード

   わかっていたけどすげー難しい。

   TODO:: 未実装の Lisp 関数
   - "w32-imeadv--notify-documentfeed-string"
   emacs_env_25 のメンバーは、emacs_env* つまりemacs_env_26 を引数にとるのであるが本当にそれでいいのか？
*/

#include <tchar.h>
#include <windows.h>

#include <iostream>
#include <ostream>
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

extern "C"{
  int plugin_is_GPL_compatible = 1;
};

static BOOL
filesystem_u8_to_wcs(const std::string &src_u8str, std::wstring &dst)
{
  int wclen = MultiByteToWideChar( CP_UTF8 , MB_ERR_INVALID_CHARS ,
                                   src_u8str.data() , static_cast<int>(src_u8str.size()) , nullptr ,0 );
  if(! wclen ){
    DWORD const lastError = GetLastError();
    if( ERROR_NO_UNICODE_TRANSLATION == lastError ){
      OutputDebugString( TEXT("ERROR_NO_UNICODE_TRANSLATION\n") );
    }
    return FALSE;
  }else{
    std::unique_ptr<wchar_t[]> buf{ new (std::nothrow) wchar_t[wclen]{} };
    if( static_cast<bool>( buf ) ){
      if( 0 < MultiByteToWideChar( CP_UTF8 , MB_ERR_INVALID_CHARS ,
                                   src_u8str.data() , static_cast<int>(src_u8str.size()) ,
                                   buf.get() ,  wclen ) ){
        dst = std::wstring(buf.get(),wclen);
        return TRUE;
      }
    }
  }
  return FALSE;
}

static BOOL
filesystem_wcs_to_u8(const std::wstring &str, std::string &dst_u8str )
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
        dst_u8str = std::string(buf.get(),mblen);
        return TRUE;
      }
    }
  }
  return FALSE;
}

template<typename emacs_env_t>
static inline emacs_value
message_utf8( emacs_env_t* env , const std::string& utf8_text ) 
{
  std::array<emacs_value , 1> args = { env->make_string( env, utf8_text.c_str() ,  utf8_text.size() ) };
  return env->funcall( env , env->intern( env, u8"message" ), args.size(),  args.data() );
}

template<typename emacs_env_t>
static inline emacs_value
my_make_value( emacs_env_t* env, const std::string& utf8_text )
{
  return env->make_string( env, utf8_text.c_str() , utf8_text.size() );
}

template<typename emacs_env_t>
static inline emacs_value
my_make_value( emacs_env_t* env, const int& integer )
{
  return env->make_integer( env , integer );
}

template<typename emacs_env_t>
static inline emacs_value
my_make_value( emacs_env_t *, emacs_value& value )
{
  return value;
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
std::wstring 
my_copy_string_contents( emacs_env_t env, emacs_value value )
{
  ptrdiff_t size{0}; // contains null terminate character. 
  if( env->copy_string_contents( env, value , NULL , &size ) ){
    if( 0 < size ){
      std::unique_ptr< char[] > buffer{ new (std::nothrow) char[size]{} };
      if( static_cast<bool>( buffer ) ){
        if( env->copy_string_contents( env , value , buffer.get() , &size ) ){
          std::string const utf8_str = std::string( buffer.get() );
          std::wstring result{};
          if( filesystem_u8_to_wcs( utf8_str , result ) ){
            return result;
          }
        }
      }
    }
  }
  return std::wstring{};
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

template<typename emacs_env_t, typename value_type>
static inline emacs_value
my_defvar( emacs_env_t *env , const std::string& symbol_name , value_type value , const std::string& document)
{
  emacs_value eval_string =
    my_funcall(env,u8"format", my_make_value(env, std::string(u8"(defvar %s %S %S)")),
               my_make_value( env, symbol_name ), my_make_value( env,  value ), my_make_value( env, document ));
  return my_funcall( env , u8"eval",
                     my_funcall( env, u8"car" ,
                                 my_funcall( env , u8"read-from-string" , eval_string )));
}

#if !defined( NDEBUG )
/* リリース時までには、ちゃんと書く関数に対して割り当てる */
template<typename emacs_env_t>
static emacs_value
Fw32_imeadv__not_implemented( emacs_env *env,
                              ptrdiff_t nargs , emacs_value[] ,
                              void* ) EMACS_NOEXCEPT
{
  message_utf8( env , std::string( u8"not implemented" ) );
  return env->intern( env, u8"nil" );
}
#endif /* !defined( NDEBUG ) */

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
      std::string path{};
      if( filesystem_wcs_to_u8( std::wstring( buf.get() ), path ) ){
        return env->make_string( env , path.data() , path.length() );
      }
    }
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
  return env->intern(env, u8"t");
}

template<typename emacs_env_t>
static emacs_value
Fw32_imeadv__get_communication_hwnd( emacs_env* env, ptrdiff_t nargs , emacs_value args[] , void *data ) EMACS_NOEXCEPT
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
          if( !env->is_not_nil( env, my_funcall( env , u8"w32-imeadv--notify-composition-font" ) ) ){
            const HWND hWnd = w32_imeadv::get_communication_HWND();
            if( hWnd && IsWindow( hWnd )){
              SendMessage( hWnd,  WM_W32_IMEADV_NOTIFY_COMPOSITION_FONT , 0 , 0 );
            }else{
              // may be ui thread dead locked,
              message_utf8( env ,
                            std::string{u8"w32_imeadv::get_communication_HWND() return NULL. "
                                "You should wait 5 sec. (WM_W32_IMEADV_NOTIFY_COMPOSITION_FONT)" });
            }
          }
          break;
        case 'R': // Reconversion
          if( !env->is_not_nil( env , my_funcall( env , u8"w32-imeadv--notify-reconversion-string") )){
            const HWND hWnd = w32_imeadv::get_communication_HWND() ;
            if( hWnd && IsWindow( hWnd )){
              SendMessage( hWnd , WM_W32_IMEADV_NOTIFY_RECONVERSION_STRING , 0 , 0 );
            }else{
              message_utf8( env,
                            std::string{u8"w32_imeadv::get_communication_HWND() return NULL. "
                                "You should wait 5 sec. (WM_W32_IMEADV_NOTIFY_RECONVERSION_STRING)" });
            }
          }
          break;
        case 'D': // Document Feed
          if( !env->is_not_nil( env , my_funcall( env , u8"w32-imeadv--notify-documentfeed-string") )){
            const HWND hWnd = w32_imeadv::get_communication_HWND() ;
            if( hWnd && IsWindow( hWnd ) ){
              DebugOutputStatic( "faillback WM_W32_IMEADV_NOTIFY_DOCUMENTFEED_STRING" );
              SendMessage( hWnd , WM_W32_IMEADV_NOTIFY_DOCUMENTFEED_STRING , 0, 0 );
            }else{
              message_utf8( env,
                            std::string{u8"w32_imeadv::get_communication_HWND() return NULL. "
                                "You should wait 5 sec. (WM_W32_IMEADV_NOTIFY_DOCUMENTFEED_STRING)" });
            }
          }
          break;
        case 'b':
          {
            my_funcall( env, u8"backward-char" );
            const HWND hWnd = w32_imeadv::get_communication_HWND();
            if( hWnd && IsWindow( hWnd ) ){
              SendMessage( hWnd, WM_W32_IMEADV_NOTIFY_BACKWARD_CHAR , 0 , 0 );
            }
          }
          break;
        case 'd':
          {
            my_funcall( env, u8"delete-char", env->make_integer( env, 1 ) , env->intern( env, u8"nil" )  );
            const HWND hWnd = w32_imeadv::get_communication_HWND();
            if( hWnd && IsWindow( hWnd )){
              SendMessage( hWnd, WM_W32_IMEADV_NOTIFY_DELETE_CHAR , 0 , 0 );
            }
          }
          break;
        case '!':
          DebugOutputStatic(" characteristic Events" );
          {
            HWND const hWnd = w32_imeadv::get_communication_HWND();
            if( hWnd && IsWindow( hWnd ) ){
              
            }
          }
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
      // run-hooks したい
      message_utf8( env , std::string( u8"Open" ));
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
      message_utf8( env, std::string( u8"Close" ));
      return env->intern( env , u8"t" );
    }
  return env->intern( env, u8"nil" );
}

template<typename emacs_env_t>
static emacs_value
Fw32_imeadv_advertise_ime_composition_font( emacs_env *env,
                                            ptrdiff_t nargs , emacs_value value[],
                                            void *data) EMACS_NOEXCEPT
{
  WPARAM wParam = 0;
  switch( nargs ){
  case 2:
    wParam = (WPARAM)env->extract_integer( env, value[1] );
    if( !wParam || !IsWindow( (HWND)wParam ))
      return env->intern(env,"nil");

    /* fall Through */
  case 1:
    do{
      if( env->is_not_nil( env, value[0] ) ){
        w32_imeadv_composition_font_configure font_configure = {};
        (void)(font_configure);
        emacs_value family = my_funcall( env , "plist-get" , value[0] , env->intern(env,":family") );
        if( env->is_not_nil( env, family ) ){
          {
            std::wstring font_family = my_copy_string_contents( env , family );
            for( ptrdiff_t i = 0; i < LF_FACESIZE ; ++i ){
              std::for_each( std::begin( font_family ), std::end( font_family ),
                             [&]( const wchar_t &c ){
                               if( (LF_FACESIZE-1) <= i  ) // The last one is terminate character.
                                 return;
                               font_configure.lfFaceName[i++] = c;
                             });
              for( ; i < LF_FACESIZE ; ++i ){
                font_configure.lfFaceName[i] = L'\0'; // fill by terminate character
              }
            }
            font_configure.enable_bits |= W32_IMEADV_FONT_CONFIGURE_BIT_FACENAME;
          }
        }

        emacs_value height = my_funcall( env , "plist-get" , value[0] , env->intern(env,":height") );
        if( env->is_not_nil( env, height ) ){
          auto font_height = env->extract_integer( env, height );
          font_configure.font_height = static_cast<decltype( font_configure.font_height )>( font_height );
          font_configure.enable_bits |= W32_IMEADV_FONT_CONFIGURE_BIT_FONTHEIGHT;
        }

        if( SendMessage(w32_imeadv::get_communication_HWND() ,
                        WM_W32_IMEADV_NOTIFY_COMPOSITION_FONT ,
                        wParam , reinterpret_cast<LPARAM>(&font_configure) ) )
          return env->intern( env , u8"t" );
        else
          return env->intern( env , u8"nil" );
      }
    }while( false );
    break;
  default:
    break;
  }
  if( 1 == nargs )
    SendMessage( w32_imeadv::get_communication_HWND() ,
                 WM_W32_IMEADV_NOTIFY_COMPOSITION_FONT ,
                 0 , 0 );
  return env->intern( env , u8"nil" );
}

template<typename emacs_env_t, UINT resultCode>
static emacs_value
Fw32_imeadv__notify_reconversion_string( emacs_env *env,
                                         ptrdiff_t nargs , emacs_value value[],
                                         void *data) EMACS_NOEXCEPT
{
  emacs_value pos = my_funcall( env , u8"point" );
  emacs_value begin = my_funcall( env , u8"line-beginning-position");
  emacs_value end = my_funcall( env , u8"line-end-position" );

  /* 確認用 */
  auto first_half_num = env->extract_integer( env,my_funcall( env , u8"-" , pos , begin ) );
  auto later_half_num = env->extract_integer( env, my_funcall( env, u8"-" , end , pos ) );

  
  emacs_value first_half = my_funcall( env , u8"buffer-substring-no-properties" ,
                                       begin , pos );
  emacs_value later_half = my_funcall( env , u8"buffer-substring-no-properties",
                                       pos ,  end );
  std::wstring first_half_str =
    ( env->is_not_nil( env,first_half ) ) ? my_copy_string_contents( env , first_half ) : std::wstring{};
  std::wstring later_half_str =
    ( env->is_not_nil( env, later_half )) ? my_copy_string_contents( env , later_half ) : std::wstring{};

  using imeadv::NotifyReconversionString;
  NotifyReconversionString nrs = {};
  {
    nrs.pos = env->extract_integer( env , pos );
    nrs.begin = env->extract_integer( env , begin );
    nrs.end = env->extract_integer( env , end );
    nrs.first_half_num = first_half_num;
    nrs.later_half_num = later_half_num;
    nrs.first_half = first_half_str;
    nrs.later_half = later_half_str;
  }
  
#if !defined( NDEBUG )
  // ちゃんとサロゲートペアを処理しているかどうかのテスト
  if(first_half_num != static_cast<decltype(first_half_num)>(first_half_str.length()) ){
    std::wstringstream out{};
    out << "first half contain surrogate-pair" << " "
        << first_half_num << " "
        << static_cast<decltype(first_half_num)>(first_half_str.length()) 
        << DEBUG_STRING(L" ") << std::endl;
    OutputDebugStringW( out.str().c_str() );
  }
  if( later_half_num != static_cast<decltype(later_half_num)>(later_half_str.length()) ){
    std::wstringstream out{};
    out << "letter half contain surrogate-pair" << " "
        << later_half_num << " "
        << static_cast<decltype(later_half_num)>(later_half_str.length() )
        << DEBUG_STRING(L" ") << std::endl;
    OutputDebugStringW( out.str().c_str() );
  }
#endif /* !defined( NDEBUG ) */
  
  {
    const HWND hWnd = w32_imeadv::get_communication_HWND();
    if( hWnd && IsWindow( hWnd ) ){
      if( SendMessage( hWnd , resultCode , 0 , reinterpret_cast<LPARAM>( &nrs ) ) ){
        return env->intern( env , u8"t" ) ;
      }
    }
  }
  return env->intern( env, u8"nil" );
}

template<typename emacs_env_t>
static inline int emacs_module_init_impl( emacs_env_t* env ) noexcept
{
  assert( env );
#if !defined( NDEBUG )
  { // これ本当は w32-imeadv-initialize の中でやった方がいいんじゃないかと思う
    intmax_t emacs_major_version = 0;
    intmax_t emacs_minor_version = 0;
    { // major version 
      emacs_value major_version_value =
        my_funcall( env , u8"symbol-value" , env->intern( env, u8"emacs-major-version") );
      if( env->is_not_nil( env , major_version_value ) ){
        emacs_major_version = env->extract_integer( env , major_version_value );
      }
    }
    { // minor version
      emacs_value minor_version_value =
        my_funcall( env , u8"symbol-value" , env->intern( env , u8"emacs-minor-version"));
      if( env->is_not_nil( env , minor_version_value ) ){
        emacs_minor_version = env->extract_integer( env , minor_version_value );
      }
    }

#if !defined( NDEBUG )
    std::stringstream out {};
    out << "(" << __FILE__ << " L." << __LINE__ << ") "
        << "w32-imeadv-system-configuration{"
        << "emacs_major_version : " << emacs_major_version << "," 
        << "emacs_minor_version : " << emacs_minor_version << "} ;" << std::endl;
    OutputDebugStringA( out.str().c_str() );
#endif /* !defined( NDEBUG ) */

  }
#endif /* !defined( NDEBUG ) */

  my_defvar( env, u8"w32-imeadv-ime-show-mode-line" ,
             env->intern( env, u8"t") ,
             u8"When t, mode line indicates IME state.");
  my_defvar( env, u8"w32-imeadv-ime-on-hook",
             env->intern( env, u8"nil"),
             u8"TODO: ");
  my_defvar( env, u8"w32-imeadv-ime-off-hook",
             env->intern( env, u8"nil"),
             u8"TODO: ");
  my_defvar( env, u8"w32-imeadv-ime-composition-font-attributes",
             env->intern( env, u8"nil"),
             u8"IME のコンポジションウィンドウで使うフォントの属性を指定する"
             "手動オーバーライドするための変数"
             "これを指定しているときには固定値になる");
  
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
        env->intern( env , u8"w32-imeadv--get-communication-hwnd" ),
        (env->make_function( env , 0 , 0 , Fw32_imeadv__get_communication_hwnd<emacs_env_t>,
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
        env->intern( env , u8"w32-imeadv-advertise-ime-composition-font"),
        (env->make_function( env , 2 ,2 ,Fw32_imeadv_advertise_ime_composition_font<emacs_env_t>,
                             u8"advertise IME composition window font\n"
                             "IMEのコンポジションウィンドウで使用するフォントを通知します。",
                             NULL )));
  fset( env,
        env->intern( env , u8"w32-imeadv-advertise-ime-composition-font-internal"),
        (env->make_function( env , 1 ,1 ,Fw32_imeadv_advertise_ime_composition_font<emacs_env_t>,
                             u8"advertise IME composition window font\n"
                             "IMEのコンポジションウィンドウで使用するフォントを通知します。これは内部で使用するようです",
                             NULL )));
  fset( env,
        env->intern( env , u8"w32-imeadv--notify-openstatus-open"),
        (env->make_function( env, 0 ,0 ,Fw32_imeadv__notify_openstatus_open<emacs_env_t>,
                             u8"IMEが開かれたときに呼び出される関数です。",NULL )));
  fset( env,
        env->intern( env , u8"w32-imeadv--notify-openstatus-close" ),
        (env->make_function( env, 0, 0 ,Fw32_imeadv__notify_openstatus_close<emacs_env_t>,
                             u8"IMEが閉じられた時に呼び出される関数です。",NULL )));

  fset( env ,
        env->intern( env , u8"w32-imeadv--notify-reconversion-string" ),
        (env->make_function( env , 0, 0 ,
                             Fw32_imeadv__notify_reconversion_string<emacs_env_t,
                             WM_W32_IMEADV_NOTIFY_RECONVERSION_STRING>,
                             u8"IMEから再変換要求がなされたときに呼び出される関数です" , NULL )));
  fset( env,
        env->intern( env , u8"w32-imeadv--notify-documentfeed-string" ),
        (env->make_function( env , 0 , 0,
                             Fw32_imeadv__notify_reconversion_string<emacs_env_t,
                             WM_W32_IMEADV_NOTIFY_DOCUMENTFEED_STRING>,
                             u8"IMEからDOCUMENT FEED がなされたときに呼び出される関数です（まだ作っていない)",NULL )));
  
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
