## Encode & Decode

LevelDB 的数据分为内存数据和磁盘数据，内存数据是当前的用户交互数据，保存在 Memtable 中；磁盘数据是持久化数据，保存在 SSTable 中。Memtable 中的数据达到一定数量，就会持久化生成SSTable，最终保存在磁盘上。    

<b>编码的必要性</b>
不论是内存数据，还是磁盘数据，出于便捷性、效率、占用空间等方面考虑，在数据存取时，通常都需要对数据进行一定格式的编码；同时计算机又存在大端、小端不同的数据存储方式，为了保证数据存储的统一性，兼容不同平台，也需要对数据进行编码。

<b>大端存储与小端存储</b>
一个多字节的数据，用逻辑表示时，从左到右，依次为高位到低位，如 0xaabbccff，最高位字节是aa，这也是在写代码或者文字表示一般都采用这种方式。而在计算机的角度，可以理解为数据在寄存器中的表示。  
用内存表示时，就是数据高低位字节在存储介质中的存放顺序，顺序的不同形成大小端两种字节序，如下所示：  
大端模式，高位字节存储在低位地址（数字小为低地址）：
<table>
<tr>
    <th>数据</th>
    <td>aa</td>
    <td>bb</td>
    <td>cc</td>
    <td>ff</td>
</tr>
<tr>
    <th>地址</th>
    <td>0x0010</td>
    <td>0x0011</td>
    <td>0x0012</td>
    <td>0x0013</td>
</tr>
</table>
小端模式，低位字节存储在低位地址：
<table>
<tr>
    <th>数据</th>
    <td>ff</td>
    <td>cc</td>
    <td>bb</td>
    <td>aa</td>
</tr>
<tr>
    <th>地址</th>
    <td>0x0010</td>
    <td>0x0011</td>
    <td>0x0012</td>
    <td>0x0013</td>
</tr>
</table>
    以上可以看出两种存储顺序是相反的。<b>在 leveldb 中统一都采用小端存储数据，也就是说如果平台采用的是大端模式，数据的编码操作会按小端模式进行编码，解码操作也会按小端模式解码。</b>

#### 分析

1.&nbsp;整数编码   
对于整数，有定长整数与变长整数两种编码方式，以下都以32位整数为例。

1.1 定长整数
平台是小端模式，直接复制即可；
平台是大端模式，按照小端字节序存储数据。
```
// util/coding.cc
// 32bit encode
void PutFixed32(std::string* dst, uint32_t value) {
  char buf[sizeof(value)];
  EncodeFixed32(buf, value);
  dst->append(buf, sizeof(buf));
}

void EncodeFixed32(char* buf, uint32_t value) {
  if (port::kLittleEndian) {
    memcpy(buf, &value, sizeof(value));
  } else {
    buf[0] = value & 0xff;
    buf[1] = (value >> 8) & 0xff;
    buf[2] = (value >> 16) & 0xff;
    buf[3] = (value >> 24) & 0xff;
  }
}
```

1.2 变长整数
采用 1bit(标志位) + 7bit(数据) 格式进行编码。   
对于一个无符号整数，从二进制表示看，第一个非0位之后的所有位才是有效位，所以可以考虑只保存有效位，可以达到压缩存储空间的效果。    

**编码规则：**
标志位表示是否为最后一个字节，0表示最后一个字节，1表示其他字节；数据每次取 7bit，从低位开始取，加上一个标志位，凑成一个字节存储，按照小端字节序，最低的 7bit 存储在最低位的地址中。

以21位有效位的整数为例（v < (1<<128)），结果如下：

```
// util/coding.cc
// 32bit encode
void PutVarint32(std::string* dst, uint32_t v) {
  char buf[5];
  char* ptr = EncodeVarint32(buf, v);
  dst->append(buf, ptr - buf);
}

char* EncodeVarint32(char* dst, uint32_t v) {
  // Operate on characters as unsigneds
  unsigned char* ptr = reinterpret_cast<unsigned char*>(dst);
  static const int B = 128;
  if (v < (1<<7)) {
    *(ptr++) = v;
  } else if (v < (1<<14)) {
    ...
  } else if (v < (1<<21)) {
    ...
  } else if (v < (1<<28)) {
    *(ptr++) = v | B;
    *(ptr++) = (v>>7) | B;
    *(ptr++) = (v>>14) | B;
    *(ptr++) = v>>21;
  } else {
    ...
  }
  return reinterpret_cast<char*>(ptr);
}
```
按照此种方式编码，32位整数将使用1-5个字节编码，而64位整数将使用1-10个字节编码。整数一般都用来表示字符串的长度，由于绝大部分字符串长度不会超过14个有效位，所以在大数据量下，节省的存储空间还是非常可观的。

2.&nbsp;字符串编码   
采用长度前缀编码。     
在字符串前添加长度信息，长度使用变长整数编码。

```
// utils/coding.cc
// size 32bits
void PutLengthPrefixedSlice(std::string* dst, const Slice& value) {
  PutVarint32(dst, value.size());
  dst->append(value.data(), value.size());
}
```


#### 源码
```
util/coding.h 
util/coding.cc
```

#### 小结


#### PS：
编程技巧    
- 大小端字节序编码
