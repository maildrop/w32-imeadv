#include <tchar.h>
#include <windows.h>
#include <imm.h>
#include <commctrl.h>

#include <type_traits>
#include <algorithm>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include <new>
#include <memory>
#include <tuple>

#include <cstddef>
#include <cstdint>
#include <cassert>

#include "w32-imeadv.h"

#define W32_IMEADV_RECONVERTSTRING "W32_IMEADV_RECONVERTSTRING"

template<UINT WaitMessage , DWORD dwTimeOutMillSecond = 5000 /* 5 sec timout (for safity) */> static inline BOOL 
my_wait_message( HWND hWnd );

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

namespace w32_imeadv_on_ui_thread{
  namespace ime_request{
    static LRESULT
    w32_imeadv_imr_reconvertstring( HWND hWnd , LPARAM lParam );
    
    static LRESULT 
    w32_imeadv_imr_reconvertstring( HWND hWnd , std::nullptr_t&&  );
  };
};

struct my_reconversion_store{      // 後で使う
  size_t size;                // メモリブロックのサイズ
  std::unique_ptr<BYTE[]> memory;               // メモリブロックへのポインタ
  /* memory が指し示すメモリブロックのレイアウトは、
     struct{
     RECONVERTSTRING ,
     // (nopadding)
     wchar_t[] 
     }; 
     である。  */
};

static LRESULT
w32_imeadv_on_ui_thread::ime_request::w32_imeadv_imr_reconvertstring( HWND hWnd , LPARAM lParam )
{
  
  assert( hWnd ); /* hWnd != NULL は呼び出し元の責任 */

  if( !lParam ){
    /* これは本来 呼び出し元の責任 */
    return w32_imeadv_imr_reconvertstring( hWnd , nullptr );
  }

  assert( lParam );

  my_reconversion_store* reconv_store = 
    reinterpret_cast<my_reconversion_store*>(GetProp( hWnd , W32_IMEADV_RECONVERTSTRING ));
  
  if( reconv_store ){
    
    RECONVERTSTRING * const src = reinterpret_cast<RECONVERTSTRING*>( reconv_store->memory.get() );
    RECONVERTSTRING * const reconvertstring = reinterpret_cast<RECONVERTSTRING *>( lParam );
    if( reconv_store->size <= reconvertstring->dwSize ){
      reconvertstring->dwVersion         = 0;
      reconvertstring->dwStrLen          = src->dwStrLen;
      // IMEから渡された dwStrOffset があるならそれを使う、0 ならば、再設定する。
      if(!( sizeof( RECONVERTSTRING ) <= reconvertstring->dwStrOffset )){
        reconvertstring->dwStrOffset     = src->dwStrOffset;
      }
      reconvertstring->dwCompStrLen      = src->dwCompStrLen;
      reconvertstring->dwCompStrOffset   = src->dwCompStrOffset;
      reconvertstring->dwTargetStrLen    = src->dwTargetStrLen;
      reconvertstring->dwTargetStrOffset = src->dwTargetStrOffset;

      {
        std::wstringstream out{};
        out << "reconv_store->memory.get() = " << reconv_store->memory.get() << ","
            << "src->dwStrOffset = " << src->dwStrOffset << ","
            << "src->dwStrLen = " << src->dwStrLen << ","
            << "lParam = " << lParam << ","
            << "reconvertstring->dwStrOffset = " << reconvertstring->dwStrOffset ;
        out << DEBUG_STRING(" std::copy call") << std::endl;
        OutputDebugStringW( out.str().c_str() );
      }
      // 書き込む前にチェックしよう
      // 下のassert 文を上に持ってくる
      
      auto terminate_pos = 
        std::copy( reinterpret_cast<wchar_t*>(( reconv_store->memory.get() + src->dwStrOffset )),
                   reinterpret_cast<wchar_t*>(( reconv_store->memory.get() + src->dwStrOffset )) + src->dwStrLen,
                   reinterpret_cast<wchar_t*>(reinterpret_cast<BYTE*>( lParam ) + reconvertstring->dwStrOffset) );
      *(terminate_pos++) = L'\0';

      assert( (reinterpret_cast<BYTE*>( terminate_pos )) <=
              (reinterpret_cast<BYTE*>( lParam ) + reconvertstring->dwSize ) );
      
      return reconvertstring->dwSize;
    }
  }
  return 0;
}

