#include <tchar.h>
#include <windows.h>
#include <imm.h>
#include <commctrl.h>

#include <type_traits>
#include <algorithm>
#include <limits>
#include <sstream>
#include <ostream>
#include <string>
#include <vector>
#include <new>
#include <memory>
#include <mutex>
#include <tuple>

#include <cstddef>
#include <cstdint>
#include <cassert>
#include <cmath>

#include "w32-imeadv.h"

template<UINT WaitMessage ,
         typename message_transporter_t,
         DWORD dwTimeOutMillSecond = 1000 /* 1000 msec * 3  timout (for safity) */> static inline BOOL 
my_wait_message( HWND hWnd , const message_transporter_t& message_transporter);

/* see w32_imm_wm_ime_startcomposition and w32_imm_wm_ime_endcomposition */
static int ignore_wm_ime_start_composition = 0; // このパラメータ プロパティに書いておく必要がある。

static LRESULT
w32_imeadv_wm_ime_startcomposition_emacs26( HWND hWnd , WPARAM wParam , LPARAM lParam );
static LRESULT
w32_imeadv_wm_ime_startcomposition_emacs27( HWND hWnd , WPARAM wParam , LPARAM lParam );

static LRESULT
w32_imeadv_wm_ime_endcomposition( HWND hWnd , WPARAM wParam , LPARAM lParam );

static LRESULT
CALLBACK w32_imeadv_ui_delete_reconversion_region( HWND hWnd, UINT message , WPARAM wParam , LPARAM lParam ,
                                                   UINT_PTR uIdSubclass, DWORD_PTR dwRefData );
static LRESULT
w32_imeadv_wm_ime_composition( HWND hWnd , WPARAM wParam , LPARAM lParam );

static LRESULT
w32_imeadv_wm_ime_notify( HWND hWnd, WPARAM wParam ,LPARAM  lParam );
static LRESULT
w32_imeadv_null( HWND hWnd , WPARAM wParam , LPARAM lParam );
static LRESULT
w32_imeadv_openstatus_open( HWND hWnd , WPARAM wParam , LPARAM lParam );
static LRESULT
w32_imeadv_openstatus_close( HWND hWnd , WPARAM wParam , LPARAM lParam );
static LRESULT
w32_imeadv_notify_reconversion_string( HWND hWnd , WPARAM wParam , LPARAM lParam );
static LRESULT
w32_imeadv_ui_perform_reconversion( HWND hWnd, WPARAM wParam , LPARAM lParam );
static LRESULT
w32_imeadv_wm_emacs_track_caret_hook( HWND hWnd , WPARAM wParam , LPARAM lParam );


template<typename CharT,typename Traits >
std::basic_ostream<CharT,Traits>&
operator<<( std::basic_ostream<CharT,Traits>& out , const RECONVERTSTRING& reconv )
{
  out << "RECONVERTSTRING{ "
      << "dwSize = " << reconv.dwSize << " , "
      << "dwVersion = " << reconv.dwVersion << " , "
      << "dwStrLen = " << reconv.dwStrLen << " , "
      << "dwStrOffset = " << reconv.dwStrOffset << " , "
      << "dwCompStrLen = " << reconv.dwCompStrLen << " , "
      << "dwTargetStrLen = " << reconv.dwTargetStrLen << " , "
      << "dwTargetStrOffset = " << reconv.dwTargetStrOffset << " , " 
      << "sizeof( RECONVERTSTRING ) = " << sizeof( RECONVERTSTRING )  << "}";
  return out;
}

