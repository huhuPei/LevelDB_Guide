Arena是一个用于KV数据内存分配的内存池。  

KV 数据内存分配存在的问题：  
- KV 数据插入操作是比较频繁的操作，采用原始分配方式，如 new/delete 分配内存，会造成系统开销较大；  
- KV 数据本身较小，如果在内存分配大量小内存数据，会造成大量的内存碎片，影响内存使用效率。
  
因此需要一个内存池管理 KV 数据的内存分配。

#### 设计思路
先 new 申请一块较大的内存块，当分配小内存时，会在这块预留的内存块上分配空闲空间，若空间用完，就会再申请一块；  
当分配较大内存时，直接使用 new 分配内存并使用，不使用预留的块，这个主要是考虑到内存使用率问题。

#### 分析
1. Arena 的结构设计是比较简单的。
用一个 vector 数组保存所有分配的内存块；   
用 alloc_ptr_ 指向当前内存块空闲空间的首地址；   
用 alloc_bytes_remaining_ 记录当前内存块剩余空闲空间大小；
```
// util/arena.h
class Arena {
 ...

 private：
  // Allocation state
  char* alloc_ptr_;
  size_t alloc_bytes_remaining_;

  // Array of new[] allocated memory blocks
  std::vector<char*> blocks_;

  // Bytes of memory in blocks allocated so far
  size_t blocks_memory_;
}
```
2. 内存分配策略
  

3. 内存对齐分配
#### 源代码
```
util/arena.h 
util/arena.cc
```
#### 小结

#### PS：
- 编程技巧
当需要禁止对象拷贝操作时，可以将拷贝构造函数与拷贝赋值函数设置为私有（在 leveldb 源码中大量使用了这个用法）。
在C++11之后，可以使用=delete操作实现相同的效果。

```
// util/arena.h
class Arena {
 ...

 private:
  ...
  // No copying allowed
  Arena(const Arena&);
  void operator=(const Arena&);
};
```
