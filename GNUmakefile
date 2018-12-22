
ifdef DEBUG
CPPFLAGS:=-DWINVER=_WIN32_WINNT_WINXP -D_WIN32_WINNT=_WIN32_WINNT_WINXP
CXXFLAGS:=-std=c++11 -O3 -Wall -Werror -g 
else
CPPFLAGS:=-DWINVER=_WIN32_WINNT_WINXP -D_WIN32_WINNT=_WIN32_WINNT_WINXP -DNDEBUG=1
CXXFLAGS:=-std=c++11 -O3 -Wall -Werror
endif

RM:=rm

W32_IMEADV_DLL_OBJS:=w32-imeadv.o w32-imeadv-on-lispy-thread.o w32-imeadv-on-ui-thread.o w32-imeadv-rundll32.o
all_TARGET:=w32-imeadv.dll

all: $(all_TARGET)

w32-imeadv.dll: $(W32_IMEADV_DLL_OBJS)
	$(CXX) --shared -o $@ $(CXXFLAGS) $(CPPFLAGS) -Wl,-Map=$@.map $+ -limm32 -lComctl32 -lgdi32 -lUser32 
emacs-imm32-input-proxy.exe: $(emacs-imm32-input-proxy_OBJS)
	$(CXX) -o $@ $(CXXFLAGS) $(CPPFLAGS) $+ -lUser32

w32-imeadv.o: w32-imeadv.cpp emacs-module.h w32-imeadv.h w32-imeadv-on-lispy-thread.h 

w32-imeadv-on-lispy-thread.o: w32-imeadv-on-lispy-thread.cpp w32-imeadv.h w32-imeadv-on-lispy-thread.h emacs-module.h 

w32-imeadv-on-ui-thread.o: w32-imeadv-on-ui-thread.cpp w32-imeadv.h 

w32-imeadv-rundll32.o: w32-imeadv-rundll32.cpp w32-imeadv.h

clean:
	$(RM) -f $(all_TARGET) $(W32_IMEADV_DLL_OBJS) *~
