#pragma once
#include "RpdControlBindings.h"
bool MWELRpdControlsScope();
namespace rpd_controls {
mwel_rpd_controls::Lease lease;unsigned owner=0;
bool Read(mwel_rpd_controls::Rows& rows,mwel_rpd_controls::InputTable& inputTable){
 const BYTE guard[]={0x53,0x56,0x57,0x8B,0x7C,0x24,0x14};
 return !memcmp(reinterpret_cast<void*>(0x628230),guard,sizeof(guard))&&
  Copy(reinterpret_cast<void*>(0x91F418),&rows,sizeof(rows))&&Copy(reinterpret_cast<void*>(0x91F150),&inputTable,sizeof(inputTable));
}
void Capture(){
 mwel_rpd_controls::Rows rows;mwel_rpd_controls::InputTable inputTable;owner=0;
 if(!Read(rows,inputTable)){lease.Clear();return;}
 const bool held=lease.Capture(rows,inputTable);
 if(worldLogEnabled)Log("RPD_CONTROL_MAP_CAPTURE native_input_available_and_padmap_saved=%u native_driving_actions=19 keyboard_and_menu_untouched=1",unsigned(held));
}
void Forget(){lease.Clear();owner=0;}
void Restore(const char* boundary){
 if(!lease.held)return;
 if(!MWELRpdControlsScope()||!owner||owner!=MWELNeoCurrentKey()){Forget();return;}
 mwel_rpd_controls::Rows rows;mwel_rpd_controls::InputTable inputTable;if(!Read(rows,inputTable)){Forget();return;}
 const int changed=lease.Restore(rows,inputTable);
 if(changed>0){
  // Use the same mutable binding table as the native controls screen. Copy
  // only pad triples; never restore a profile, defaults, keyboard or menu map.
  for(unsigned i=0;i<rows.size();++i){auto live=reinterpret_cast<mwel_rpd_controls::Row*>(0x91F418+i*sizeof(mwel_rpd_controls::Row));
   live->padPrimary=rows[i].padPrimary;live->padSecondary=rows[i].padSecondary;
  }
 }
 if(worldLogEnabled)Log("RPD_CONTROL_MAP_RESTORE boundary=%s changed_bindings=%d same_native_input_table_required=1",boundary,changed);
}
void BeginTransition(){if(!lease.held)Capture();owner=MWELNeoCurrentKey();}
void Stage(){if(!MWELRpdControlsScope()){Forget();return;}BeginTransition();Restore("stage_before");}
}
