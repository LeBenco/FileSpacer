
int main(){
 try{
  IFolderView2 original;
  PROPERTYKEY name{},size{};name.pid=10;size.pid=12;size.fmtid[0]=19;
  original.manager.keys={name,size};original.manager.widths={480,150};original.sort={{size,SORT_DESCENDING}};
  auto state=captureOptions(&original,{});
  require(state.present==31 && state.columns.size()==2 && state.columns[0].width==320 && state.columns[1].width==100,"capture/96DPI widths");
  require(state.groupBy==FolderViewSettings::Key{},"S_FALSE must store no grouping");
  IFolderView2 restored;restoreOptions(&restored,state);
  require(restored.manager.widths==original.manager.widths && restored.sort[0].direction==SORT_DESCENDING && toKey(restored.sort[0].propkey)==toKey(size),"native restore");
  require(sameView(state,captureOptions(&restored,{})),"unchanged view produces writes");
  original.manager.fail=true;original.failSort=true;
  auto unsupported=captureOptions(&original,state);
  require(unsupported.columns[0].width==320 && unsupported.sort[0].direction==-1,"unsupported getters erased previous data");
  original.grouped=true;original.group=size;original.ascending=0;
  auto grouped=captureOptions(&original,state);restoreOptions(&restored,grouped);
  require(toKey(restored.group)==toKey(size)&&!restored.ascending,"group key/direction");
  for(unsigned i=0;i<20;++i){auto changed=state;changed.columns[0].key[i]^=1;require(!sameView(state,changed),"column key equality");}
  std::cout<<"PASS: extracted native capture/restore adapters, DPI, grouping, unsupported properties, change detection.\n";
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
}
