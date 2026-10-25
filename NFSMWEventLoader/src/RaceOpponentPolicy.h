#pragma once
namespace mwel_opponents {
// Ordinary extra grid slots respect the installed game's random-opponent
// eligibility. Fixed native/story characters and calibrated NFRR selections
// have their own ownership and are not governed by this fallback pool.
inline bool OrdinaryCandidate(bool racer,bool installed,unsigned randomOpponent){return racer&&installed&&randomOpponent==1;}
// A custom event uses the live game's racer inheritance, not model-name
// prefixes or a fixed list of stock cars. Traffic can have RandomOpponent=1.
template<class Node,class Parent,class Key,class Class>
bool RacerDescendant(Node* node,unsigned racers,Parent parent,Key key,Class cls){
 if(!node)return false;auto owner=cls(node);
 for(unsigned depth=0;node&&depth<32;++depth){if(cls(node)!=owner)return false;if(key(node)==racers)return depth>0;node=parent(node);}return false;
}
}
