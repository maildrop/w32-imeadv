CPPFLAGS:=-DWINVER=_WIN32_WINNT_WINXP -D_WIN32_WINNT=_WIN32_WINNT_WINXP
CXXFLAGS:=-std=c++11 -O3 -g -Wall

W32_IMEADV_DLL_OBJS:=w32-imeadv.o w32-imeadv-on-lispy-thread.o w32-imeadv-on-ui-thread.o

w32-imeadv.dll: $(W32_IMEADV_DLL_OBJS)
	g++ --shared -o $@ $(CXXFLAGS) $(CPPFLAGS) $+ -limm32 -lComctl32 -lUser32

w32-imeadv.o: w32-imeadv.cpp w32-imeadv.h emacs-module.h 

w32-imeadv-on-lispy-thread.o: w32-imeadv-on-lispy-thread.cpp w32-imeadv.h

w32-imeadv-on-ui-thread.o: w32-imeadv-on-ui-thread.cpp w32-imeadv.h

clean:
	rm -f w32-imeadv.dll $(W32_IMEADV_DLL_OBJS) *~
