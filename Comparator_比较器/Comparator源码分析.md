## Comparator

LevelDB 的底层数据结构是跳表，数据是有序的 key-value，所以在插入或者查找数据时，需要进行 key 大小的比较，Comparator 就是为了定义比较规则设计的接口。   

### 比较器分析
#### 接口设计
Comparator 设计使用了装饰器模式。   
Comparator 是比较器的抽象接口，任何比较器实现都会继承这个接口，然后在 Compare() 方法中定义比较规则。 
```
// include/leveldb/comparator.h
class Comparator {
 public:
  // Three-way comparison.  Returns value:
  //   < 0 iff "a" < "b",
  //   == 0 iff "a" == "b",
  //   > 0 iff "a" > "b"
  virtual int Compare(const Slice& a, const Slice& b) const = 0;
  ...
};
```
InternalKeyComparator 是 LevelDB 内部实现的比较器，用于比较 internal key。它继承了 Comparator，同时通过一个 Comparator 成员去接收用户自定义的比较器。
```
// db/dbformat.h
class InternalKeyComparator : public Comparator {
 private:
  const Comparator* user_comparator_;
 public:
  explicit InternalKeyComparator(const Comparator* c) : user_comparator_(c) { }

  virtual int Compare(const Slice& a, const Slice& b) const;
  ...
};
```
比较器使用方法举例：
```
class UserComparatorImpl: public Comparator {
  int Compare(const Slice& a, const Slice& b) const {
    // 自定义实现比较规则
  }
}

Comparator* cmp = new InternalKeyComparator(new UserComparatorImpl)
cmp->Compare(a, b)
```
为什么需要这么设计？   
可以在已有功能上进行灵活扩展。在比较器设计中，用户只需关注 user key 的比较规则，数据库内部会进行规则扩展，加入序列号和类型比较，内部扩展这对用户来说是隔离的。

#### InternalKey 比较器
internal key 比较规则：  
- 先调用用户比较器，比较 user key 的大小；（升序）
- 若 user key 相等，再比较 seq num 和 type。（降序）

这里要注意的是序列号的比较是按降序来的，这在后面分析 memtable 如何插入或查找数据时是一个关键点。
```
// db/dbformat.cc
int InternalKeyComparator::Compare(const Slice& akey, const Slice& bkey) const {
  // Order by:
  //    increasing user key (according to user-supplied comparator)
  //    decreasing sequence number
  //    decreasing type (though sequence# should be enough to disambiguate)
  int r = user_comparator_->Compare(ExtractUserKey(akey), ExtractUserKey(bkey));
  if (r == 0) {
    const uint64_t anum = DecodeFixed64(akey.data() + akey.size() - 8);
    const uint64_t bnum = DecodeFixed64(bkey.data() + bkey.size() - 8);
    if (anum > bnum) {
      r = -1;
    } else if (anum < bnum) {
      r = +1;
    }
  }
  return r;
}
```
#### 高级方法     
Comparator 有两个优化空间的高级方法。     
主要是用于减少索引块中的索引大小，压缩索引块。索引块是在 SSTable 中使用的，后续会在 SSTable 中进行解析，这里不做展开。

```
// include/leveldb/comparator.h
class Comparator {
  ...
  // 在 [start,limit)中，找到最短的字符串 string >= start
  virtual void FindShortestSeparator(
      std::string* start,
      const Slice& limit) const = 0;
  // 找到最短字符串 string >= start
  virtual void FindShortSuccessor(std::string* key) const = 0;
}
```
如果不需要这种优化操作，用户自定义 Comparator 时，对这两个方法进行空实现即可。

### 默认比较器   
util 提供了一个 user key 默认比较器，规则是按字节大小排序。    
```
// util/comparator.cc
class BytewiseComparatorImpl : public Comparator {
 public:
  ...
  virtual int Compare(const Slice& a, const Slice& b) const {
    // 直接使用 Slice 的比较规则。
    return a.compare(b);
  }
}

// include/leveldb/slice.h
inline int Slice::compare(const Slice& b) const {
  // 先比较字符串字节的大小，再比较长度。
  const size_t min_len = (size_ < b.size_) ? size_ : b.size_;
  int r = memcmp(data_, b.data_, min_len);
  if (r == 0) {
    if (size_ < b.size_) r = -1;
    else if (size_ > b.size_) r = +1;
  }
  return r;
}
```

### 参考文件
```
db/dbformat.h
db/dbformat.cc
util/comparator.cc
include/leveldb/slice.h
include/leveldb/comparator.h
```
### PS
**编码技巧**   
- explicit 关键字：主要用于防止单参数构造函数的隐式类型转换。   
在 C++ 中，若一个类的构造函数只有一个参数，或者除第一个参数外其余参数都有默认值，那么这个构造函数就能用于隐式类型转换。
  ```
  InternalKeyComparator 的构造函数：
  explicit InternalKeyComparator(const Comparator* c) : user_comparator_(c) { }

  // 有如下场景：
  void someFunction(const InternalKeyComparator& comparator) {
    // 函数实现
  }
  const Comparator* myComparator = getComparator();
  // 如果没有 explicit，会发生隐式转换。
  someFunction(myComparator); 
  // 如果有 explicit，需要显式转换。
  someFunction(InternalKeyComparator(myComparator)); 
  ```