static LRESULT
w32_imeadv_on_ui_thread::ime_request::w32_imeadv_imr_reconvertstring( HWND hWnd, std::nullptr_t&& )
{
  HWND communication_window_handle = reinterpret_cast<HWND>( GetProp( hWnd , "W32_IMM32ADV_COMWIN" ));
  if( communication_window_handle ){

    struct SubclassRefData {
      BOOL process;
      DWORD dwComStrLen;     // composition string length (in byte unit)
      DWORD dwComStrOffset;  // composition string offset (in byte unit)
      std::wstring text;
    } refData = {};

    /*
      これから、Window を一時的にサブクラス化して、 WM_W32_IMEADV_NOTIFY_RECONVERSION_STRING フィルタ関数として待機する。
      @param dwRefData 呼び出し元がスタック上に準備した、 SubclassRefData* 
      サブクラス化の解除は、呼び出し元が行う
    */
    SUBCLASSPROC const subclass_proc =
      []( HWND hWnd , UINT uMsg , WPARAM wParam , LPARAM lParam ,
          UINT_PTR , DWORD_PTR dwRefData )->LRESULT{

        assert( dwRefData );
        
        if( WM_W32_IMEADV_NOTIFY_RECONVERSION_STRING == uMsg ){
          DebugOutputStatic( "subclass_proc hooks in IMR_RECONVERTSTRING , uMsg is WM_W32_IMEADV_NOTIFY_RECONVERSION_STRING" );
          using imeadv::NotifyReconversionString;
          if( (static_cast<bool>(lParam)) && (static_cast<bool>(dwRefData)) ){
            const NotifyReconversionString* const nrs =
              reinterpret_cast<const NotifyReconversionString*>( lParam );
            SubclassRefData *refData = reinterpret_cast<SubclassRefData*>( dwRefData ) ;
            assert( nrs );
            assert( refData );
            refData->dwComStrLen = 0;
            refData->dwComStrOffset = static_cast<DWORD>(nrs->first_half.size() * sizeof( wchar_t ));
            refData->text = nrs->first_half + nrs->later_half;
            {
              std::wstringstream out;
              out << "\"" << nrs->first_half << "|" << nrs->later_half << "\""
                  << DEBUG_STRING(L" ") << std::endl;
              OutputDebugStringW( out.str().c_str() );
            }
            refData->process = 1;
          }
        }
        return ::DefSubclassProc( hWnd , uMsg , wParam , lParam );
      };
    
    if( ::SetWindowSubclass( hWnd , subclass_proc ,
                             reinterpret_cast<UINT_PTR>(subclass_proc),
                             reinterpret_cast<DWORD_PTR>(&refData) ) ) {
      struct SubclassRAII{
        const HWND hWnd ;
        const SUBCLASSPROC subclass_proc;
        ~SubclassRAII(){
          VERIFY( ::RemoveWindowSubclass( hWnd, subclass_proc , reinterpret_cast<UINT_PTR>(subclass_proc) ) );
        }
      } subclass_raii = { hWnd , subclass_proc} ;

      if( SendMessageW( communication_window_handle , WM_W32_IMEADV_REQUEST_RECONVERSION_STRING ,
                        reinterpret_cast<WPARAM>(hWnd) , 0 ) ){
        // Wait Conversion Message
        DebugOutputStatic( "IMR_RECONVERTSTRING waiting message" );
        my_wait_message<WM_W32_IMEADV_NOTIFY_RECONVERSION_STRING>( hWnd );
        
        if( refData.process ){
          {
            std::wstringstream out{};
            out << L"subclass process WM_W32_IMEADV_NOTIFY_RECONVERSION_STRING "
                << L"request size is " << std::to_wstring( sizeof( RECONVERTSTRING ) +
                                                           (sizeof(wchar_t) * (refData.text.size() + 1)) )
                << L" = ( (sizeof( RECONVERTSTRING ) =" << sizeof( RECONVERTSTRING ) << L") + ("
                << (sizeof(wchar_t) * (refData.text.size() + 1)) << L") )"
                << DEBUG_STRING(L" ")  << std::endl;
            OutputDebugStringW( out.str().c_str() );
          }
          
          
          if( refData.text.empty() ){ // 再変換対象となる文字列がないよ
            return 0;
          }
          assert( 0 < refData.text.size() );
          
          std::size_t const memory_block_size =
            { sizeof( RECONVERTSTRING ) +
              ( sizeof(wchar_t) * (refData.text.size() + ( 1 /* null terminater */ )) ) }; 

          assert( memory_block_size > sizeof( RECONVERTSTRING ) );

          std::unique_ptr<BYTE[]> memory_block{ new (std::nothrow) BYTE[ memory_block_size ]{} };
          
          if( memory_block ){
            /* zero fill */
            std::fill( memory_block.get() , memory_block.get() + memory_block_size , 0 );
            /* メモリブロックから、各構造体のポインタを切り出す */
            RECONVERTSTRING * const reconv = reinterpret_cast<RECONVERTSTRING*>(memory_block.get());
            wchar_t * const text = reinterpret_cast<wchar_t*>( memory_block.get() + sizeof( RECONVERTSTRING ) );
            {
              assert( text );
              {
                const size_t size{ refData.text.size () };
                const wchar_t* const ch_ptr{refData.text.c_str()};
                wchar_t* dst = text;
                std::for_each( ch_ptr , ch_ptr+size ,
                               [&dst](const wchar_t &c){
                                 *(dst++) = c;
                               } );
                *(dst++) = L'\0';
                assert( ( memory_block.get() + memory_block_size ) == reinterpret_cast<BYTE*>( dst ) );
              }
              assert( reconv );
              reconv->dwSize            = memory_block_size;
              reconv->dwVersion         = 0;
              reconv->dwStrLen          = refData.text.size();
              reconv->dwStrOffset       = sizeof( RECONVERTSTRING ); //  byte unit
              reconv->dwCompStrLen      = 0;
              reconv->dwCompStrOffset   = refData.dwComStrOffset;    // in byte unit
              reconv->dwTargetStrLen    = 0;
              reconv->dwTargetStrOffset = refData.dwComStrOffset;    // in byte unit
            }
            // RECONVERTSTRING が用意できたので 、ImmSetCompositionStringW を呼び出す
            HIMC hImc = ImmGetContext( hWnd );
            if( hImc ){
              struct HIMC_RAII{
                const HWND hWnd;
                const HIMC hImc;
                ~HIMC_RAII(){
                  if( hWnd && hImc ){
                    VERIFY( ImmReleaseContext( hWnd, hImc ) );
                  }
                }
              } himc_raii = { hWnd, hImc };

              // カーソル位置の調整を準備する 
              DWORD const cursor_pos_in_byte = reconv->dwCompStrOffset; 
              
              // IME に再変換領域の調整をお願いする
              if( ImmSetCompositionStringW( hImc ,
                                            SCS_QUERYRECONVERTSTRING ,
                                            reinterpret_cast<LPVOID>(memory_block.get()),
                                            reconv->dwSize ,
                                            nullptr ,
                                            0 ) ){
                // カーソルの位置を IMEで指定された再変換領域の位置に合わせる
                if(  (0 == ( cursor_pos_in_byte % sizeof( wchar_t )))
                     && (0 == (reconv->dwCompStrOffset % sizeof( wchar_t ))) ){
                  if( reconv->dwCompStrOffset < cursor_pos_in_byte ){
                    const wchar_t * const begin =
                      reinterpret_cast<const wchar_t*>((reinterpret_cast<const BYTE*>( text ) + reconv->dwCompStrOffset ));
                    const wchar_t * const end =
                      reinterpret_cast<const wchar_t*>((reinterpret_cast<const BYTE*>( text ) + cursor_pos_in_byte));
                    size_t nCount = 0;
                    for(const wchar_t* p = begin; p < end ; ++p , ++nCount){
                      if( 0xd800 <= *p && *p < 0xdc00 ){ // high surrogate 
                        ++p;
                      }
                    }
                    { 
                      w32_imeadv_request_backward_char_lparam req_param{ hWnd , nCount };
                      VERIFY( ((LRESULT)nCount) == 
                              (SendMessageW( communication_window_handle,
                                             WM_W32_IMEADV_REQUEST_BACKWARD_CHAR ,
                                             reinterpret_cast<WPARAM>( hWnd ),
                                             reinterpret_cast<LPARAM>( &req_param ) ) ) );
                    }
                  }
                }

                std::unique_ptr<my_reconversion_store> reconv_store{new (std::nothrow) my_reconversion_store()};
                if( reconv_store ){
                  reconv_store->size = memory_block_size;
                  reconv_store->memory = std::move( memory_block );
                  {
                    std::wstringstream out{};
                    out << reconv_store.get() << DEBUG_STRING(" handle") << std::endl;
                    OutputDebugStringW( out.str().c_str() );
                  }
                  
                  if( SetProp( hWnd , TEXT(W32_IMEADV_RECONVERTSTRING),
                               static_cast<HANDLE>( reconv_store.get() ) ) ){
                    reconv_store.release();
                    return reconv->dwSize;
                  }
                }
                
              }else{
                DebugOutputStatic( "ImmSetCompositionStringW SCS_QUERYRECONVERTSTRING failed" );
              }
            } // end of if (hIMC) 
          }
        }
      }
    } // end of if ::SetWindowSubclass
  } // end of if( communication_window_handle )
  return 0;
}