template<UINT WaitMessage , typename message_transporter_t, DWORD dwTimeOutMillSecond> static inline BOOL 
my_wait_message( HWND hWnd , const message_transporter_t& message_transporter)
{
  assert( hWnd );

  if(! hWnd )
    return FALSE;
  if(! IsWindow( hWnd ) )
    return FALSE;

  assert( GetCurrentThreadId() == GetWindowThreadProcessId( hWnd, nullptr ) );
  
  SUBCLASSPROC const subclass_proc
    = [](HWND hWnd, UINT uMsg , WPARAM wParam , LPARAM lParam, UINT_PTR , DWORD_PTR dwRefData) -> LRESULT {
      if( WaitMessage == uMsg ){
        int* ptr = reinterpret_cast<int*>( dwRefData );
        ++(*ptr);
      }
      return ::DefSubclassProc( hWnd, uMsg , wParam , lParam );
    };
  
  UINT_PTR const uIdSubclass = reinterpret_cast<UINT_PTR const >( subclass_proc );

  /* not throw std::bad_alloc in this case  */
  std::unique_ptr< int > waiting_data{(new ( std::nothrow ) int{0})}; 
  if( ! static_cast<bool>( waiting_data ) ){
    return FALSE;
  }
  
  if(! ::SetWindowSubclass( hWnd , subclass_proc , uIdSubclass , reinterpret_cast<DWORD_PTR>( waiting_data.get()) ) ){
    DebugOutputStatic( " SetWindowSubclass failed" );
    return FALSE;
  }else{
    struct raii{
      HWND const hWnd;
      SUBCLASSPROC const subclass_proc;
      UINT_PTR const uIdSubclass;
      std::unique_ptr< int >& waiting_data;
      ~raii(){
        if( !::RemoveWindowSubclass( this->hWnd, this->subclass_proc , this->uIdSubclass ) ){
          // 不本意ながら、 waiting_data を detach して、状況の保全を図る
          DebugOutputStatic( "** WARNING ** RemoveWindowSubClass failed. " );
          this->waiting_data.release();
        }
      }
    } remove_window_subclass_raii = { hWnd , subclass_proc , uIdSubclass , waiting_data };

    assert( static_cast<bool>( waiting_data ) ); // 上の条件式ではねてる

    // message_transporter の中で SendMessage していると、そのSendMessage の最中にSendMessage が戻ってくる可能性がある。
    int const times = message_transporter(); ;
    // つまりこの時点 waiting_data が 0 とは限らない
    int wakeup_chance = 3;
    while( *waiting_data < times ){
      if( !wakeup_chance )
        break;
      /* 
         ここ、なぜMsgWaitForMultipleObjects では無く、MsgWaitForMutipleObjectsEx なのかは
         @See Windows via C/C++ ver4. Chaper 26. Section 4. 
         和名 Advanced Windows ver 4. pp 919 
      */
      typedef std::integral_constant<DWORD , 0> nObjects;
      DWORD const wait_result =
        MsgWaitForMultipleObjectsEx( nObjects::value  , nullptr , dwTimeOutMillSecond ,
                                     QS_SENDMESSAGE  , MWMO_INPUTAVAILABLE );
      switch( wait_result ){
      case (WAIT_OBJECT_0 + nObjects::value):
        {
          MSG msg = {};
          // Process only messages sent with SendMessage
          while( PeekMessageW( &msg , hWnd , 0 , 0 , PM_REMOVE | PM_QS_SENDMESSAGE ) ){
            if( WM_QUIT == msg.message ){
              // これは起こらない状態と思うが、今メッセージポンプの内側でWM_QUITメッセージを受け取ったので、
              // 外側のメッセージポンプにWM_QUITを伝播させる。
              PostQuitMessage( msg.wParam );
              break;
            }
            TranslateMessage( &msg );
            DispatchMessageW( &msg );
          }
          break;
        }
      case WAIT_TIMEOUT:
        {
          std::wstringstream out{};
          out << "** WARNNING ** my_wait_message TIME_OUT! "
              << "(" << *waiting_data << ")" << "wakup_change = " << wakeup_chance << " " 
              << "WaitMessage=" << WaitMessage << " "
              << __PRETTY_FUNCTION__
              << DEBUG_STRING( " " ) << std::endl;
          OutputDebugStringW( out.str().c_str() );
          if( wakeup_chance-- ){
            // バグ #18420 対応させるために、WAIT_TIMEOUT が戻ってきたら、 Lispスレッドに WM_W32_IMEADV_NULL を入れて、
            // Lisp スレッドを起こしてやる。?  
            HWND communication_window_handle = reinterpret_cast<HWND>( GetProp( hWnd , "W32_IMM32ADV_COMWIN" ));
            if( communication_window_handle ){
              // wake up lisp thread 
              SendMessage( communication_window_handle , WM_W32_IMEADV_NULL , 0 , 0 );
              continue;
            }
          }
          return FALSE;
        }
      default:
        if( (WAIT_ABANDONED_0 <= wait_result) && (wait_result < (WAIT_ABANDONED_0 + nObjects::value)) ){
          // nObjects::value = 0
#if !defined( NDEBUG )
          ::DebugBreak();
#endif /* !defined( NDEBUG ) */
        }
        return FALSE;
      }
    }
    return TRUE;
  }
}



/**
   Emacs のバグに対応するところ。
*/

/* emacs 27 からは、eamcs 本体が DefWindowProc を呼び出すので、
   w32_imeadv_wm_ime_startcomposition_emacs26 は、 DefSubclassProc を呼び出した上で
   さらに DefWindowProc を呼び出すことによって、 emacs が （意図的に） DefWindowProc を呼び出さない
   問題に対処している。
   
   emacs 27 からは emacs 本体が DefwindowProc を呼び出すので、
   w32_imeadv_wm_ime_startcomposition_emacs27 は、 DefSubClassProc を呼び出すのみである */

static LRESULT
w32_imeadv_wm_ime_startcomposition_emacs26( HWND hWnd , WPARAM wParam , LPARAM lParam )
{
  // deny break in WM_IME_STARTCOMPOSITION , call DefWindowProc 
  LRESULT const result = ::DefSubclassProc( hWnd , WM_IME_STARTCOMPOSITION , wParam , lParam );
  // ここでの問題点は、GNU版の Emacs は Lispスレッドが、何度も WM_IME_STARTCOMPOSITION を連打するという問題がある。
  // これはバグだと思うが、オリジナルを修正しないようにするためには、ここでフィルタする

  if( ignore_wm_ime_start_composition ){
    return result;
  }
  
  ignore_wm_ime_start_composition = 1;

  /* 
     注意： 最初の DefSubclassProc() および Emacs の w32fns.c の確認が必要 
     
     この関数は SubClassProc であって、「DefSubclassProc 呼び出し、DefWindowProc は呼び出さない」というのが通常の
     作り方である

     しかしながら、問題点は、(emacs 26.1 までは) ウィンドウプロシージャがDefWindowProc を呼び出さないために
     IMEのover-the-spot 変換が動作しないという事であった。
     ここでは、 DefSubclassProc を呼び出した上に、DefWindowProc で動作を変更するというのが最も重要な仕事である。 */
  if( IsWindowUnicode( hWnd ) ){
    return ::DefWindowProcW( hWnd , WM_IME_STARTCOMPOSITION , wParam , lParam );
  }else{
    return ::DefWindowProcA( hWnd , WM_IME_STARTCOMPOSITION , wParam , lParam );
  }
}

/*  emacs 27 では、WM_IME_STARTCOMPOSITION で DefWindowProc が呼び出されるように変更された。
    そのためここで特段に処理をする必要は無い。 */
static LRESULT
w32_imeadv_wm_ime_startcomposition_emacs27( HWND hWnd , WPARAM wParam , LPARAM lParam )
{
  /* 
     26.2 以降はのEmacs は、DefWindowProc を呼び出すように変更されたので、 DefSubclassProc() を呼び出すのみでよくなった
  */
  if( false ){
    const LRESULT result = ::DefSubclassProc( hWnd , WM_IME_STARTCOMPOSITION , wParam , lParam );
    ignore_wm_ime_start_composition = 1;
    return result;
  }else{

    /* WM_IME_STARTCOMPOSITION は自前で処理するので、emacs をバイパスする */
    ignore_wm_ime_start_composition = 1;

    if( IsWindowUnicode( hWnd ) ){
      return DefWindowProcW( hWnd, WM_IME_STARTCOMPOSITION , wParam , lParam );
    }else{
      return DefWindowProcA( hWnd, WM_IME_STARTCOMPOSITION , wParam , lParam );
    }
  }
}

