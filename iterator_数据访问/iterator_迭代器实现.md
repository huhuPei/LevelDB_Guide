## Iterator

迭代器用于遍历数据，每迭代一次（一般是调用 next 函数），输出当前元素，并定位到下一个元素，直到输出最后一个元素。   
不管是 Memtable 还是 SSTable，底层存储还是上层 DB 访问，统一都通过迭代器访问。这里只解析与 Memtable 相关的接口实现。   

### 迭代器接口
#### 接口设计
一个迭代器设计，通常包含定位、移动、获取数据等功能。
```
// include/leveldb/iterator.h
class Iterator {
 public:
  Iterator();
  virtual ~Iterator();

  // 返回 true 表示迭代器定位在有效 entry 上，否则，表示迭代器当前无效，不能访问数据。
  // 在迭代一个 entry 之前，需要先调用 Valid() 进行判断其是否有效。
  // 在 LevelDB 实现中，entry 指代 key-value pair
  virtual bool Valid() const = 0;
  // 定位到第一个 entry，若数据非空，Valid() 返回 true。
  virtual void SeekToFirst() = 0;
  // 定位到最后一个 pair，若数据非空，Valid() 返回 true。
  virtual void SeekToLast() = 0;

  // 定位到第一个 >= target 的 entry，这与跳表的查找逻辑是保持一致的。
  // 若存在满足条件的 entry，Valid() 返回 true。
  virtual void Seek(const Slice& target) = 0;

  // 移动到下一个 entry。若当前定位在最后一个 entry，移动后，Valid() 返回 false。
  // 前置条件： Valid () 返回 true。
  virtual void Next() = 0;

  // 移动到上一个 entry。若当前定在第一个 entry，移动后，Valid() 返回 false。
  // 前置条件： Valid () 返回 true。
  virtual void Prev() = 0;

  // 返回当前 entry 的 key。
  // 返回的 Slice 在迭代器移动后不可再使用，因为其引用的底层存储可能已经被释放，继续使用属于非法访问。
  // 前置条件：Valid() 返回 true。
  virtual Slice key() const = 0;

  // 返回当前 entry 的 value。    
  // 返回的 Slice 在迭代器移动后不可再使用，因为其引用的底层存储可能已经被释放，继续使用属于非法访问。
  // 前置条件：Valid() 返回 true。
  virtual Slice value() const = 0;

  // 返回迭代器当前状态，若迭代过程出现错误将返回 false。
  virtual Status status() const = 0;
}
```
对于 key() 和 value() 的限制，是用户必须遵循的接口规范，防止出现未定义行为。尽管对于 memtable 而言，它是整个数据一起释放的，Slice 没有访问的风险，但对于 sstable 而言，它属于磁盘数据，内存分配是以磁盘块为单位的，所以如果一个磁盘块占用的内存被释放，Slice 之前引用了该内存上的数据，那么就有非法访问的风险。   

### 迭代器实现

#### memtable 迭代器实现
继承并实现相关接口函数，通过 skiplist iteator 访问数据。

```
// db/memtable.cc
class MemTableIterator: public Iterator {
 public:
  explicit MemTableIterator(MemTable::Table* table) : iter_(table) { }

  virtual bool Valid() const { return iter_.Valid(); }
  virtual void Seek(const Slice& k) { iter_.Seek(EncodeKey(&tmp_, k)); }
  virtual void SeekToFirst() { iter_.SeekToFirst(); }
  virtual void SeekToLast() { iter_.SeekToLast(); }
  virtual void Next() { iter_.Next(); }
  virtual void Prev() { iter_.Prev(); }
  // 返回 internal key
  virtual Slice key() const { return GetLengthPrefixedSlice(iter_.key()); }
  // 返回 value
  virtual Slice value() const {
    Slice key_slice = GetLengthPrefixedSlice(iter_.key());
    return GetLengthPrefixedSlice(key_slice.data() + key_slice.size());
  }

  virtual Status status() const { return Status::OK(); }

 private:
  // skiplist iterator，用于访问底层存储
  MemTable::Table::Iterator iter_;
  std::string tmp_;       // For passing to EncodeKey
  ...
}

// 返回一个 memtable iterator，用于遍历 entry
Iterator* MemTable::NewIterator() {
  return new MemTableIterator(&table_);
}
```
**迭代示例**        
向前遍历，从第一个遍历到最后一个。
```
...
Iterator* iter = mem.NewIterator();
// 初始的迭代器还未定位到有效 entry，需要先定位到一个有效 entry，这里用 SeekToFirst() 定位到首个entry。当然也能用 Seek() 定位到特定 entry。
for (iter.SeekToFirst(); iter.Valid(); iter.Next()) {
  Slice internal_key = iter.key();
  Slice value = iter.value();
  ...
}
```

#### skiplist 迭代器实现
skiplist 不使用 Iterator 接口，而是在内部单独定义了一个类 SkipList::Iterator，这是因为 skiplist 存储的是 entry，不区分 key 和 value，迭代时只需要用 key() 返回 entry，而 value() 是不需要的，因此 Iterator 接口不适用。    
```
// db/skiplist.h
// 只展示部分需要讨论的函数
template<typename Key, class Comparator>
class SkipList {
  class Iterator {
   public:
    // 返回一个无效的迭代器.
    explicit Iterator(const SkipList* list);
    ...
   private:
    // 指向 skiplist
    const SkipList* list_;
    // 指向 skiplist 当前节点
    Node* node_;
  }
}

// 初始化时，当前节点为空，所以初始的迭代器是无效的
// 这是需要在创建迭代器后，需要调用定位函数的原因
template<typename Key, class Comparator>
inline SkipList<Key,Comparator>::Iterator::Iterator(const SkipList* list) {
  list_ = list;
  node_ = NULL;
}

// 返回节点数据 entry
template<typename Key, class Comparator>
inline const Key& SkipList<Key,Comparator>::Iterator::key() const {
  assert(Valid());
  return node_->key;
}

// 定位到 >= target 的第一个节点 
template<typename Key, class Comparator>
inline void SkipList<Key,Comparator>::Iterator::Seek(const Key& target) {
  node_ = list_->FindGreaterOrEqual(target, NULL);
}

// 定位到第一个有效节点
template<typename Key, class Comparator>
inline void SkipList<Key,Comparator>::Iterator::SeekToFirst() {
  // 需要最底层定位，不然会定位更后面的节点，Next 传入 0
  node_ = list_->head_->Next(0);
}
```
除了底层存储，上层业务都会区分 key 和 value，统一使用 Iterator 接口访问数据。

### 参考文件
```
db/memtable.cc
db/skiplist.h
include/leveldb/iterator.h
db/skiplist_test.cc
```