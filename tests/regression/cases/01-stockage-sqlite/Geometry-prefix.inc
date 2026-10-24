
#include "FolderStateStore.h"
#include <iostream>
#include <stdexcept>
using namespace filespacer;
#define checkLE(x) (x)
constexpr int FALSE=0,GWL_EXSTYLE=0,WS_EX_TOOLWINDOW=0x80,MONITOR_DEFAULTTONEAREST=2;
struct RECT{int left,top,right,bottom;};struct SIZE{int cx,cy;};
struct WINDOWPLACEMENT{unsigned length;RECT rcNormalPosition;};
struct MONITORINFO{unsigned cbSize;RECT rcWork,rcMonitor;};
RECT native{100,200,740,680},normal=native,work{0,40,1920,1080},mockMonitorRect{0,0,1920,1080};
bool minimized=false,mockZoomed=false,tool=false,fail=false;
bool IsIconic(int){return minimized;}bool IsZoomed(int){return mockZoomed;}
bool GetWindowRect(int,RECT*r){*r=native;return !fail;}
bool GetWindowPlacement(int,WINDOWPLACEMENT*p){p->rcNormalPosition=normal;return !fail;}
int GetWindowLongPtr(int,int){return tool?WS_EX_TOOLWINDOW:0;}
int MonitorFromWindow(int,int){return 0;}
bool GetMonitorInfo(int,MONITORINFO*p){p->rcWork=work;p->rcMonitor=mockMonitorRect;return !fail;}
void OffsetRect(RECT*r,int x,int y){r->left+=x;r->right+=x;r->top+=y;r->bottom+=y;}
int rectWidth(RECT r){return r.right-r.left;}int rectHeight(RECT r){return r.bottom-r.top;}
SIZE rectSize(RECT r){return {rectWidth(r),rectHeight(r)};}int invScaleDPI(int v){return v;}
struct ItemWindow{
 int hwnd=1;RECT lastNormalRect{};bool geometryKnown=false,lastMaximized=false;unsigned dirtyViewState=0;FolderState savedState;
 bool normalWindowRect(RECT*)const;void recordGeometry();uint32_t captureViewState(uint32_t);
 void viewStateDirty(uint32_t mask){dirtyViewState|=mask;}
};
void require(bool value,const char*message){if(!value)throw std::runtime_error(message);}