static LRESULT
w32_imeadv_wm_ime_composition( HWND hWnd , WPARAM wParam , LPARAM lParam )
{

  if( !( lParam & GCS_RESULTSTR ) ){ /* 確定じゃなければ */
    do{ /* The request of IME Composition font setting */
      HWND communication_window_handle = reinterpret_cast<HWND>( GetProp( hWnd , "W32_IMM32ADV_COMWIN" ));
      if( !communication_window_handle ){
        DebugOutputStatic( " communication_window_handle is 0 " );
        break;
      }

      /*The request of IME Composition font setting dose not need to be a synchronous method.
        So use PostMessage() here. */
      if( PostMessageW( communication_window_handle , WM_W32_IMEADV_REQUEST_COMPOSITION_FONT ,
                        reinterpret_cast<WPARAM>(hWnd) ,lParam ) ){
      }else{
        DebugOutputStatic( "SendMessage WM_W32_IMEADV_REQUEST_COMPOSITION_FONT failed" );
      }
    }while( false );
  }

  if( lParam & GCS_RESULTSTR )
    {
      HIMC hImc = ImmGetContext( hWnd );
      if( hImc )
        {
          const LONG byte_size = ImmGetCompositionStringW( hImc , GCS_RESULTSTR , NULL , 0 );
          if( IMM_ERROR_NODATA == byte_size ){
          }else if( IMM_ERROR_GENERAL == byte_size ){
          }else if( 0 == byte_size ){
          }else{
            /* byte_size is supposed even, odd values ​​since also take ,
               and to 2 by adding a minute fraction .*/
            size_t const character_length =
              (((size_t)((ULONG)byte_size) )/sizeof( wchar_t )) + 2 ;
            
            LPVOID himestr = HeapAlloc( GetProcessHeap() ,
                                        HEAP_ZERO_MEMORY ,
                                        (SIZE_T)(sizeof(wchar_t) * character_length ));
            if( himestr ){
              const LONG copydata_bytes =
                ImmGetCompositionStringW( hImc , GCS_RESULTSTR , himestr , sizeof( wchar_t ) * character_length );
              
              assert( static_cast<size_t>(copydata_bytes) < (character_length * sizeof( wchar_t ) ));
              
              if( byte_size == copydata_bytes ){
                wchar_t* lpStr = static_cast< wchar_t * >(himestr);
                size_t const loop_end = (((size_t)((ULONG)byte_size) )/sizeof( wchar_t ));
                for( size_t i = 0 ; i < loop_end ; ++i ){
                  /* considering UTF-16 surrogate pair */
                  if( L'\0' == lpStr[i] ) break;
                  uint32_t utf32_code = 0u; /* UTF32 */
                  if ((lpStr[i] & 0xFC00) == 0xD800         /* This is hight surrogate */
                      && (lpStr[i + 1] & 0xFC00) == 0xDC00) /* This is low surrogate */
                    { /* (lpStr[i]) and ( lpStr[i+1] ) make surrogate pair. */
                      /*  make UTF-32 codepoint from UTF-16 surrogate pair */
                      utf32_code = 0x10000 + (((lpStr[i] & 0x3FF) << 10) | (lpStr[i + 1] & 0x3FF));
                      ++i; /* surrogate pair were processed ,so advance lpStr pointer */
                    }
                  else
                    { /* (lpStr[i]) is pointing BMP */
                      utf32_code = lpStr[i];
                    }
                  /* TODO validate utf-32 codepoint */
                  assert( !utf32_code );
                  if( utf32_code ){
                    PostMessageW( hWnd , WM_UNICHAR , (WPARAM) utf32_code , 0 );
                  }
                } /* end of cracking IME Result string and queueing WM_UNICHARs. */
              }
              if( ! HeapFree( GetProcessHeap() , 0 , himestr ) ){
                assert( !"! HeapFree( GetProcessHeap() , 0 , himestr ) " );
              }
            }
          }
          VERIFY(ImmReleaseContext( hWnd , hImc ));
          lParam &= (~GCS_RESULTSTR );
        } // end of if( hImc )
    }
  return DefWindowProc( hWnd , WM_IME_COMPOSITION , wParam , lParam );
}

static LRESULT
w32_imeadv_wm_ime_endcomposition( HWND hWnd , WPARAM wParam , LPARAM lParam )
{
  ignore_wm_ime_start_composition = 0;

  return DefSubclassProc( hWnd , WM_IME_ENDCOMPOSITION , wParam , lParam );
}

static LRESULT
w32_imeadv_wm_ime_notify( HWND hWnd, WPARAM wParam ,LPARAM  lParam )
{
  switch( wParam ){
  case IMN_SETOPENSTATUS:
    {
      HWND communication_window_handle = reinterpret_cast<HWND>( GetProp( hWnd , "W32_IMM32ADV_COMWIN" ) );
      if( communication_window_handle )
        {
          HIMC hImc = ImmGetContext( hWnd );
          if( hImc )
            {
              if( ImmGetOpenStatus( hImc ) )
                VERIFY(PostMessageW( communication_window_handle , WM_W32_IMEADV_OPENSTATUS_OPEN , 0 , 0 ));
              else
                VERIFY(PostMessageW( communication_window_handle , WM_W32_IMEADV_OPENSTATUS_CLOSE , 0 , 0 ));
              VERIFY(ImmReleaseContext( hWnd , hImc ));
            }
        }
    }
    break;
  default:
    // TODO
    //OutputDebugStringA( "w32_imeadv_wm_ime_notify( HWND hWnd, WPARAM wParam ,LPARAM  lParam )\n");
    break;
  }
  return DefSubclassProc( hWnd, WM_IME_NOTIFY , wParam , lParam );
}

