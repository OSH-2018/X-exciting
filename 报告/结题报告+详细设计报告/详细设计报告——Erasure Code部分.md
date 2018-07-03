# 详细设计报告——Erasure Code部分

### Erasure Code简介

​    **erasure code是一种技术，它可以将n份原始数据，增加m份数据(用来存储erasure编码)，并能通过n+m份中的任意n份数据，还原为原始数据。定义中包含了encode和decode两个过程，将原始的n份数据变为n+m份是encode，之后这n+m份数据可存放在不同的device上，如果有任意小于m份的数据失效，仍然能通过剩下的数据还原出来。也就是说，通常n+m的erasure编码，能容m块数据故障的场景，这时候的存储成本是1+m/n，通常m<n。因此，通过erasure编码，我们能够把副本数降到1.x。**

###Erasure Code与RAID的关系

**erasure code可以认为是RAID的通式，任何RAID都可以转换为特定的erasure code。在传统的RAID中，仅支持少量的磁盘分布，当系统中存在多个分发点和多节点时，RAID将无法满足需求。比如RAID5只支持一个盘失效，即使是RAID6也仅支持两个盘失效，所以支持多个盘失效的算法也就是erasure code是解决这一问题的办法。Erasure Code作为可有效提升存储效率、安全性和便捷性的新兴存储技术。**

### 模块设计

**Erausre Code作为一个单独的模块集成到校园分布式文件共享系统中。**

**模块功能：**

**1、文件分块**

**2、文件校验**

**3、文件冗余**

**模块接口：**

```c
download(char **path,int block_num)
```

**对下载回来的文件块进行校验，并恢复成原来的文件**

```c
upload(char *filename,char *path,char *localblocks_path)
```

**对即将上传的文件进行分块、编码并生成md5校验码**

### 详细设计

**1、文件分块**

**文件块大小规则：**

**提供五种块大小：2K、32K、256K、4M、64M**

- **小于等于128K的文件采用2K大小的块进行分块**

- **128K~1M的文件采用32K大小的块进行分块**

- **1M~16M的文件采用256K大小的块进行分块**

- **16M~256M的文件采用4M大小的块进行分块**

- **256M以上的文件采用64M大小的块进行分块**

  **块数目一般不超过64个，不会少于4个。保证既不会因为块数量过多下载困难，也不会因为快数量过少使erasure code失去优势。**  

**函数实现：**

```
int split(char *filename,char *path,int64_t *blockLen, char **blockname,int64_t *last_blocklen)
```

**filename——原文件名**

**path—— 原文件所在目录(注意必须以''/''结尾)**

**blockname—— 分出的数据块的文件名**

**last_blocklen——最后一个数据块的大小**

**函数返回值：**

**成功——返回分得的数据块数**

**失败——返回对应错误代码**

**2、文件校验**

**在数据块和编码块均生成后会生成文件块的16位md5校验值，并写入文件名中，生成的code块也会生成md5校验值**

**在恢复文件时，若生成的md5校验值与文件名中的不符，则认为文件块失效。**

**块文件名规则：**

**原文件名-最后一块的字节数-数据块数-编码块数-块序号(数据块和编码块单独编号)-1/0(1代表数据块，2代表编码块)-md5校验值.tmp**

**函数实现：**

```c
char *MD5_file (char *path, int md5_len)
```

**path——文件路径(包括文件所在目录和文件名)**

**md5_len——需要生成的md5校验值的位数(默认16位，可更改)**

**文件返回值：**

**成功——返回生成的md5校验值的指针**

**失败——返回对应错误代码**

**函数功能：**

**生成对应文件的md5校验值**

```c
int check(char **path, int block_num,int *valid, int datablocknum_int,char **sequential_path,int64_t *blockLen)
```

**path——文件块路径(包括文件块所在目录和文件块名)**

**block_num——文件块数**

**valid——指示可用的文件块的数组**

**datablocknum_int——数据块数**

