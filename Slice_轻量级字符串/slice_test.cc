#include <string>
#include <iostream>

#include "leveldb/slice.h"

using leveldb::Slice;
using namespace std;

int main() {
    const char* str = "hello world";
    //切片操作 str[0:5) == "hello"
    Slice s1(str, 5);
    //切片操作 str[6:n) == "world"
    Slice s2(str+6, 5);
    std::cout << "s1: " << s1.data() << std::endl;
    std::cout << "s2: " << s2.data() << std::endl;
    
    Slice s3;
    Slice s4;
    {
        // str2 指向字符串字面量，存储在静态数据区
        const char* str1 = "abc";
        // s3 指向的是静态数据，是安全的 
        Slice s3(str1);
        // string 内部字符串数据存储在堆内存上，在对象销毁时，内存会被释放
        std::string str2("def");
        // s4 指向堆内存
        s4 = Slice(str2);
        std::cout << "s4: " << s4.data() << std::endl;
    }
    // str2 已销毁，内存释放被系统回收，str3 会重新使用回收后的内存 
    string str3("hhh");
    // s4 指向已释放的内存（该内存已被 str3 重使用），属于非法访问，操作不安全   
    std::cout << "s4: "<< s4.data() << std::endl;
    return 0;
}

/* Output:
s1: hello world
s2: world
s4: def
s4: hhh
*/