static LRESULT
w32_imeadv_wm_ime_request( HWND hWnd , WPARAM wParam , LPARAM lParam )
{
  switch( wParam ){
  case IMR_CANDIDATEWINDOW:
    {
#if !defined( NDEBUG )
      DebugOutputStatic("w32_imeadv_wm_ime_request -> IMR_CANDIDATEWINDOW" );
#endif /* !defined( NDEBUG ) */
      break;
    }

  case IMR_COMPOSITIONFONT:
    {
      DebugOutputStatic("w32_imeadv_wm_ime_request -> IMR_COMPOSITIONFONT");
      HWND communication_window_handle = reinterpret_cast<HWND>( GetProp( hWnd , "W32_IMM32ADV_COMWIN" ));
      if( communication_window_handle ){
        my_wait_message<WM_W32_IMEADV_NOTIFY_COMPOSITION_FONT>(hWnd, 
                                                               ([&]()->int{
                                                                 // Wait Conversion Message
                                                                 OutputDebugStringA( "IMR_COMPOSITIONFONT waiting message\n");
                                                                 if( SendMessageW( communication_window_handle , WM_W32_IMEADV_REQUEST_COMPOSITION_FONT ,
                                                                                   reinterpret_cast<WPARAM>(hWnd) , 0  ) ){
                                                                   return 1;
                                                                 }
                                                                 return 0;
                                                               }));
      }
      break;
    }

  case IMR_COMPOSITIONWINDOW:
    {
#if !defined( NDEBUG )
      DebugOutputStatic("w32_imeadv_wm_ime_request -> IMR_COMPOSITIONWINDOW");
#endif /* !defined( NDEBUG ) */
      break;
    }
    
  case IMR_CONFIRMRECONVERTSTRING:
    {
#if !defined( NDEBUG )
      DebugOutputStatic( "w32_imeadv_wm_ime_request -> IMR_CONFIRMRECONVERTSTRING" );
#endif /* !defined( NDEBUG ) */
    }
    break;

  case IMR_DOCUMENTFEED:
    {
      //      DebugOutputStatic( "w32_imeadv_wm_ime_request -> IMR_DOCUMENTFEED" );
      HWND communication_window_handle = reinterpret_cast<HWND>( GetProp( hWnd , "W32_IMM32ADV_COMWIN" ));
      if( communication_window_handle ){
        using imeadv::NotifyReconversionString;
        std::unique_ptr<NotifyReconversionString> nrs { new ( std::nothrow ) NotifyReconversionString() };
        if( ! static_cast<bool>( nrs ) ){
          return 0;
        }
        
        /*  WM_W32_IMEADV_NOTIFY_DOCUMENTFEED_STRING を処理する */
        SUBCLASSPROC const subclass_proc = 
          []( HWND hWnd , UINT uMsg , WPARAM wParam , LPARAM lParam ,
              UINT_PTR uIdSubclass , DWORD_PTR dwRefData )->LRESULT {
            switch( uMsg ){
            case WM_W32_IMEADV_NOTIFY_DOCUMENTFEED_STRING:
              {
                NotifyReconversionString* dst( reinterpret_cast<NotifyReconversionString*>( dwRefData ));
                NotifyReconversionString* src( reinterpret_cast<NotifyReconversionString*>( lParam ));
                if( dst && src ){
                  dst->pos = src->pos;
                  dst->begin = src->begin;
                  dst->end = src->end;
                  dst->first_half_num = src->first_half_num;
                  dst->later_half_num = src->later_half_num;
                  dst->first_half = src->first_half;
                  dst->later_half = src->later_half;
                }
                break;
              }
            default:
              break;
            }
            return ::DefSubclassProc( hWnd, uMsg ,wParam , lParam );
          };
        
        if( SetWindowSubclass( hWnd, subclass_proc ,
                               reinterpret_cast<UINT_PTR>( subclass_proc ) ,
                               reinterpret_cast<DWORD_PTR>( nrs.get()  ) ) ){
          my_wait_message<WM_W32_IMEADV_NOTIFY_DOCUMENTFEED_STRING>( hWnd , 
                                                                     [&]()->int{
                                                                       if( SendMessageW( communication_window_handle , WM_W32_IMEADV_REQUEST_DOCUMENTFEED_STRING ,
                                                                                         reinterpret_cast<WPARAM>(hWnd) , 0 ) ){
                                                                         return 1;
                                                                       }else{
                                                                         return 0;
                                                                       }
                                                                     } );
          
          if( !::RemoveWindowSubclass( hWnd , subclass_proc , reinterpret_cast<UINT_PTR>( subclass_proc ) ) ){
            // 不本意ながら、メモリーリークとなるが、サブクラス化の解除が出来ないので、dwRefData をそのまま残しておく 
            nrs.release();
            DebugOutputStatic("** WARNING ** RemoveWindowSubclass failed");
            return 0;
          }
          if( lParam ){
            BYTE * const ptr(reinterpret_cast<BYTE*>( lParam ));
            RECONVERTSTRING * const reconv( reinterpret_cast<RECONVERTSTRING*>( lParam ) );
            if( reconv->dwSize < sizeof( RECONVERTSTRING ) ){
              DebugOutputStatic( "reconv->dwSize < sizeof( RECONVERTSTRING )" );
              return 0;
            }
            if( reconv->dwStrOffset < sizeof( RECONVERTSTRING ) ){
              reconv->dwStrOffset = sizeof( RECONVERTSTRING );
            }
            if( reconv->dwSize < reconv->dwStrOffset ){
              DebugOutputStatic( "reconv->dwSize < reconv->dwStrOffset" );
              return 0;
            }
            reconv->dwVersion = 0;
            reconv->dwStrLen = (std::min)((( reconv->dwSize - reconv->dwStrOffset ) / sizeof( wchar_t )) ,
                                          nrs->first_half.size() + nrs->later_half.size() );
            // reconv->dwStrOffset ; set up above.
            reconv->dwCompStrLen = 0;
            reconv->dwCompStrOffset = sizeof( wchar_t ) * nrs->first_half.size() ;
            reconv->dwTargetStrLen = 0;
            reconv->dwTargetStrOffset = sizeof( wchar_t ) * nrs->first_half.size() ;
            
            wchar_t* const text = reinterpret_cast<wchar_t*>(ptr + reconv->dwStrOffset);
            wchar_t* p = text;

            if(nrs->first_half.size() <= reconv->dwStrLen){
              for( auto&& v : nrs->first_half ){
                if( p - text < reconv->dwStrLen){
                  *(p++) = v;
                }else{
                  break;
                }
              }
              for( auto&& v : nrs->later_half ){
                if( p - text < reconv->dwStrLen ){
                  *(p++) = v;
                }else{
                  break;
                }
              }
              if( p < text + reconv->dwStrLen ){
                std::fill( p , text + reconv->dwStrLen , L'\0' );
              }
            }

            return reconv->dwStrLen;
          }else{
            return sizeof( RECONVERTSTRING ) +
              (sizeof( wchar_t ) * ( nrs->first_half.size() + nrs->later_half.size() + 1 ));
          }
        }
      }
      return 0;
    }
    
  case IMR_QUERYCHARPOSITION:
    {
      // ずっと IMR_QUERYCHARPOSITION を送ってくる
#if 1
      return ::DefWindowProc( hWnd , WM_IME_REQUEST , IMR_QUERYCHARPOSITION , lParam );
#else
      if( !lParam ){
        DebugOutputStatic( "IMR_QUERYCHARPOSITION lParam is nullptr ?" );
        return 0;
      }
      
      //DebugOutputStatic( "w32_imeadv_wm_ime_request -> IMR_QUERYCHARPOSITION" );
      IMECHARPOSITION *imecharposition = reinterpret_cast<IMECHARPOSITION*>( lParam );
      assert( sizeof( IMECHARPOSITION ) == imecharposition->dwSize );
      if( sizeof( IMECHARPOSITION ) == imecharposition->dwSize ){
        return 1;
      }
      return ::DefWindowProc( hWnd , WM_IME_REQUEST , IMR_QUERYCHARPOSITION , lParam );
#endif
    }
    
  case IMR_RECONVERTSTRING:
    {
      //DebugOutputStatic( "w32_imeadv_wm_ime_request -> IMR_RECONVERTSTRING" );
      HWND communication_window_handle = reinterpret_cast<HWND>( GetProp( hWnd , "W32_IMM32ADV_COMWIN" ));
      if( communication_window_handle ){
        VERIFY(PostMessageW( communication_window_handle , WM_W32_IMEADV_REQUEST_RECONVERSION_STRING ,
                             reinterpret_cast<WPARAM>( hWnd ), 0 ));
      }
      return 0;
    }
  default:
    {
#if !defined( NDEBUG )
      std::stringstream out{};
      out << "WM_IME_REQUEST " << wParam << " "
          << "(@" __FILE__ << ",L." << __LINE__ << ")" << std::endl;
      OutputDebugStringA( out.str().c_str() );
#endif /* !defined( NDEBUG ) */
    }
    break;
  }
  return DefSubclassProc( hWnd, WM_IME_REQUEST , wParam , lParam );
}