**sequential_path——按顺序排列的文件块路径(包括文件块所在目录和文件块名)**

**blockLen——文件块的长度**

**文件返回值：**

**成功——返回0**

**失败——返回对应错误代码**

**函数作用：**

**检查文件块是否已经损坏，并获取恢复原文件的一些必要信息**

**3、文件冗余^[1]^**

**采用的是Erasure Code中的LRC，增加了本地块冗余，大大降低I/O的消耗(多占用约5%左右的空间，将I/O的消耗降低至没有本地块时的10~50%。)**

**集成GF-complete（伽罗瓦运算库）采用多种矩阵(如柯西矩阵、范德蒙德矩阵)计算编码块，同时优化算法提升计算的速度，尽量降低erasure code的计算延迟。**

**函数实现：**

***初始化类函数：***

```c
int lrc_init_n(lrc_t *lrc, int n_local, uint8_t *local_arr, int m)
```

**lrc——指向lrc结构体，其中定义了一些lrc的相关参数**

**n_local——要生成的local块的数目**

**local_arr——每个本地块对应的数据块数**

**m——全部编码块数(包括local编码块和global编码块的总数)**

**函数返回值：**

**成功——返回0**

**失败——返回对应错误代码**

**函数功能：**

**初始化lrc**

```c
int  lrc_buf_init(lrc_buf_t *lrc_buf, lrc_t *lrc, int64_t chunk_size)
```

**lrc_buf——存储数据块和编码块中数据的buf**

**lrc——指向lrc结构体，其中定义了一些lrc的相关参数**

**chunk_size——每块的大小**

**函数返回值：**

**成功——返回0**

**失败——返回相应错误代码**

**函数功能：**

**初始化lrc_buf**

***释放资源类函数：***

```c
void lrc_destroy(lrc_t *lrc)
```

**lrc——指向lrc结构体，其中定义了一些lrc的相关参数**

**函数返回值：无**

**函数功能：**

**释放分配给lrc的内存**

```c
void lrc_buf_destroy(lrc_buf_t *lrc_buf)
```

**lrc_buf——存储数据块和编码块中数据的buf**

**函数返回值：无**

**函数功能：**

**释放分配给lrc_buf的内存**

***编码类函数：***

```c
int lrc_encode(lrc_t *lrc, lrc_buf_t *lrc_buf)
```

**lrc——指向lrc结构体，其中定义了一些lrc的相关参数**

**lrc_buf——存储数据块和编码块中数据的buf**

**函数返回值：**

**成功——返回0**

**失败——返回对应错误代码**

**函数功能：**

**对给定的数据块进行编码，生成的编码块中的内容写入buf**

***译码类函数：***

```c
int lrc_get_source(lrc_t *lrc, int8_t *erased, int8_t *source)
```

**lrc——指向lrc结构体，其中定义了一些lrc的相关参数**

**erased——指示哪些块可用的数组**

**source——指示恢复文件需要那些块的数组**

**函数返回值：**

**成功——返回0**

**失败——返回对应错误呢代码**

**函数功能：**

**判断剩余的文件块能否恢复文件，并给出恢复文件需要的文件块**

```c
int lrc_decode(lrc_t *lrc, lrc_buf_t *lrc_buf, int8_t *erased)
```

**lrc——指向lrc结构体，其中定义了一些lrc的相关参数**

**lrc_buf——存储数据块和编码块中数据的buf**

**erased——指示哪些块可用的数组**

**函数返回值：**

**成功——返回0**

**失败——返回对应错误代码**

**函数功能：**

**对给定的文件进行译码，将恢复的数据块写入buf中**

### 模块使用

**1、安装**

**lrc-erasure-code文件夹内的源文件需单独安装，安装步骤：**

**进入该文件夹，运行**

```bash
./configure
make
sudo make install
```

**即可安装成功**

**检验是否安装成功，可进入test文件夹，运行：**

```bash
gcc example.c -o example -llrc
./example
```