template<UINT WaitMessage , DWORD dwTimeOutMillSecond> static inline BOOL 
my_wait_message( HWND hWnd )
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
          *ptr = 0;
        }
        return ::DefSubclassProc( hWnd, uMsg , wParam , lParam );
      };
  UINT_PTR const uIdSubclass = reinterpret_cast<UINT_PTR const >( subclass_proc );

  /* not throw std::bad_alloc in this case  */
  std::unique_ptr< DWORD > waiting_data{(new ( std::nothrow ) DWORD{1})}; 
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
    OutputDebugStringA("w32_imeadv_wm_ime_startcomposition effective\n");

    // ここでフォントの要求をする
    HWND communication_window_handle = reinterpret_cast<HWND>( GetProp( hWnd , "W32_IMM32ADV_COMWIN" ));
    if( communication_window_handle ){
      if( SendMessageW( communication_window_handle , WM_W32_IMEADV_REQUEST_COMPOSITION_FONT ,
                        reinterpret_cast<WPARAM>(hWnd) ,lParam ) ){
        // Wait Conversion Message
        DebugOutputStatic( "IMR_COMPOSITIONFONT waiting message" );
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
      { // 再変換の後始末
        HANDLE handle = GetProp( hWnd , W32_IMEADV_RECONVERTSTRING );
        if( handle ){ 
          std::unique_ptr<my_reconversion_store> reconv_store{};
          reconv_store.reset( reinterpret_cast<my_reconversion_store*>( handle ) );
          // detach window property.
          VERIFY( handle == RemoveProp( hWnd ,TEXT( W32_IMEADV_RECONVERTSTRING ) ) );
          const BYTE* const memory_block = reconv_store->memory.get();
          if( memory_block ){
            const RECONVERTSTRING* const reconv{reinterpret_cast<const RECONVERTSTRING*>( memory_block )};
            if( 0 < reconv->dwCompStrLen ){
              const wchar_t* const begin_comp =
                reinterpret_cast<const wchar_t*>( memory_block + ( reconv->dwStrOffset + reconv->dwCompStrOffset ));
              const wchar_t* const end_comp =
                reinterpret_cast<const wchar_t*>( memory_block + ( reconv->dwStrOffset + reconv->dwCompStrOffset ))
                + reconv->dwCompStrLen;
              assert( begin_comp < end_comp );
              assert( reinterpret_cast<const BYTE*>( end_comp ) - memory_block < reconv->dwSize );
              if( reinterpret_cast<const BYTE*>( end_comp ) - memory_block < reconv->dwSize ){
                size_t nCount = 0;
                for(const wchar_t *p = begin_comp ; p < end_comp ; ++p,++nCount ){
                  if( 0xd800 <= *p && *p < 0xdc00 ){ // high surrogate 
                    ++p;
                  }
                }
                if( 0 < nCount ){
                  w32_imeadv_request_delete_char_lparam w32_imeadv_request_delete_char = { hWnd , nCount };
                  HWND communication_window_handle = reinterpret_cast<HWND>( GetProp( hWnd , "W32_IMM32ADV_COMWIN" ));
                  if( communication_window_handle ){
                    VERIFY( static_cast<LPARAM>(nCount) ==
                            SendMessage( communication_window_handle , WM_W32_IMEADV_REQUEST_DELETE_CHAR ,
                                         reinterpret_cast<WPARAM>(hWnd),
                                         reinterpret_cast<LPARAM>(&w32_imeadv_request_delete_char) ) );
                  }
                }
              }
            }
          }
        }
      }

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
          ImmReleaseContext( hWnd , hImc );
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
                PostMessageW( communication_window_handle , WM_W32_IMEADV_OPENSTATUS_OPEN , 0 , 0 );
              else
                PostMessageW( communication_window_handle , WM_W32_IMEADV_OPENSTATUS_CLOSE , 0 , 0 );
              ImmReleaseContext( hWnd , hImc );
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
      DebugOutputStatic( "w32_imeadv_wm_ime_request -> IMR_DOCUMENTFEED" );
      HWND communication_window_handle = reinterpret_cast<HWND>( GetProp( hWnd , "W32_IMM32ADV_COMWIN" ));
      if( communication_window_handle ){
        
        if( SendMessageW( communication_window_handle , WM_W32_IMEADV_REQUEST_DOCUMENTFEED_STRING ,
                          reinterpret_cast<WPARAM>(hWnd) , 0 ) ){
          // Wait Conversion Message
          my_wait_message<WM_W32_IMEADV_NOTIFY_DOCUMENTFEED_STRING>( hWnd );
          return 0;
        }
      }
      break;
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
      if( lParam )
        return w32_imeadv_on_ui_thread::ime_request::w32_imeadv_imr_reconvertstring( hWnd, lParam );
      else
        return w32_imeadv_on_ui_thread::ime_request::w32_imeadv_imr_reconvertstring( hWnd, nullptr );
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
    ImmSetOpenStatus( hImc , TRUE );
    ImmReleaseContext( hWnd, hImc );
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
    ImmSetOpenStatus( hImc, FALSE );
    ImmReleaseContext( hWnd, hImc );
    return 1;
  }
  return 0;
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
  case WM_W32_IMEADV_OPENSTATUS_OPEN:
    return w32_imeadv_openstatus_open( hWnd , wParam , lParam );
  case WM_W32_IMEADV_OPENSTATUS_CLOSE:
    return w32_imeadv_openstatus_close( hWnd , wParam , lParam );
  case WM_W32_IMEADV_NOTIFY_COMPOSITION_FONT:
    {
      OutputDebugString( "WM_W32_IMEADV_NOTIFY_COMPOSITION_FONT recieve and consume\n" );
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
            ImmReleaseContext( hWnd, hImc );
          }
        }
        std::wstringstream out{};
        out << *font_configure;
        out << __FILE__ << "@L." << __LINE__ << " " ;
        out << std::endl;
        OutputDebugStringW( out.str().c_str() );
      }
    }
    return 1;
  case WM_W32_IMEADV_NOTIFY_RECONVERSION_STRING:
    DebugOutputStatic( "consume WM_W32_IMEADV_NOTIFY_RECONVERSION_STRING message" );
    return 1;
  case WM_W32_IMEADV_NOTIFY_DOCUMENTFEED_STRING:
    DebugOutputStatic( "consume WM_W32_IMEADV_NOTIFY_DOCUMENTFEED_STRING message\n" );
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
          ImmReleaseContext( hWnd , hImc );
        }
      
      HWND communication_window_handle = (HWND)(wParam);
      SetProp( hWnd , "W32_IMM32ADV_COMWIN" , (communication_window_handle ) );
      PostMessageW( communication_window_handle , WM_W32_IMEADV_SUBCLASSIFY , (WPARAM)( hWnd ) , 0);
      return 1;
    }
  else if( WM_W32_IMEADV_UNSUBCLASSIFY == uMsg )
    {
      HWND communication_window_handle = reinterpret_cast<HWND>( GetProp( hWnd , "W32_IMM32ADV_COMWIN" ) );
      if( communication_window_handle ){
        RemoveProp( hWnd , "W32_IMM32ADV_COMWIN" );
        PostMessageW( communication_window_handle , WM_W32_IMEADV_UNSUBCLASSIFY , (WPARAM)(hWnd) , 0 );
      }
      return ::RemoveWindowSubclass( hWnd , subclass_proc , uIdSubclass);
    }
  
  return DefSubclassProc (hWnd, uMsg , wParam , lParam );
}