static LRESULT
w32_imeadv_null( HWND hWnd , WPARAM wParam , LPARAM lParam )
{
  return 1;
}

static LRESULT
w32_imeadv_openstatus_open( HWND hWnd , WPARAM wParam , LPARAM lParam )
{
#if !defined( NDEBUG )
  DebugOutputStatic("w32_imeadv_openstatus_open");
#endif /* !defined( NDEBUG ) */

  HIMC hImc = ImmGetContext( hWnd );
  if( hImc ){
    VERIFY(ImmSetOpenStatus( hImc , TRUE ));
    VERIFY(ImmReleaseContext( hWnd, hImc ));
    return 1;
  }
  return 0;
}

static LRESULT
w32_imeadv_openstatus_close( HWND hWnd , WPARAM wParam , LPARAM lParam )
{
#if !defined( NDEBUG )
  DebugOutputStatic("w32_imeadv_openstatus_close");
#endif /* !defined( NDEBUG ) */

  HIMC hImc = ImmGetContext( hWnd );
  if( hImc ){
    VERIFY(ImmSetOpenStatus( hImc, FALSE ));
    VERIFY(ImmReleaseContext( hWnd, hImc ));
    return 1;
  }
  return 0;
}

static LRESULT
w32_imeadv_notify_reconversion_string( HWND hWnd , WPARAM wParam , LPARAM lParam )
{
  BOOL result = false;
  if( lParam ){
    // これは今Lisp スレッドが SendMessage で送ってきてるから、lParam をコピーして自分に PostMessageして第二段階へ移行する。
    using imeadv::NotifyReconversionString;
    const NotifyReconversionString *nrs = reinterpret_cast<NotifyReconversionString*>( lParam );
    // lParam の指し示す先はスタックなので、即座にコピー
    std::unique_ptr<NotifyReconversionString> ptr{ new (std::nothrow) NotifyReconversionString{} };
    if( ptr ){
      ptr->pos            = nrs->pos;
      ptr->begin          = nrs->begin;
      ptr->end            = nrs->end;
      ptr->first_half_num = nrs->first_half_num;
      ptr->later_half_num = nrs->later_half_num;
      ptr->first_half     = nrs->first_half;
      ptr->later_half     = nrs->later_half;
      if( (result = PostMessage( hWnd, WM_w32_IMEADV_UI_PERFORM_RECONVERSION ,
                                 0 , reinterpret_cast<LPARAM>(ptr.get()) ) )){
        ptr.release();
      }
    }
  }
  return result;
}