**此处检验时可能会出现编译或执行文件无法链接动态库的现象，需手动将动态库加到系统环境变量中，具体方法在上步安装完毕后的说明信息中介绍了4种。**

**module文件夹内为其他源文件，包括：**

**split.c——负责文件分块**

**combine.c——负责文件合并**

**check.c——负责文件检验**

**md5_file.c、md5.h、md5.c——负责文件md5校验值生成**

**upload.c——上传文件之前需执行的代码(包括文件分块、校验生成、ec编码)**

**download.c——下载文件之后需执行的代码(包括文件校验、ec译码、文件合并)**

**这些源文件在引用时只需include upload.c 和download.c 即可，同时需在文件中include"md5_file.c" 和 "lrc.h"，注意这些文件必须放在同一文件夹内。**

**2、运行**

**上传之前——编码：**

**1、通过upload接口获得需要分块冗余的原文件**

**2、调用split函数，检测文件大小，按照文件分块中陈述的规则进行分块**

**3、此时分得的块都是数据块，调用lrc_init_n,lrc_buf_init函数初始化lrc模块**

**4、将分得数据块写入buf中，调用lrc_encode函数生成编码块(4个local编码块+每4个数据块1个global编码块)**

**5、所有的数据块和编码块都会调用md5_file函数生成16位的md5校验值**

**6、调用lrc_destroy,lrc_buf_destroy函数释放申请的内存空间，释放资源**

**下载之后——译码：**

**1、通过download接口获得需要文件块**

**2、调用check函数，通过检验文件生成的md5值与文件名中的md5值来比较，校验文件是否损坏。同时从文件名中获取原文件的一些基本信息，以便恢复原文件**

**3、调用lrc_init_n,lrc_buf_init函数初始化lrc模块**

**4、调用lrc_get_source函数检测恢复文件需要的文件块以及检验是否能够恢复文件**

**5、将lrc_get_source函数指定的文件块写入buf中，调用lrc_decode恢复数据块**

**6、调用combine函数将数据块合成为原文件**

**7、调用lrc_destroy,lrc_buf_destroy函数释放申请的内存空间，释放资源**

### 模块测试

**模块安装完毕后，利用upload接口，编码并分块test.pdf，得到的结果如下图所示(path和localblocks_path均设为同一路径)：**

![image](https://github.com/OSH-2018/X-exciting/blob/master/%E6%8A%A5%E5%91%8A/%E7%BB%93%E9%A2%98%E6%8A%A5%E5%91%8A%2B%E8%AF%A6%E7%BB%86%E8%AE%BE%E8%AE%A1%E6%8A%A5%E5%91%8A/1.png)

**test.pdf的md5校验值为：**

![image](https://github.com/OSH-2018/X-exciting/blob/master/%E6%8A%A5%E5%91%8A/%E7%BB%93%E9%A2%98%E6%8A%A5%E5%91%8A%2B%E8%AF%A6%E7%BB%86%E8%AE%BE%E8%AE%A1%E6%8A%A5%E5%91%8A/2.png)

**在文件夹中任意取28块，进行文件恢复：**

![image](https://github.com/OSH-2018/X-exciting/blob/master/%E6%8A%A5%E5%91%8A/%E7%BB%93%E9%A2%98%E6%8A%A5%E5%91%8A%2B%E8%AF%A6%E7%BB%86%E8%AE%BE%E8%AE%A1%E6%8A%A5%E5%91%8A/3.png)

**校验恢复的文件是否与原文件相同：**

![image](https://github.com/OSH-2018/X-exciting/blob/master/%E6%8A%A5%E5%91%8A/%E7%BB%93%E9%A2%98%E6%8A%A5%E5%91%8A%2B%E8%AF%A6%E7%BB%86%E8%AE%BE%E8%AE%A1%E6%8A%A5%E5%91%8A/4.png)

**可以看到恢复的文件与原文件相同**

### 参考资料

**【1】https://github.com/baishancloud/lrc-erasure-code**