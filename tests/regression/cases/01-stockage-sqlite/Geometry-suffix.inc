
int main(){try{
 ItemWindow w;w.recordGeometry();require(!w.dirtyViewState,"initial geometry dirty");
 native={124,224,740,680};w.recordGeometry();require(w.dirtyViewState==3,"top/left resize");
 for(unsigned mask=0;mask<32;++mask){auto c=w.captureViewState(mask);require(c==(mask&(FolderState::Position|FolderState::Size|FolderState::Maximized)),"capture mask");}
 require(w.savedState.x==124&&w.savedState.y==224&&w.savedState.width==616&&w.savedState.height==456,"geometry capture");
 normal={124,184,740,640};native={-32000,-32000,-31900,-31900};
 for(int mode=0;mode<2;++mode){minimized=mode==0;mockZoomed=mode==1;w.dirtyViewState=0;w.recordGeometry();require(w.dirtyViewState==(mode==1?FolderState::Maximized:0),"min/max changed unexpected state");w.captureViewState(3);require(w.savedState.x==124&&w.savedState.y==224&&w.savedState.width==616,"normal rectangle save");}
 w.captureViewState(FolderState::Maximized);require(w.savedState.maximized,"maximized state not captured");
 fail=true;require(w.captureViewState(3)==0,"failed geometry was captured");
 fail=false;minimized=mockZoomed=false;native={-72000,68000,-71000,68600};w.captureViewState(3);require(w.savedState.x==-72000&&w.savedState.y==68000,"32bit coordinates truncated");
 std::cout<<"PASS: extracted geometry, top/left resize, minimized/maximized, failed capture, 32-bit coordinates.\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
