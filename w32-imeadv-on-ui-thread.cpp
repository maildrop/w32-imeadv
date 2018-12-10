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
#include <tuple>

#include <cstddef>
#include <cstdint>
#include <cassert>

#include "w32-imeadv.h"

template<UINT WaitMessage , DWORD dwTimeOutMillSecond = 5000 /* 5 sec timout (for safity) */> static inline BOOL 
my_wait_message( HWND hWnd , DWORD times = 1u);

/* see w32_imm_wm_ime_startcomposition and w32_imm_wm_ime_endcomposition */
static int ignore_wm_ime_start_composition = 0; // このパラメータ プロパティに書いておく必要がある。

static LRESULT
w32_imeadv_wm_ime_startcomposition_emacs26( HWND hWnd , WPARAM wParam , LPARAM lParam );
static LRESULT
w32_imeadv_wm_ime_endcomposition( HWND hWnd , WPARAM wParam , LPARAM lParam );
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


template<UINT WaitMessage , DWORD dwTimeOutMillSecond> static inline BOOL 
my_wait_message( HWND hWnd , DWORD times)
{

  assert( hWnd );

  if(! hWnd )
    return FALSE;
  if(! IsWindow( hWnd ) )
    return FALSE;

  assert( GetCurrentThreadId() == GetWindowThreadProcessId( hWnd, nullptr ) );
  
  SUBCLASSPROC const subclass_proc
    = [](HWND hWnd, UINT uMsg , WPARAM wParam , LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) -> LRESULT {
        std::ignore = uIdSubclass;
        if( WaitMessage == uMsg ){
          DWORD *ptr = reinterpret_cast<DWORD*>( dwRefData );
          --(*ptr);
        }
        return ::DefSubclassProc( hWnd, uMsg , wParam , lParam );
      };
  UINT_PTR const uIdSubclass = reinterpret_cast<UINT_PTR const >( subclass_proc );

  /* not throw std::bad_alloc in this case  */
  std::unique_ptr< DWORD > waiting_data{(new ( std::nothrow ) DWORD{times})}; 
  if( ! static_cast<bool>( waiting_data ) ){
    return FALSE;
  }
  
  if( ::SetWindowSubclass( hWnd , subclass_proc , uIdSubclass , reinterpret_cast<DWORD_PTR>(waiting_data.get()) ) ){
    struct raii{
      HWND const hWnd;
      SUBCLASSPROC const subclass_proc;
      UINT_PTR const uIdSubclass;
      std::unique_ptr< DWORD >& waiting_data;
      ~raii(){
        if( !::RemoveWindowSubclass( this->hWnd, this->subclass_proc , this->uIdSubclass ) ){
          // 不本意ながら、 waiting_data を detach して、状況の保全を図る
          DebugOutputStatic( "** WARNING ** RemoveWindowSubClass failed. " );
          this->waiting_data.release();
        }
      }
    } remove_window_subclass_raii = { hWnd , subclass_proc , uIdSubclass , waiting_data};
    
    while( *waiting_data ){
      typedef std::integral_constant<DWORD , 0> nObjects;
      DWORD const wait_result =
        MsgWaitForMultipleObjects( nObjects::value  , nullptr ,FALSE,
                                   dwTimeOutMillSecond , QS_SENDMESSAGE | QS_PAINT );
      switch( wait_result ){
      case (WAIT_OBJECT_0 + nObjects::value):
        {
          MSG msg = {};
          // Process only messages sent with SendMessage
          while( PeekMessageW( &msg , NULL , WM_NULL ,WM_NULL , PM_REMOVE | PM_QS_SENDMESSAGE | PM_QS_PAINT ) ){
            TranslateMessage( &msg );
            DispatchMessageW( &msg );
          }
          break;
        }
      case WAIT_TIMEOUT:
        {
          DebugOutputStatic( "** WARNNING ** my_wait_message TIME_OUT!" );
          return FALSE;
        }
        goto end_of_loop;
      default:
        if( (WAIT_ABANDONED_0 <= wait_result) && (wait_result < (WAIT_ABANDONED_0 + nObjects::value)) ){
          // nObjects::value = 0
          ::DebugBreak();
        }
        goto end_of_loop;
      }
    }
  end_of_loop:
    return TRUE;
  }
  return FALSE;
}

