## Comparator   

LevelDB 的底层数据结构是跳表，数据是有序的，所以在插入或者查找数据时，需要进行数据大小的比较，Comparator 就是为了定义比较规则设计的接口。   

Comparator的设计使用了装饰器模式。
### 分析
1. Comparator接口设计
-  Compare( ) 用于实现比较规则；
-  FindShortestSeparator( )，在 [start,limit)中，找到 >= start 且 < limit的最短字符串，该函数的目的是用于压缩索引长度，主要用于降低索引块的大小，这个需要在 SSTable 的实现中去理解；（可以是空实现）
-  FindShortSuccessor( ) 用于找到大于 key的最短字符串。（可以是空实现）
```
// include/leveldb/comparator.h
class Comparator {
 public:
  // Three-way comparison.  Returns value:
  //   < 0 iff "a" < "b",
  //   == 0 iff "a" == "b",
  //   > 0 iff "a" > "b"
  virtual int Compare(const Slice& a, const Slice& b) const = 0;

  virtual void FindShortestSeparator(
      std::string* start,
      const Slice& limit) const = 0;

  virtual void FindShortSuccessor(std::string* key) const = 0;

  ...
};
```
2. 装饰器模式设计。  
- leveldb 内部实际使用的是 InternalKeyComparator， 采用了典型的装饰器模式设计，继承了 Comparator，并通过一个 Comparator 接口成员，实现对比较器的扩展。
- 为什么需要这么设计？   
  第一，这样做的作用在于实现主体功能与扩展功能的分离，用户只需要定义 key 的比较规则，而无需对其他比较内容进行操作定义；
  第二，
```
// db/dbformat.h
class InternalKeyComparator : public Comparator {
 private:
  const Comparator* user_comparator_;
 public:
  explicit InternalKeyComparator(const Comparator* c) : user_comparator_(c) { }

  int Compare(const InternalKey& a, const InternalKey& b) const;

  ...
};
```
- 可以看出除了 user_key 的比较之外，额外添加了其他比较操作。
```
// db/dbformat.h
int InternalKeyComparator::Compare(const Slice& akey, const Slice& bkey) const {
  int r = user_comparator_->Compare(ExtractUserKey(akey), ExtractUserKey(bkey));
  if (r == 0) {
    // sequence number + type 的比较
    ...
  }
  return r;
}
```

3. 默认比较器
utils 提供了一个默认比较器，规则是按字节大小排序。

### PS：
- 编码技巧
explicit 关键字使用
