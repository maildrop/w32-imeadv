#include <tchar.h>
#include <windows.h>
#include <iostream>
#include <sstream>
#include <string>
#include <array>
#include <cassert>
#include <tuple>

#include "emacs-module.h"
#include "w32-imeadv.h"

extern "C"{
__declspec( dllexport ) void CALLBACK
EntryPoint( HWND hWnd , HINSTANCE hInst , LPSTR lpszCmdLine , int nCmdShow );
};

__declspec( dllexport ) void CALLBACK
EntryPoint( HWND hWnd , HINSTANCE hInst , LPSTR lpszCmdLine , int nCmdShow )
{
  std::ignore = hWnd ;
  std::ignore = hInst;
  std::ignore = nCmdShow;
  if( ::MessageBox( hWnd , TEXT("Rundll32.exe and example.dll test") , TEXT("example.dll") , MB_YESNO ) ){
    std::cout << "MB_OK" << std::endl;
  }
  return;
}
