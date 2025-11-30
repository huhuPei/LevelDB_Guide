## Access Method
像大多数键值引擎一样，LevelDB 也提供了一些常见的数据访问接口，如 Get、Put 和 Delete，同时还允许用户高效地按顺序迭代键值。  

### 数据库定义
严格上讲，LevelDB 不是一个完整的数据库，而是一个存储引擎。它没有事务处理、查询语言、索引等高级功能，只负责存储键值数据，并提供简单的 API 调用。

#### 接口概览
DB 类是存储引擎的抽象，代表 LevelDB 的业务/应用层，而从字面上看，它是数据库的抽象。为了方便描述和用户使用，会将它当作具备简单操作的数据库类。    
DB 类各方法在源码注释中有详细描述，这里只做简述。    
```
// include/leveldb/db.h
class DB {
 public:、
  // 创建数据库，名称为 name
  static Status Open(const Options& options,
                     const std::string& name,
                     DB** dbptr);

  DB() { }
  virtual ~DB();
  // 设置字符串映射 key:value
  virtual Status Put(const WriteOptions& options,
                     const Slice& key,
                     const Slice& value) = 0;
  // 删除 key 对应的映射关系
  virtual Status Delete(const WriteOptions& options, const Slice& key) = 0;
  // 批次写入，一次性设置多个 key:value
  virtual Status Write(const WriteOptions& options, WriteBatch* updates) = 0;
  // 获取 key 对应的 value
  virtual Status Get(const ReadOptions& options,
                     const Slice& key, std::string* value) = 0;
  // 可以按序遍历所有 key:value 映射
  virtual Iterator* NewIterator(const ReadOptions& options) = 0;
  // 获取当前数据库状态的快照，可以实现重复读，不受后续写入影响
  virtual const Snapshot* GetSnapshot() = 0;
  ...
}
```
Status 是状态码，表示操作是否执行成功，若成功，返回 success status，即 “OK”；若失败，返回 error status，如 “NotFound” 表示 key 不存在。  

### 访问接口
DBImpl 是 DB 的子类，其中封装了数据库的核心业务逻辑。当用户通过 DB 接口访问数据库时，实际执行操作的是 DBImpl 对象。系统内部创建 DBImpl 对象后，会将其向上转型为 DB* 类型，仅对外暴露 DB 中定义的部分方法。这种接口隔离的方式屏蔽了子类具体细节，降低了用户调用与子类实现的耦合度。

#### 创建/打开接口
Open 操作会进行数据库初始化，主要包括数据库实例化、新建日志、数据恢复等操作：           
1.数据库实例化是创建一个 DBImpl 对象，完成初始化后回传给 *dbptr 指针，在不需要使用时，记得释放 *dbptr 指针防止内存泄漏；      
2.数据库使用日志（WAL）实现数据持久化，防止系统崩溃时出现更新丢失，因此需要新建日志文件是用于记录写入操作；     
3.数据恢复包括日志重放和版本重建，该操作涉及到**版本管理**和**写前日志**等相关内容，它们会在独立篇章中进行阐述。      

Options 是数据库的配置参数，使用 Open 创建/打开数据库时，可以通过 Options 设置不同的参数，不同的参数会影响数据库的具体行为。    
这里只列举部分参数的作用和注意事项。
```
// include/options.h
struct Options {
  // key 会按照比较器定义的规则进行排序。
  // 可传入自定义比较器，默认使用字节序比较器 BytewiseComparator。
  // 需要注意的是，后续打开数据库使用的比较器必须与创建时保持一致，因为 key 的顺序是由比较器决定的，
  // 如果前后使用不同的比较器，会因排序规则冲突导致数据顺序错乱，导致查询失败。
  const Comparator* comparator;
  // 若数据库不存在则创建。
  bool create_if_missing;
  
  // 写缓存大小，用于限制 Memtable 大小。
  // 当 Memtable 达到该阈值时，会被标记为 immutable(不可变)，同时创建新的 Memtable。
  // 写缓存最多有两个，一个是可写的 Memtable，一个是不可写的 Memtable。
  size_t write_buffer_size;
  // SStable DataBlock 大小
  size_t block_size;
  // 数据过滤器，在 SSTable 中查找数据时，会先进行过滤，提高读性能。
  // 默认为空。
  const FilterPolicy* filter_policy;
  ...
}
```
**关于写缓存**   
MemTable：可写，用于接受新的写入操作。
immutable MemTable：不可写，它是达到阈值的 MemTable，不再接受新写入，等待被后台线程转换为 SSTable ，然后 flush 到磁盘上。    

在一般情况下，这种双缓存机制可以保证在 Memtable 刷盘时，继续接收新的写入，避免写入阻塞，提升写性能。   

#### 写入接口
Put 操作用于新增/更新数据，对指定 key 的每次更新都是插入一个新版本。       
Delete 操作用于删除数据，删除指定 key 在底层也是插入操作——插入一个被标记为删除的新版本。    

WriteOptions 会影响写操作的具体行为，它只有一个参数：sync。
```
// include/options.h
struct WriteOptions {
  // sync=true 时，写操作会等待
  // 默认为 false
  bool sync;

  WriteOptions()
      : sync(false) {
  }
}
```

**批处理**     
特殊的写操作。
Write 
WriteBatch

#### 读取接口
Get 操作默认获取到最新数据。ReadOptions 会影响读操作的具体行为，如果想实现重复读取相同的数据，可以通过设置快照的方式实现。

```
// include/options.h
// 这里只关注快照参数
struct ReadOptions {
  ...
  // 默认为空，系统会返回最新数据
  // 如果传入指定快照，该快照之后的更新都不会产生影响，每次读取都会返回快照生成时对应的旧版本数据
  const Snapshot* snapshot;
};
```

一个快照读的简单例子
```
ReadOption ropt;
// 生成当前时刻的快照，此刻有数据 {"key":"v1"}
ropt.snapshot = db->GetSnapshot();
string val;
// val = "v1"
db->Get(ropt, "key", &val);
// 其他线程进行了更新 put("key", "v2")，依然可以读取到 val = "v1"
db->Get(ropt, "key", &val);
```

### 参考文件
```
db/db_impl.h
include/db.h
include/options.h
```