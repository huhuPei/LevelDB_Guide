## Arena

Arena是一个用于KV数据内存分配的内存池。  

KV 数据内存分配存在的问题：  
- KV 数据插入操作是比较频繁的操作，采用原始分配方式，如 new/delete 分配内存，会造成系统开销较大；  
- KV 数据本身较小，如果在内存分配大量小内存数据，会造成大量的内存碎片，影响内存使用效率。
  
因此需要一个内存池管理 KV 数据的内存分配。

### 设计思路
1.&nbsp;先 new 申请一块较大的内存块，当分配小内存时，会在这块预留的内存块上分配剩余空间，若空间用完或不足分配，就会再申请一块；  
2.&nbsp;当分配较大内存时，直接使用 new 分配所需空间的内存并使用，不使用预留的块，这个主要是考虑到内存使用率问题。

### 分析
1.&nbsp;内存池结构设计    
- 用一个 vector 数组保存所有分配的内存块地址（内存块默认大小为4KB）；   
- 用 alloc_ptr_ 指向当前内存块剩余空间的首地址；   
- 用 alloc_bytes_remaining_ 记录当前内存块剩余空闲空间大小。
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
原理图示：

![arena 示例](../img/arena.png "arena")

2.&nbsp;内存分配策略
- 若需要内存小于当前内存块的剩余空间，由当前内存块分配，并移动内存指针，更改剩余大小；
- 若需要内存大于当前内存块的剩余空间，将会申请新的内存块；
```
// util/arena.h
inline char* Arena::Allocate(size_t bytes) {
  // The semantics of what to return are a bit messy if we allow
  // 0-byte allocations, so we disallow them here (we don't need
  // them for our internal use).
  assert(bytes > 0);
  if (bytes <= alloc_bytes_remaining_) {
    char* result = alloc_ptr_;
    alloc_ptr_ += bytes;
    alloc_bytes_remaining_ -= bytes;
    return result;
  }
  return AllocateFallback(bytes);
}
```
- 当分配新的内存空间时，如果大于标准内存块的1/4（1KB），就申请非标准内存块，空间容量为用户所需大小，并独占整块内存，当前内存块信息不变；
- 否则，申请新的标准块（4KB），当前内存指针指向新块，然后分配所需内存。
```
// util/arena.cc
char* Arena::AllocateFallback(size_t bytes) {
  if (bytes > kBlockSize / 4) {
    // Object is more than a quarter of our block size.  Allocate it separately
    // to avoid wasting too much space in leftover bytes.
    char* result = AllocateNewBlock(bytes);
    return result;
  }

  // We waste the remaining space in the current block.
  alloc_ptr_ = AllocateNewBlock(kBlockSize);
  alloc_bytes_remaining_ = kBlockSize;

  char* result = alloc_ptr_;
  alloc_ptr_ += bytes;
  alloc_bytes_remaining_ -= bytes;
  return result;
}
```
以上可以看出内存池分配内存的整个过程，其中只有 Allocate() 方法是对用户开放，其他都是私有的，当需要分配内存时，只需调用Allocate()，其他工作由内存池负责完成。  

3.&nbsp;内存对齐分配   
Arena 中还有一个特殊的分配内存的函数 AllocateAligned()，相比于原始的 Allocate()，增加了一个内存对齐操作，可以提高对内存访问的效率。
```
char* Arena::AllocateAligned(size_t bytes) {
  const int align = (sizeof(void*) > 8) ? sizeof(void*) : 8;
  assert((align & (align-1)) == 0);   // Pointer size should be a power of 2
  size_t current_mod = reinterpret_cast<uintptr_t>(alloc_ptr_) & (align-1);
  size_t slop = (current_mod == 0 ? 0 : align - current_mod);
  size_t needed = bytes + slop;
  char* result;
  if (needed <= alloc_bytes_remaining_) {
    result = alloc_ptr_ + slop;
    alloc_ptr_ += needed;
    alloc_bytes_remaining_ -= needed;
  }
  ...
}

```
### 源代码
```
util/arena.h 
util/arena.cc
```
### 小结
在 LevelDB 中，Arena 只用于 Memtable 底层 Skiplist 键值插入时的内存分配，其他场景如 SSTable、WAL 都需要比较大的内存，同时内存申请操作频次低，所以都不会用到。

### PS： 
1.&nbsp;思考：**Arena内存分配不采用轮询内存块的方式，而是只使用当前内存块进行分配，也就是不管上一个内存块剩余多少，这样会不会造成内存浪费呢？**  

答案是肯定的，但是内存浪费的程度是可以接受的。
首先，leveldb的 KV 数据是没有删除操作的，只有插入操作，所有之前分配的内存不会回收再分配，浪费的内存只有内存块末尾的剩余空间；
其次，内存分配策略中，大于1KB 的内存会单独分配内存，同时 KV 一般都是内存比较小的数据，基本不会出现剩余990B，需要991B的情况，所以最后浪费的空间都是比较小的。

2.&nbsp;编程技巧   
- 禁止对象拷贝操作：可以将拷贝构造函数与拷贝赋值函数设置为私有（在 leveldb 源码中大量使用了这个用法）。
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
- 2次幂取余：数 m 对一个2次幂数 n 取余时，可以通过 m & (n-1) 进行快速取余，原理如下图所示。