/**
   Emacs のバグに対応するところ。
   
*/
static LRESULT
w32_imeadv_wm_ime_startcomposition_emacs26( HWND hWnd , WPARAM wParam , LPARAM lParam )
{
  // deny break in WM_IME_STARTCOMPOSITION , call DefWindowProc 
  // Emacs のバージョンで切り分けるという作業をしないとダメですよ
  LRESULT const result = ::DefSubclassProc( hWnd , WM_IME_STARTCOMPOSITION , wParam , lParam );

  // ここでの問題点は、GNU版の Emacs は Lispスレッドが、何度も WM_IME_STARTCOMPOSITION を連打するという問題がある。
  // これはバグだと思うが、オリジナルを修正しないようにするためには、ここでフィルタする

  /* ignore_wm_ime_start_composition を 1にするタイミングについては、
     無節操に、 WM_IME_STARTCOMPOSITION を送る GNU Emacs があるので
     これを止めてよいタイミングは、フォントの設定を送ることができたときである。
     とする。 
     TODO : この前提は間違っているので修正が必要  */

  if( ignore_wm_ime_start_composition ){
    return result;
  }else{
    //OutputDebugStringA("w32_imeadv_wm_ime_startcomposition effective\n");

    // ここでフォントの要求をする
    HWND communication_window_handle = reinterpret_cast<HWND>( GetProp( hWnd , "W32_IMM32ADV_COMWIN" ));
    if( communication_window_handle ){
      if( SendMessageW( communication_window_handle , WM_W32_IMEADV_REQUEST_COMPOSITION_FONT ,
                        reinterpret_cast<WPARAM>(hWnd) ,lParam ) ){
        // Wait Conversion Message
        //DebugOutputStatic( "IMR_COMPOSITIONFONT waiting message" );
        ignore_wm_ime_start_composition = 1;
        my_wait_message<WM_W32_IMEADV_NOTIFY_COMPOSITION_FONT>(hWnd);
        return DefWindowProc( hWnd, WM_IME_STARTCOMPOSITION , wParam , lParam );
      }else{
        DebugOutputStatic( "SendMessage WM_W32_IMEADV_REQUEST_COMPOSITION_FONT failed" );
      }
    }else{
      DebugOutputStatic( " communication_window_handle is 0 " );
    }
  }
  return result;
}

