CPPFLAGS:=-DWINVER=_WIN32_WINNT_WINXP -D_WIN32_WINNT=_WIN32_WINNT_WINXP -DCHECK_LISP_OBJECT_TYPE=1
CXXFLAGS:=-std=c++11 -O3 -g -Wall
W32_IMEADV_DLL_OBJS:=w32-imeadv.o w32-imeadv-on-lispy-thread.o w32-imeadv-on-ui-thread.o
emacs-imm32-input-proxy_OBJS:= emacs-imm32-input-proxy.o
all_TARGET:= w32-imeadv.dll emacs-imm32-input-proxy.exe

all: $(all_TARGET)

w32-imeadv.dll: $(W32_IMEADV_DLL_OBJS)
	g++ --shared -o $@ $(CXXFLAGS) $(CPPFLAGS) $+ -limm32 -lComctl32 -lUser32
emacs-imm32-input-proxy.exe: $(emacs-imm32-input-proxy_OBJS)
	g++ -o $@ $(CXXFLAGS) $(CPPFLAGS) $+ -lUser32

w32-imeadv.o: w32-imeadv.cpp emacs-module.h w32-imeadv.h w32-imeadv-on-lispy-thread.h 

w32-imeadv-on-lispy-thread.o: w32-imeadv-on-lispy-thread.cpp w32-imeadv.h w32-imeadv-on-lispy-thread.h emacs-module.h 

w32-imeadv-on-ui-thread.o: w32-imeadv-on-ui-thread.cpp w32-imeadv.h 

emacs-imm32-input-proxy.o: emacs-imm32-input-proxy.cpp w32-imeadv.h

clean:
	rm -f $(all_TARGET) $(W32_IMEADV_DLL_OBJS) $(emacs-imm32-input-proxy_OBJS) *~
