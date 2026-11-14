#pragma once
namespace rpd_arrest {
// Native activity teardown can clear AIPursuit before the next render tick.
// Retain only a real native arrest, bound to this race and this suspect.
struct Receipt {
 unsigned race=0,key=0,epoch=0,suspect=0;bool arrested=false;
 void Reset(){*this={};}
 void Observe(unsigned r,unsigned k,unsigned e,unsigned s,bool busted,bool pursuitArrested){
  if(r!=race||k!=key||e!=epoch||s!=suspect){Reset();race=r;key=k;epoch=e;suspect=s;}
  if(r&&k&&s&&(busted||pursuitArrested))arrested=true;
 }
 bool Matches(unsigned r,unsigned k,unsigned e,unsigned s)const{return arrested&&r&&k&&s&&race==r&&key==k&&epoch==e&&suspect==s;}
};
}