static LRESULT
w32_imeadv_wm_ime_composition( HWND hWnd , WPARAM wParam , LPARAM lParam )
{
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
                wchar_t* lpStr = ( wchar_t *)himestr;
                size_t const loop_end = (((size_t)((ULONG)byte_size) )/sizeof( wchar_t ));
                for( size_t i = 0 ; i < loop_end ; ++i ){
                  /* considering UTF-16 surrogate pair */
                  if( L'\0' == lpStr[i] ) break;
                  uint32_t utf32_code = 0; /* UTF32 */
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
                  if( utf32_code )
                    SendMessageW( hWnd , WM_UNICHAR , (WPARAM) utf32_code , 0 );
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
      DebugOutputStatic("w32_imeadv_wm_ime_request -> IMR_CANDIDATEWINDOW" );
      break;
    }

  case IMR_COMPOSITIONFONT:
    {
      DebugOutputStatic("w32_imeadv_wm_ime_request -> IMR_COMPOSITIONFONT");
      HWND communication_window_handle = reinterpret_cast<HWND>( GetProp( hWnd , "W32_IMM32ADV_COMWIN" ));
      if( communication_window_handle ){
        if( SendMessageW( communication_window_handle , WM_W32_IMEADV_REQUEST_COMPOSITION_FONT ,
                          reinterpret_cast<WPARAM>(hWnd) , 0  ) ){
          // Wait Conversion Message
          OutputDebugStringA( "IMR_COMPOSITIONFONT waiting message\n");
          my_wait_message<WM_W32_IMEADV_NOTIFY_COMPOSITION_FONT>(hWnd);
          return 0;
        }
      }
      break;
    }

  case IMR_COMPOSITIONWINDOW:
    {
      DebugOutputStatic("w32_imeadv_wm_ime_request -> IMR_COMPOSITIONWINDOW");
      break;
    }
    
  case IMR_CONFIRMRECONVERTSTRING:
    {
      DebugOutputStatic( "w32_imeadv_wm_ime_request -> IMR_CONFIRMRECONVERTSTRING" );
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
        SUBCLASSPROC const subclass_proc = 
          []( HWND hWnd , UINT uMsg , WPARAM wParam , LPARAM lParam ,
              UINT_PTR uIdSubclass , DWORD_PTR dwRefData )->LRESULT {
            NotifyReconversionString* dst( reinterpret_cast<NotifyReconversionString*>( dwRefData ));
            NotifyReconversionString* src(reinterpret_cast<NotifyReconversionString*>( lParam ));
            if( dst && src ){
              dst->pos = src->pos;
              dst->begin = src->begin;
              dst->end = src->end;
              dst->first_half_num = src->first_half_num;
              dst->later_half_num = src->later_half_num;
              dst->first_half = src->first_half;
              dst->later_half = src->later_half;
            }
            return ::DefSubclassProc( hWnd, uMsg ,wParam , lParam );
          };
        
        if( SetWindowSubclass( hWnd, subclass_proc ,
                               reinterpret_cast<UINT_PTR>(subclass_proc) ,
                               reinterpret_cast<DWORD_PTR>( nrs.get()  ) ) ){
          if( SendMessageW( communication_window_handle , WM_W32_IMEADV_REQUEST_DOCUMENTFEED_STRING ,
                            reinterpret_cast<WPARAM>(hWnd) , 0 ) ){
            // Wait Conversion Message
            my_wait_message<WM_W32_IMEADV_NOTIFY_DOCUMENTFEED_STRING>( hWnd );
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
#if 0
              {
                std::wstringstream out{};
                out << *reconv << " \"";
                std::for_each( text , text + reconv->dwStrLen ,
                               [&out]( const wchar_t &v ){
                                 out << v;
                               });
                out << DEBUG_STRING( "\" return DOCUMENT_FEEED" );
                OutputDebugStringW( out.str().c_str() );
              }
#endif 
              return reconv->dwStrLen;
            }else{
              return sizeof( RECONVERTSTRING ) +
                (sizeof( wchar_t ) * ( nrs->first_half.size() + nrs->later_half.size() + 1 ));
            }
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
      
      DebugOutputStatic( "w32_imeadv_wm_ime_request -> IMR_QUERYCHARPOSITION" );
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
      DebugOutputStatic( "w32_imeadv_wm_ime_request -> IMR_RECONVERTSTRING" );
      HWND communication_window_handle = reinterpret_cast<HWND>( GetProp( hWnd , "W32_IMM32ADV_COMWIN" ));
      if( communication_window_handle ){
        VERIFY(PostMessageW( communication_window_handle , WM_W32_IMEADV_REQUEST_RECONVERSION_STRING ,
                             reinterpret_cast<WPARAM>( hWnd ), 0 ));
      }
      return 0;
    }
  default:
    {
      std::stringstream out{};
      out << "WM_IME_REQUEST " << wParam << std::endl;
      OutputDebugStringA( out.str().c_str() );
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
  OutputDebugStringA("w32_imeadv_openstatus_open\n");
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
  OutputDebugStringA("w32_imeadv_openstatus_close\n");
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
    // lParam の指し示す先はスタックなので、即座にコピーしてリターンする。
    std::unique_ptr<NotifyReconversionString> ptr{ new (std::nothrow) NotifyReconversionString{} };
    if( ptr ){
      ptr->pos = nrs->pos;
      ptr->begin = nrs->begin;
      ptr->end = nrs->end;
      ptr->first_half_num = nrs->first_half_num;
      ptr->later_half_num = nrs->later_half_num;
      ptr->first_half = nrs->first_half;
      ptr->later_half = nrs->later_half;
      if( (result = PostMessage( hWnd, WM_w32_IMEADV_UI_PERFORM_RECONVERSION ,
                                 0 , reinterpret_cast<LPARAM>(ptr.get()) ) )){
        ptr.release();
      }
    }
  }
  return result;
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
        // カーソル位置の調整を行う必要があるが、後回し。
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
          if( 0< nCharacter ){
            if( communication_window_handle ){
              w32_imeadv_request_backward_char_lparam backward_char = { hWnd, nCharacter };
              auto times = SendMessage( communication_window_handle , WM_W32_IMEADV_REQUEST_BACKWARD_CHAR ,
                                        reinterpret_cast<WPARAM>(hWnd),
                                        reinterpret_cast<LPARAM>(&backward_char ));
              if( times ){
                my_wait_message<WM_W32_IMEADV_NOTIFY_BACKWARD_CHAR>( hWnd,  times );
              }
            }
          }
        }
        
        if( ! ImmSetCompositionStringW( hImc, SCS_SETRECONVERTSTRING ,
                                        (LPVOID)memory_block.get(),
                                        (DWORD)memory_block_size ,
                                        nullptr , 0 ) ){
          
          DebugOutputStatic( "ImmSetCompositionStringW SCS_SETRECONVERTSTRING failed" );
          break;
        }
        {
          // 再変換文字は、後でIMEが投入してくるので、ここで消す
          wchar_t * const lft =
            reinterpret_cast<wchar_t*>( (reinterpret_cast<BYTE*>(text)) + reconv->dwCompStrOffset );
          wchar_t * const rgt = lft + reconv->dwCompStrLen;
          size_t nCharacter = 0;
          for( wchar_t* p = lft ; p < rgt ; ++p , ++nCharacter ){
            if( ((*p & 0xFC00) == 0xD800) && (*(p+1)& 0xFC00)== 0xDC00 ){
              ++p; // UTF-16 surroage pair;
            }
          }
          if( 0< nCharacter ){
            w32_imeadv_request_delete_char_lparam delete_char = { hWnd , nCharacter };
            auto times = SendMessage( communication_window_handle ,
                                      WM_W32_IMEADV_REQUEST_DELETE_CHAR ,
                                      reinterpret_cast<WPARAM>( hWnd ),
                                      reinterpret_cast<LPARAM>( &delete_char ) );
            if( times ){
              my_wait_message<WM_W32_IMEADV_NOTIFY_DELETE_CHAR>( hWnd, times );
            }
          }
        }
      }while( false );
      VERIFY( ImmReleaseContext( hWnd, hImc ) );
    }
    
  }
  return 1;
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
      case VK_PROCESSKEY:
        { 
          const MSG msg = { hWnd , uMsg , wParam , lParam , 0 , {0} };
          TranslateMessage(&msg);
          return ::DefWindowProc( hWnd, uMsg , wParam , lParam );
        }
        break;
      default:
        break;
      }
    }
    return ::DefSubclassProc( hWnd , uMsg , wParam ,lParam );
  case WM_IME_COMPOSITION:
    return w32_imeadv_wm_ime_composition( hWnd , wParam , lParam );

  case WM_IME_STARTCOMPOSITION:
    return w32_imeadv_wm_ime_startcomposition_emacs26( hWnd ,wParam , lParam);

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
        w32_imeadv_composition_font_configure* font_configure =
          reinterpret_cast<w32_imeadv_composition_font_configure*>( lParam );
        if( font_configure->enable_bits ){
          HIMC hImc = ImmGetContext( hWnd );
          if( hImc ){
            LOGFONTW logFont = {0};
            if( ImmGetCompositionFontW( hImc , &logFont ) ){
              if( font_configure->enable_bits & W32_IMEADV_FONT_CONFIGURE_BIT_FACENAME ){
                std::copy( std::begin( font_configure->lfFaceName ) , std::end( font_configure->lfFaceName ) ,
                           &(logFont.lfFaceName[0]) );
              }
              if( font_configure->enable_bits & W32_IMEADV_FONT_CONFIGURE_BIT_FONTHEIGHT ){
                const double point_size = static_cast<double>(font_configure->font_height) / 10.0f ;
                logFont.lfHeight =
                  static_cast<LONG>(-(point_size *
                                      ( double( GetDeviceCaps( GetDC(NULL), LOGPIXELSY) ) / double( 72.0f ) )));
                logFont.lfWidth = 0;
              }
              ImmSetCompositionFontW( hImc, &logFont );
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

  default: 
    break;
  }

  if ( WM_W32_IMEADV_SUBCLASSIFY == uMsg )
    {
      // これは、ウィンドウを開いたときに IME が動作しないことがあるという問題に対応するため
      // 一旦ウィンドウを開けたり閉じたりする必要がある。 どうしてなのかはわからない。
      HIMC hImc = ImmGetContext( hWnd );
      if( hImc )
        {
          const BOOL openStatus = ImmGetOpenStatus( hImc );
          if( ImmSetOpenStatus( hImc , ! openStatus ) )
            if( ImmSetOpenStatus( hImc , openStatus ) )
              ; // success 
            else
              DebugOutputStatic( "w32-imeadv subclass_proc second ImmSetStateus failed");
          else
            DebugOutputStatic( "w32-imeadv subclass_proc first ImmSetStateus failed" );
          VERIFY(ImmReleaseContext( hWnd , hImc ));
        }
      
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
