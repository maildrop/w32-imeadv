/* 
   わかっていたけどすげー難しい。
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
  env->funcall( env , env->intern( env, u8"message" ), std::extent<decltype( args )>::value , args.data() );
  return;
}

template<typename emacs_env_t>
static inline void
fset( emacs_env_t* env , emacs_value symbol, emacs_value function )
{
  std::array<emacs_value , 2> args = {symbol,function};
  env->funcall( env , env->intern( env , u8"fset" ) , std::extent<decltype(args)>::value  , args.data() );
  return;
}

template<typename emacs_env_t>
static inline int emacs_module_init_impl( emacs_env_t* env ) noexcept
{
  assert( env );
  return 0;
}

int emacs_module_init (struct emacs_runtime *ert) EMACS_NOEXCEPT
{
  switch( ert->size ){
  case sizeof( struct emacs_env_25 ):
    return emacs_module_init_impl( reinterpret_cast<emacs_env_25*>(ert->get_environment( ert )) );
    break;
  default:
    if( ert->size > 0 )
      if( sizeof( emacs_env_26 ) <= static_cast<size_t>(ert->size) ){
          return emacs_module_init_impl( ert->get_environment( ert ) );
      }
    break;
  }
  return 0;
}
