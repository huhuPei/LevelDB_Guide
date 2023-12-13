# LevelDB_Guide
关于LevelDB的源码解析 ——基于leveldb v1.18版本

#### 学习要点：  
1. Slice 轻量级字符串  
2. Arena 内存管理  
3. coding 编码与解码   
4. compare（装饰器设计模式）
5. logblock设计
6. Skiplist实现（四叉）
7. 内存编解码（动态长度，小端编码）
8. iterator设计
9. LRU缓存设计

**目的**：通过LevelDB各个组件的解析，结合软件中的实际代码，巩固基础，熟悉语言特性，学习良好的编程规范与优秀的软件设计思想。

**PS**：LevelDB中有很多组件是相对单独的，在源码解析的文档，也会放入到相应的源码分析文件夹下便于学习使用。