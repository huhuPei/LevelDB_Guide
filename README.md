# LevelDB_Guide
关于LevelDB的源码解析 ——基于leveldb v1.18版本

## 学习要点：  
### Memtable
1. Slice 轻量级字符串  
2. Arena 内存管理  
3. Coding 编码与解码   
4. Key 内部设计（序列号机制）
5. Comparator 比较器（装饰器设计模式）
6. Skiplist 实现（分支为四）
7. MemTable 内存结构
### SSTable
1. SSTable 持久化
2. BloomFilter（布隆过滤器）
3. Compaction 策略
### 系统设计
1. Log 日志系统设计
2. Iterator 设计
3. LRU 缓存设计

**目的**：通过LevelDB各个组件的解析，结合软件中的实际代码，巩固基础，熟悉语言特性，学习良好的编程规范与优秀的软件设计思想。

**PS**：LevelDB 中有很多组件是相对单独的，在源码解析的文档中，也会放入到相应的源码分析文件夹下便于学习使用。