static LRESULT CALLBACK w32_imeadv_ui_delete_reconversion_region( HWND hWnd, UINT message , WPARAM wParam , LPARAM lParam ,
                                                                  UINT_PTR uIdSubclass, DWORD_PTR dwRefData )
{
  if( WM_IME_COMPOSITION == message ){
    if( lParam & GCS_RESULTSTR ){ // 文字が確定したので、消す
      HWND communication_window_handle = reinterpret_cast<HWND>( GetProp( hWnd , "W32_IMM32ADV_COMWIN" ));
      if( communication_window_handle ){
        if( dwRefData ){
          w32_imeadv_request_delete_char_lparam delete_char = { hWnd , size_t( dwRefData ) };
          my_wait_message<WM_W32_IMEADV_NOTIFY_DELETE_CHAR>( hWnd , [&]()->int{
            return static_cast<int>(SendMessage( communication_window_handle ,
                                                 WM_W32_IMEADV_REQUEST_DELETE_CHAR ,
                                                 reinterpret_cast<WPARAM>( hWnd ),
                                                 reinterpret_cast<LPARAM>( &delete_char ) ));
          });
        }
      }
      VERIFY( RemoveWindowSubclass( hWnd , w32_imeadv_ui_delete_reconversion_region , uIdSubclass ) );
    }
  }
  return ::DefSubclassProc( hWnd, message , wParam , lParam );
}

static LRESULT
w32_imeadv_ui_perform_reconversion( HWND hWnd, WPARAM wParam , LPARAM lParam )
{
  // 実際の再変換コードはココ
  if( ! lParam ){
    return 0;
  }
  
  using imeadv::NotifyReconversionString;
  std::unique_ptr<NotifyReconversionString> nrs{reinterpret_cast<NotifyReconversionString*>( lParam )};
  const size_t text_length = nrs->first_half.size() + nrs->later_half.size();
  const size_t text_memory_byte = sizeof( wchar_t ) * (text_length + 1);
  const size_t memory_block_size = sizeof( RECONVERTSTRING ) + text_memory_byte;
  std::unique_ptr<BYTE[]> memory_block{ new ( std::nothrow ) BYTE[ memory_block_size ] };
  if( memory_block ){
    // メモリブロックを切り分けます
    RECONVERTSTRING *reconv{reinterpret_cast<RECONVERTSTRING*>( memory_block.get() ) };
    { // reconv の設定
      reconv->dwSize = memory_block_size;
      reconv->dwVersion = 0;
      reconv->dwStrLen = text_length;
      reconv->dwStrOffset = sizeof( RECONVERTSTRING );
      reconv->dwCompStrLen = 0;
      reconv->dwCompStrOffset = nrs->first_half.size() * sizeof( wchar_t );
      reconv->dwTargetStrLen = 0;
      reconv->dwTargetStrOffset = reconv->dwCompStrOffset;
    }
    wchar_t * text{reinterpret_cast<wchar_t*>( memory_block.get() + sizeof( RECONVERTSTRING ))};
    { // textの設定
      wchar_t * p = text;
      std::for_each( std::begin( nrs->first_half ) , std::end( nrs->first_half ),
                     [&p]( const wchar_t& v ){
                       *(p++) = v;
                     } );
      std::for_each( std::begin( nrs->later_half ) , std::end( nrs->later_half ),
                     [&p]( const wchar_t& v ){
                       *(p++) = v;
                     } );
      *(p++) = L'\0';
      assert( reinterpret_cast<void*>(p) == (memory_block.get() + memory_block_size) );
    }
    
    // メモリブロック全体の準備が出来たので、変換実行を始めます
    HIMC hImc = ImmGetContext( hWnd );
    if( hImc ){
      HWND communication_window_handle = reinterpret_cast<HWND>( GetProp( hWnd , "W32_IMM32ADV_COMWIN" ));
      do{
        const DWORD cursor_position = reconv->dwCompStrOffset;
        // まず最初に RECONVERTSTRING の調整をIMEに任せます。
        if( ! ImmSetCompositionStringW( hImc , SCS_QUERYRECONVERTSTRING ,
                                        (LPVOID)memory_block.get(),
                                        (DWORD)memory_block_size ,
                                        nullptr , 0 ) ){
          DebugOutputStatic( "ImmSetCompositionStringW SCS_QUERYRECONVERTSTRING failed" );
          break;
        }
        // カーソル位置の調整を行う
        if( 0 < cursor_position - reconv->dwCompStrOffset ){
          /* cursor_position から reconv->dwComStrOffset にカーソルが左にずれたので、
             これをバイト単位から wchar_t 単位にして、サロゲートペアを考慮して文字数単位に変換する */
          wchar_t * const lft = reinterpret_cast<wchar_t*>((reinterpret_cast<BYTE*>(text)) + reconv->dwCompStrOffset);
          wchar_t * const rgt = reinterpret_cast<wchar_t*>((reinterpret_cast<BYTE*>(text)) + cursor_position);
          size_t nCharacter = 0;
          for( wchar_t* p = lft ; p < rgt ; ++p , ++nCharacter ){
            if( ((*p & 0xFC00) == 0xD800) && (*(p+1)& 0xFC00)== 0xDC00 ){
              ++p; // UTF-16 surroage pair;
            }
          }
          if( 0 < nCharacter ){
            if( communication_window_handle ){
              w32_imeadv_request_backward_char_lparam backward_char = { hWnd, nCharacter };
              my_wait_message<WM_W32_IMEADV_NOTIFY_BACKWARD_CHAR>( hWnd ,
                                                                   [&]()->DWORD{
#if 1 /* これはPostMessage でも大丈夫な作りになっている backward_char が破棄されるのは、 my_wait_message から戻ってきてからだから */
                                                                     return static_cast<int>(SendMessage( communication_window_handle ,
                                                                                                          WM_W32_IMEADV_REQUEST_BACKWARD_CHAR ,
                                                                                                          reinterpret_cast<WPARAM>(hWnd),
                                                                                                          reinterpret_cast<LPARAM>(&backward_char) ) );
#else
                                                                     if( PostMessage ( communication_window_handle ,
                                                                                       WM_W32_IMEADV_REQUEST_BACKWARD_CHAR ,
                                                                                       reinterpret_cast<WPARAM>(hWnd),
                                                                                       reinterpret_cast<LPARAM>(&backward_char) )){
                                                                       return nCharacter;
                                                                     }else{
                                                                       return 0;
                                                                     }
#endif
                                                                   });
            }
          }
        }

        {
          // 再変換文字は、後でIMEが投入してくるので、文字が確定された時に消すようにサブクラス化をかけておく
          wchar_t * const lft =
            reinterpret_cast<wchar_t*>( (reinterpret_cast<BYTE*>(text)) + reconv->dwCompStrOffset );
          wchar_t * const rgt = lft + reconv->dwCompStrLen;
          size_t nCharacter = 0;
          for( wchar_t* p = lft ; p < rgt ; ++p , ++nCharacter ){
            if( ((*p & 0xFC00) == 0xD800) && (*(p+1)& 0xFC00)== 0xDC00 ){
              ++p; // UTF-16 surroage pair;
            }
          }
          if( 0 < nCharacter ){
            if( SetWindowSubclass( hWnd , w32_imeadv_ui_delete_reconversion_region ,
                                   reinterpret_cast<UINT_PTR>( w32_imeadv_ui_delete_reconversion_region ),
                                   DWORD_PTR( nCharacter ) ) ){

            }
          }
        }

        // 準備が整ったので、再変換を実行する
        if( ! ImmSetCompositionStringW( hImc, SCS_SETRECONVERTSTRING ,
                                        (LPVOID)memory_block.get(),
                                        (DWORD)memory_block_size ,
                                        nullptr , 0 ) ){
          DebugOutputStatic( "ImmSetCompositionStringW SCS_SETRECONVERTSTRING failed" );
          break;
        }
        
      }while( false );
      VERIFY( ImmReleaseContext( hWnd, hImc ) );
    }
    
  }
  return 1;
}

