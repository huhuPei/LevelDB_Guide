#include <string>
#include <iostream>
#include "slice.h"

using std::string;
using std::cout;
using std::endl;
using leveldb::Slice;

//可以实现对原始str的任意切片操作
int main(){
    const char* str = "hello world";
    //切片操作 str[0:5) == "hello"
    Slice s1(str, 5);
    //切片操作 str[6:n) == "world"
    Slice s2(str+6, 5);
    // s3 引用了局部字符串，这是非法操作
    Slice s3;
    if(1){
        const char* str2 = "abc";
        Slice s4(str2);
        s3 = s4;
    }

    cout << s1.ToString() << endl;
    cout << s2.ToString() << endl;
    cout << s3.ToString() << endl;
    return 0;
}