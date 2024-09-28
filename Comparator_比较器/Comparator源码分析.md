## Comparator   

LevelDB 的底层数据结构是跳表，数据是有序的 key-value，所以在插入或者查找数据时，需要进行 key 大小的比较，Comparator 就是为了定义比较规则设计的接口。   

Comparator 设计使用了装饰器模式。
### 分析
1.&nbsp;Comparator 接口设计      

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
    ...
  }
}

Comparator* cmp = new InternalKeyComparator(new UserComparatorImpl)
cmp->Compare(a, b)
```
为什么需要这么设计？   
  可以在不影响主体功能的情况下进行功能扩展。用户只需关注 user key 的比较规则，数据库内部会进行规则扩展，加入序列号和类型比较。

InternalKey 比较规则：   
- 先比较 user key 的大小；
- 若 user key 相等，再比较 seq num 和 type。
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



2.&nbsp;高级方法      

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
如果不需要这种优化操作，用户自定义 Comparator 时，在这两个方法里可以不做任何操作。

3.&nbsp;默认比较器   

utils 提供了一个默认比较器，规则是按字节大小排序。

### 源码

### PS
- 编码技巧
explicit 关键字使用