static LRESULT
w32_imeadv_wm_emacs_track_caret_hook( HWND hWnd , WPARAM wParam , LPARAM lParam )
{
  assert( 0 == wParam );
  assert( 0 == lParam );
  LRESULT const result = ::DefSubclassProc(hWnd, WM_EMACS_TRACK_CARET ,wParam ,lParam );
  POINT caret_position = {0};
  POINT previous_point = {0};

  if( !::GetCaretPos( &caret_position ) ){
    return result;
  }
  
  static_assert( sizeof( POINT ) <= sizeof( HANDLE ), "");

  {
    HANDLE handle = GetProp( hWnd , TEXT( "w32-imeadv-caret-position" ) );
    if( handle ){
      memcpy_s( &previous_point , sizeof(previous_point) , &handle , sizeof(previous_point) );
    }
  }

  /* 
  {
    std::stringstream ss{};
    ss << "caret{x: " << caret_position.x << ", y: " << caret_position.y <<"}, "
       << "prev{x: " << previous_point.x << ", y: " << previous_point.y << "}";
    OutputDebugString( ss.str().c_str() );
  }
  */
  
  if( caret_position.x != previous_point.x ||
      caret_position.y != previous_point.y )
    {
      RECT rect = {0};
      VERIFY( GetClientRect( hWnd , &rect ) );
      COMPOSITIONFORM form = { CFS_RECT , caret_position , rect };
      HIMC hImc = ImmGetContext ( hWnd );
      if( hImc ){
        ImmSetCompositionWindow( hImc , &form );
        VERIFY( ImmReleaseContext ( hWnd , hImc ) );
      }
    }
  {
    HANDLE handle = 0;
    memcpy_s( &handle , sizeof(caret_position) , &caret_position , sizeof(caret_position) );
    SetProp( hWnd , TEXT( "w32-imeadv-caret-position" ), handle );
  }
  return result;
}

