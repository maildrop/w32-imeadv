﻿#if !defined( W32_IMEADV_ON_LISPY_THREAD_H_HEADER_GUARD__d6fd366a_eac3_4b32_9d15_a328ec972869 )
#define W32_IMEADV_ON_LISPY_THREAD_H_HEADER_GUARD__d6fd366a_eac3_4b32_9d15_a328ec972869 1
/**

 */
#include "emacs-module.h"

#if defined( __cplusplus )

namespace w32_imeadv {
  BOOL initialize(emacs_env *);
  BOOL finalize();
  BOOL subclassify_hwnd( HWND hWnd , DWORD_PTR dwRefData);
};
#endif /* defined( __cplusplus ) */

#endif /* W32_IMEADV_ON_LISPY_THREAD_H_HEADER_GUARD__d6fd366a_eac3_4b32_9d15_a328ec972869 */
