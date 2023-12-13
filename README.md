# LevelDB_Guide
关于LevelDB的源码解析 ——基于leveldb v1.18版本

学习要点：
1、轻量级字符串Slice
2、area内存管理（当一个块用完或者不够分配时，才会申请下一个块）
3、coding编码与解码
4、compare（装饰器设计模式）
5、logblock设计
6、skiplist实现（四叉）
7、内存编解码（动态长度，小端编码）
8、iterator设计
9、LRU缓存设计

效果：通过LevelDB各个组件的解析，结合软件中的实际代码，巩固基础，熟悉语言特性，学习良好的编程规范与优秀的软件设计思想。