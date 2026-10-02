#include<stdio.h>
// In-class test case (see docs/Chats.txt): four optimizations in one function.
int main(){
        int a=3,b=4,c;
        a=7; //redundant assignment elimination
        b=8; //
        c = a+0;//statement with identity operator
        b=16*b;//statement with multiplication by power of two
        return a;
}