LRESULT (CALLBACK subclass_proc)( HWND hWnd , UINT uMsg , WPARAM wParam , LPARAM lParam ,
                                  UINT_PTR uIdSubclass, DWORD_PTR dwRefData ){
  UNREFERENCED_PARAMETER( dwRefData );
  if( WM_DESTROY == uMsg )
    SendMessageW( hWnd, WM_W32_IMEADV_UNSUBCLASSIFY , 0 , 0 ); // 自分自身に送る
  
  switch( uMsg ){
  case WM_KEYDOWN:
    {
      switch( wParam ){
      case VK_PROCESSKEY: // VK_PROCESSKEY は Ctrlキーを押しているときがあるので、Emacs本体には触らせない。
        { 
          const MSG msg = { hWnd , uMsg , wParam , lParam , (DWORD)GetMessageTime() , {0} };
          (void)TranslateMessage(&msg);
          return ::DefWindowProc( hWnd, uMsg , wParam , lParam );
        }
        break;
        
#if 1
      case 0x58: // X Key 
        { /* Ctrl-X が来たときに IME を無効にする本当はLispスレッドでやるべき仕事 experimental */
          if( GetAsyncKeyState( VK_CONTROL ) ){ 
            HIMC hImc = ImmGetContext( hWnd );
            if( hImc ){
              if( ImmGetOpenStatus( hImc ) ){
                ImmSetOpenStatus( hImc , FALSE );
              }
              ImmReleaseContext( hWnd , hImc );
            }
          }
        }
        break;
#endif
      default:
        break;
      }
    }
    return ::DefSubclassProc( hWnd , uMsg , wParam ,lParam );
  case WM_IME_COMPOSITION:
    return w32_imeadv_wm_ime_composition( hWnd , wParam , lParam );

  case WM_IME_STARTCOMPOSITION:
    {
      bool emacs_is_broken_ime_startcomposition{false};
      {
        std::unique_lock<decltype( w32_imeadv_runtime_environment.mutex )>
          lock( w32_imeadv_runtime_environment.mutex );

        if( w32_imeadv_runtime_environment.emacs_major_version < 27 )
          {
            switch( w32_imeadv_runtime_environment.emacs_minor_version )
              {
              case 0:
              case 1:
                emacs_is_broken_ime_startcomposition = true;
                break;
                /* w32fns.c の WM_IME_STARTCOMPOSITION で DefWindowProc を呼び出さない問題は、 26.1 と 26.2 の間で修正された。 */
                /* このため 26.1 の時のみ 動作を変える (26.0は、開発バージョンなので普通使われない はず）*/
              case 2:
              case 3:
              default:
                emacs_is_broken_ime_startcomposition = false;
                break;
              }
          }
      }
      
      if( emacs_is_broken_ime_startcomposition )
        return w32_imeadv_wm_ime_startcomposition_emacs26( hWnd ,wParam , lParam);
      else
        return w32_imeadv_wm_ime_startcomposition_emacs27( hWnd ,wParam , lParam);
    }
  case WM_IME_ENDCOMPOSITION:
    return w32_imeadv_wm_ime_endcomposition( hWnd, wParam , lParam );

  case WM_IME_NOTIFY :
    return w32_imeadv_wm_ime_notify( hWnd, wParam , lParam );

  case WM_IME_REQUEST:
    return w32_imeadv_wm_ime_request( hWnd, wParam , lParam );
    
    /* ********************************** */
    /* Private Window Message             */
    /* ********************************** */
  case WM_W32_IMEADV_NULL:
    return w32_imeadv_null( hWnd , wParam , lParam );

  case WM_W32_IMEADV_GET_OPENSTATUS:
    {
      BOOL result(FALSE);
      HIMC hImc = ImmGetContext( hWnd );
      if( hImc ){
        result = ImmGetOpenStatus( hImc );
        VERIFY(ImmReleaseContext( hWnd, hImc ));
      }
      return result;
    }
  case WM_W32_IMEADV_OPENSTATUS_OPEN:
    return w32_imeadv_openstatus_open( hWnd , wParam , lParam );

  case WM_W32_IMEADV_OPENSTATUS_CLOSE:
    return w32_imeadv_openstatus_close( hWnd , wParam , lParam );

  case WM_W32_IMEADV_NOTIFY_COMPOSITION_FONT:
    {
      if( lParam ){
        const w32_imeadv_composition_font_configure* font_configure =
          reinterpret_cast<const w32_imeadv_composition_font_configure*>( lParam );
        if( font_configure->enable_bits ){
          HIMC hImc = ImmGetContext( hWnd );
          if( hImc ){
            LOGFONTW logFont = {0};
            if( ImmGetCompositionFontW( hImc , &logFont ) ){
              bool dirtyBit = false; // フォントの設定が異なっていれば再設定する
              if( font_configure->enable_bits & W32_IMEADV_FONT_CONFIGURE_BIT_FACENAME ){
                static_assert( sizeof( logFont.lfFaceName ) == sizeof( font_configure->lfFaceName ),
                               "sizeof( logFont.lfFaceName ) == sizeof( font_configure->lfFaceName )" );
                if( wcscmp( font_configure->lfFaceName , logFont.lfFaceName ) ){
                  std::copy( std::begin( font_configure->lfFaceName ) , std::end( font_configure->lfFaceName ) ,
                             &(logFont.lfFaceName[0]) );
                  dirtyBit = true;
                }
              }
              if( font_configure->enable_bits & W32_IMEADV_FONT_CONFIGURE_BIT_FONTHEIGHT ){
                const auto value =
                  - std::abs( static_cast<LONG>( (double(font_configure->font_height) / double(10.0f)) *
                                                 (double(GetDeviceCaps(GetDC(NULL),LOGPIXELSY)) / double( 72.0f ) )));
                if( value != logFont.lfHeight ){
                  logFont.lfHeight = value;
                  dirtyBit = true;
                }
                logFont.lfWidth = 0;
                assert( logFont.lfHeight < 0 || !"lfHeight must be negative integer.");
              }
              if( dirtyBit ){
                VERIFY( ImmSetCompositionFontW( hImc, &logFont ) );
              }
            }
            VERIFY(ImmReleaseContext( hWnd, hImc ));
          }
        }
      }
    }
    return 1;

  case WM_W32_IMEADV_NOTIFY_RECONVERSION_STRING:
    return w32_imeadv_notify_reconversion_string( hWnd, wParam , lParam );

  case WM_w32_IMEADV_UI_PERFORM_RECONVERSION:
    return w32_imeadv_ui_perform_reconversion( hWnd, wParam , lParam );
    
  case WM_W32_IMEADV_NOTIFY_DOCUMENTFEED_STRING:
    return 1;
  case WM_EMACS_TRACK_CARET:
    return w32_imeadv_wm_emacs_track_caret_hook( hWnd, wParam , lParam );
  default: 
    break;
  }

  if ( WM_W32_IMEADV_SUBCLASSIFY == uMsg )
    {
      HWND communication_window_handle = (HWND)(wParam);
      SetProp( hWnd , "W32_IMM32ADV_COMWIN" , (communication_window_handle ) );
      VERIFY(PostMessageW( communication_window_handle , WM_W32_IMEADV_SUBCLASSIFY , (WPARAM)( hWnd ) , 0));
      return 1;
    }
  else if( WM_W32_IMEADV_UNSUBCLASSIFY == uMsg )
    {
      HWND communication_window_handle = reinterpret_cast<HWND>( GetProp( hWnd , "W32_IMM32ADV_COMWIN" ) );
      if( communication_window_handle ){
        RemoveProp( hWnd , "W32_IMM32ADV_COMWIN" );
        VERIFY(PostMessageW( communication_window_handle , WM_W32_IMEADV_UNSUBCLASSIFY , (WPARAM)(hWnd) , 0 ));
      }
      return ::RemoveWindowSubclass( hWnd , subclass_proc , uIdSubclass);
    }
  
  return DefSubclassProc (hWnd, uMsg , wParam , lParam );
}
