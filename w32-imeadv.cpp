/* 
   わかっていたけどすげー難しい。

   emacs_env_25 のメンバーは、emacs_env* つまりemacs_env_26 を引数にとるので、
   本当にそれでいいのか？
*/

#include <tchar.h>
#include <windows.h>

#include <iostream>
#include <sstream>
#include <type_traits>
#include <string>
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
static emacs_value
Fw32_imeadv_initialize( emacs_env *env , ptrdiff_t nargs , emacs_value args[] , void *data ) EMACS_NOEXCEPT
{
  if( w32_imeadv::initialize() ){
    return env->intern(env,"t");
  }else{
    return env->intern(env,"nil");
  }
};

template<typename emacs_env_t>
static emacs_value
Fw32_imeadv_finalize( emacs_env* env , ptrdiff_t nargs , emacs_value args[] , void *data ) EMACS_NOEXCEPT
{
  w32_imeadv::finalize();
  message( env, std::string("w32-imeadv-finalize") );
  return env->intern(env,"t");
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
    return_value = env->intern(env, "nil" );
  }
  return return_value;
}

template<typename emacs_env_t>
static emacs_value
Fw32_imeadv_inject_control( emacs_env* env , ptrdiff_t nargs , emacs_value args[] , void *data ) EMACS_NOEXCEPT
{
  if( nargs != 1 ){
    return env->intern( env, "nil" );
  }
  auto window_id = env->extract_integer( env,  args[0] );
  HWND hWnd = reinterpret_cast<HWND>( window_id );
  if( IsWindow( hWnd ) ){
    if( w32_imeadv::subclassify_hwnd( hWnd , 0 ) )
      return env->intern(env,"t");
    else
      return env->intern(env,"nil");
  }else{
    return env->intern(env,"nil");
  }
}

template<typename emacs_env_t>
static emacs_value
Fw32_imeadv__defualt_message_input_handler ( emacs_env* env ,
                                             ptrdiff_t nargs , emacs_value args[] ,
                                             void *data ) EMACS_NOEXCEPT
{
  std::stringstream out;
  if( nargs == 2 ){
    out << "hello world ";
    ptrdiff_t size{0};
    env->copy_string_contents( env, args[1], NULL , &size );
    if( 0 < size ){
      std::unique_ptr< char[] > buffer{ new char[size] };
      env->copy_string_contents( env, args[1], buffer.get() , &size ) ;
      out << buffer.get() ;
    }
    message( env , out.str() );
    return env->intern( env , "t" );
  }else{
    return env->intern( env , "nil" );
  }
}

template<typename emacs_env_t>
static inline int emacs_module_init_impl( emacs_env_t* env ) noexcept
{
  assert( env );
  fset( env,
        env->intern( env , "w32-imeadv-initialize" ), 
        (env->make_function( env, 0 ,1 , Fw32_imeadv_initialize<emacs_env_t> , "initialize w32-imeadv" , NULL )) );
  fset( env,
        env->intern( env , "w32-imeadv-finalize" ),
        (env->make_function( env, 0 ,0 , Fw32_imeadv_finalize<emacs_env_t> , "finalize w32-imeadv" , NULL )));
  fset( env,
        env->intern( env , "w32-imeadv-inject-control" ),
        (env->make_function( env ,1 ,1 , Fw32_imeadv_inject_control<emacs_env_t>, "inject window" , NULL )));
  fset( env,
        env->intern( env , "w32-imeadv-get-communication-hwnd" ),
        (env->make_function( env , 0 , 0 , Fw32_imeadv_get_communication_hwnd<emacs_env_t>, "get comminication window handle ", NULL )));

  fset( env,
        env->intern( env , "w32-imeadv--defualt-message-input-handler"),
        (env->make_function( env , 2, 2 , Fw32_imeadv__defualt_message_input_handler<emacs_env_t>,
                             "signal input handler" , NULL )));
  
  std::array<emacs_value,1> provide_args =  { env->intern( env , "w32-imeadv" ) };
  env->funcall( env,
                env->intern( env , "provide" ) ,
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
