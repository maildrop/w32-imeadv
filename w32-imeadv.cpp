/* 
   わかっていたけどすげー難しい。

   emacs_env_25 のメンバーは、emacs_env* つまりemacs_env_26 を引数にとるので、
   本当にそれでいいのか？
 */

#include <tchar.h>
#include <windows.h>

#include <type_traits>
#include <string>
#include <array>
#include <utility>
#include <algorithm>

#include <cassert>

#include "emacs-module.h"
#include "w32-imeadv.h"

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
  message( env, std::string("w32-imadv"));
  return env->intern(env,"t");
};

template<typename emacs_env_t>
static inline int emacs_module_init_impl( emacs_env_t* env ) noexcept
{
  assert( env );
  fset( env,
        env->intern( env , "w32-imeadv-initialize" ), 
        (env->make_function( env, 0 ,1 , Fw32_imeadv_initialize<emacs_env_t> , "initialize w32-imeadv" , NULL )) );
  
  std::array<emacs_value,1> provide_args =  { env->intern( env , "w32-imeadv" ) };
  env->funcall( env,
                env->intern( env , "provide" ) ,
                provide_args.size() ,
                provide_args.data() );
  OutputDebugStringA( "emacs_module_init_impl\n" );
  return 0;
}

int emacs_module_init (struct emacs_runtime *ert) EMACS_NOEXCEPT
{
  OutputDebugStringA( "emacs_module_init\n" );
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
    {
      OutputDebugString( "emacs_runtime size is invalid\n");
    }
  return 0;
}
