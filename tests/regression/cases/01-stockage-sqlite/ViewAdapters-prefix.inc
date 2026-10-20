
#include "FolderStateStore.h"
#include <cstring>
#include <algorithm>
#include <iostream>
#include <stdexcept>
using HRESULT=int;using DWORD=uint32_t;using UINT=uint32_t;using BOOL=int;
#define SUCCEEDED(x) ((x)>=0)
#define FAILED(x) ((x)<0)
bool checkHR(HRESULT x){return x>=0;}
constexpr int S_OK=0,S_FALSE=1,FALSE=0,CM_ENUM_VISIBLE=1,CM_MASK_WIDTH=1,FWF_AUTOARRANGE=1,FWF_SNAPTOGRID=4;
enum FOLDERVIEWMODE{FVM_DETAILS=4};enum SORTDIRECTION{SORT_DESCENDING=-1,SORT_ASCENDING=1};
struct PROPERTYKEY { uint8_t fmtid[16];uint32_t pid; };
struct SORTCOLUMN { PROPERTYKEY propkey;SORTDIRECTION direction; };
struct CM_COLUMNINFO { size_t cbSize;UINT dwMask;UINT uWidth=0; };
int invScaleDPI(int value){return value*2/3;}int scaleDPI(int value){return value*3/2;}
using namespace filespacer;
PROPERTYKEY fromKey(const FolderViewSettings::Key&k){PROPERTYKEY p{};std::memcpy(&p,k.data(),20);return p;}
FolderViewSettings::Key toKey(const PROPERTYKEY&p){FolderViewSettings::Key k{};std::memcpy(k.data(),&p,20);return k;}
struct IColumnManager{
 std::vector<PROPERTYKEY>keys;std::vector<UINT>widths;bool fail=false;
 HRESULT GetColumnCount(UINT,UINT*out){*out=static_cast<UINT>(keys.size());return fail?-1:0;}
 HRESULT GetColumns(UINT,PROPERTYKEY*out,UINT n){std::copy_n(keys.data(),n,out);return fail?-1:0;}
 HRESULT GetColumnInfo(const PROPERTYKEY&key,CM_COLUMNINFO*out){for(size_t i=0;i<keys.size();++i)if(toKey(key)==toKey(keys[i])){out->uWidth=widths[i];return 0;}return -1;}
 HRESULT SetColumns(const PROPERTYKEY*in,UINT n){keys.assign(in,in+n);widths.resize(n);return 0;}
 HRESULT SetColumnInfo(const PROPERTYKEY&key,CM_COLUMNINFO*in){for(size_t i=0;i<keys.size();++i)if(toKey(key)==toKey(keys[i])){widths[i]=in->uWidth;return 0;}return -1;}
};
struct IFolderView2{
 IColumnManager manager;FOLDERVIEWMODE mode=FVM_DETAILS;int iconSize=32;DWORD flags=5;PROPERTYKEY group={};BOOL ascending=1;bool grouped=false,failSort=false;std::vector<SORTCOLUMN>sort;
 HRESULT GetViewModeAndIconSize(FOLDERVIEWMODE*m,int*i){*m=mode;*i=iconSize;return 0;}
 HRESULT GetCurrentFolderFlags(DWORD*out){*out=flags;return 0;}
 HRESULT GetSortColumnCount(int*out){*out=static_cast<int>(sort.size());return failSort?-1:0;}
 HRESULT GetSortColumns(SORTCOLUMN*out,int n){std::copy_n(sort.data(),n,out);return 0;}
 HRESULT GetGroupBy(PROPERTYKEY*out,BOOL*a){*out=group;*a=ascending;return grouped?0:S_FALSE;}
 HRESULT SetViewModeAndIconSize(FOLDERVIEWMODE m,int i){mode=m;iconSize=i;return 0;}
 HRESULT SetCurrentFolderFlags(DWORD mask,DWORD value){flags=(flags&~mask)|(value&mask);return 0;}
 HRESULT SetSortColumns(const SORTCOLUMN*in,int n){sort.assign(in,in+n);return 0;}
 HRESULT SetGroupBy(const PROPERTYKEY&key,BOOL a){group=key;ascending=a;return 0;}
};
template<class T>struct CComQIPtr{T*p;CComQIPtr(IFolderView2*v):p(&v->manager){};T*operator->(){return p;}operator bool(){return p!=nullptr;}};
void require(bool condition,const char*message){if(!condition)throw std::runtime_error(message);}
