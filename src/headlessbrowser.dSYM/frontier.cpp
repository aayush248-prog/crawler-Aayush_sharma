#include<iostream>
#include<list>
using namespace std;
class node{
    public:
 node*nxt;
 string val;
 node(){
    nxt=NULL;
    val="";
 }

};
class frontier{
    public:
node*front;
node*end;
int count;
frontier(){
    end=front=NULL;
    count=0;
}
void push(string val){
   node*temp=new node();
   temp->val=val;
   if(front==NULL){
    front=temp;
    count++;
    end=temp;
    return;
   }
   end->nxt=temp;
   count++;
   end=end->nxt;
   return;

}
void pop(){
    if(front==NULL){
        throw out_of_range("out of range");
    }
    node*temp=front;
    if(front==end){
       front=end=NULL;
       count=0;
    }
    else{
    front=front->nxt;
    count--;
    }
    delete temp;

}
int size(){
    return count;
}
string peek(){
    if(front==NULL){
        return "";
    }
    return front->val;
}
bool isempty(){
    return front==NULL;
}
~frontier(){

    while(front!=NULL){
        node* temp=front;
        front=front->nxt;
        delete temp;
    }

    end=NULL;
}
};
int main(){

}