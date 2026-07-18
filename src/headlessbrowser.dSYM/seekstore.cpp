#include<iostream>
using namespace std;
class node{
    public:
    int depth;
    string url;
    node(){
      depth=-1;
      url="";
    }
};
class seekstore{
    public:
    HashMap<string,node>hs;
    seekstore(){

    }
    void push(string val,int depth){
     if(hs.find(val)!=NULL){
        cout<<"already exist \n";
        return ;
     }
      node ns;
      ns.depth=depth;
      ns.url=val;
      hs.push(val,ns);
    }
    void pop(string val){
        hs.pop(val);
    }
    bool exists(string val){
        if(hs.find(val)!=NULL){
            return true;
        }
        return false;
    }
    node*get(string val){
         return hs.find(val);
    }

};
