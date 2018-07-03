## erasure code 
- Erasure Code（EC），即纠删码，是一种前向错误纠正技术（Forward Error Correction，FEC），主要应用在网络传输中避免包的丢失，存储系统利用它来提高存储可靠性。相比多副本复制而言，纠删码能够以更小的数据冗余度获得更高数据可靠性，但编码方式较复杂，需要大量计算。纠删码只能容忍数据丢失，无法容忍数据篡改，纠删码正是得名与此。 
- 它可以将n份原始数据，增加m份数据，并能通过n+m份中的任意n份数据，还原为原始数据。
### Reed-Solomon Codes
- RS codes是基于有限域的一种编码算法，有限域又称为Galois Field，是以法国著名数学家Galois命名的，在RS codes中使用GF(2^w)，其中2^w >= n + m 。
- RS codes定义了一个(n + m) * n的分发矩阵(Distribution Matrix) （下图表示为B）。对每一段的n份数据，我们都可以通过B * D得到：
![](./RS_code1.png)
- 假如D1, D4, C2 失效，那么我们可以同时从矩阵B和B*D中，去掉相应的行，得到下面的等式：
![](./RS_code2.png)
- 如果想要从survivors 求得D，我们只需在等式的两边左乘上B'的逆：
![](./RS_code3.png)
- 由(B')^-1 * B' = I，所以我们可以计算得到D：
![](./RS_code4.png)
- 存在的问题：
    - 所有的数据丢失都将导致同样的重建代价，无论是一个数据块，还是m个数据块。
    - 单个数据块丢失的几率远远大于多个数据块同时丢失的几率。

### Locally Repairable Code（LRC，本地副本存储）
- LRC编码与RS编码方式基本相同，同时增加了额外的数据块副本。
- LRC编码本质上是RS编码+2副本备份。
    - LRC编码步骤如下：
        1. 对原始数据使用RS编码，例如编码为4：2，编码结果为4个数据块：D1、D2、D3、D4，2个编码块C1、C2。
        2. 原始数据做2副本，将4个数据块的前2个数据块和后2个数据块，分别生成2个编码块，即R1=D1 * D2，R2=D3 * D4。
        3. 如果某一个数据块丢失，例如D2丢失，则只需要R1和D1即可恢复D2。

### 基于EC的开源实现技术
- 现今，基于纠删码的开源实现技术主要有Intel ISA-L、Jerasure等库。 
- Intel ISA-L
    - Intel ISA-L（Intelligent Storage Acceleration Library），即英特尔智能存储加速库，是英特尔公司开发的一套专门用来加速和优化基于英特尔架构（IntelArchitecture，IA）存储的lib库，可在多种英特尔处理器上运行，能够最大程度提高存储吞吐量，安全性和灵活性，并减少空间使用量。该库还可加速RS码的计算速度，通过使用AES-NI、SSE、AVX、AVX2等指令集来提高计算速度，从而提高编解码性能。同时还可在存储数据可恢复性、数据完整性性、数据安全性以及加速数据压缩等方面提供帮助，并提供了一组高度优化的功能函数，主要有：RAID函数、纠删码函数、CRC（循环冗余检查）函数、缓冲散列（MbH）函数、加密函数压缩函数等。
- Jerasure
    - Jerasure是美国田纳西大学Plank教授开发的C/C++纠删码函数库，提供Reed-Solomon和Cauchy Reed-Solomon两种编码算法的实现。Jerasure有1.2和 2.0两个常用版本，Jerasure 2.0为目前的最新版本，可借助intel sse指令集来加速编解码，相比1.2版本有较大的提升。Jerasure库分为5个模块，每个模块包括一个头文件和实现文件。
    1. galois.h/galois.c：提供了伽罗华域算术运算。
    2. jerasure.j/jerasure.c：为绝大部分纠删码提供的核心函数，它只依赖galois模块。这些核心函数支持基于矩阵的编码与解码、基于位矩阵的编码与解码、位矩阵变换、矩阵转置和位矩阵转置。
    3. reedsol.h/reedsol.c：支持RS编/解码和优化后的RS编码。
    4. cauchy.h/Cauchy.c：支持柯西RS编解码和最优柯西RS编码。
    5. liberation.h/liberation.c：支持Liberation RAID-6编码啊、Blaum-Roth编码和Liberation RAID-6编码。其中，Liberation是一种最低密度的MDS编码。这三种编码采用位矩阵来实现，其性能优于现在有RS码和EVENODD，在某种情况下也优于RDP编码。

### GlusterFS纠删码卷
http://blog.51cto.com/dangzhiqiang/1924679

### 相关链接
https://blog.csdn.net/shelldon/article/details/54144730
https://blog.csdn.net/shelldon/article/details/54144730
http://blog.163.com/yandong_8212/blog/static/13215391420143281143547/
http://blog.sina.com.cn/s/blog_57f61b490102viq9.html

## BitTorrent
- BitTorrent是由布莱姆·科恩设计的一个点对点（P2P）文件共享协议，此协议使多个客户端通过不可信任的网络的文件传输变得更容易。 
- 根据BitTorrent协议，文件发布者会根据要发布的文件生成提供一个.torrent文件，即种子文件，也简称为“种子”。
- 种子文件本质上是文本文件，包含Tracker信息和文件信息两部分。Tracker信息主要是BT下载中需要用到的Tracker服务器的地址和针对Tracker服务器的设置，文件信息是根据对目标文件的计算生成的，计算结果根据BitTorrent协议内的Bencode规则进行编码。它的主要原理是需要把提供下载的文件虚拟分成大小相等的块，块大小必须为2k的整数次方（由于是虚拟分块，硬盘上并不产生各个块文件），并把每个块的索引信息和Hash验证码写入种子文件中；所以，种子文件就是被下载文件的“索引”。
- 下载者要下载文件内容，需要先得到相应的种子文件，然后使用BT客户端软件进行下载。
- 下载时，BT客户端首先解析种子文件得到Tracker地址，然后连接Tracker服务器。Tracker服务器回应下载者的请求，提供下载者其他下载者（包括发布者）的IP。下载者再连接其他下载者，根据种子文件，两者分别告知对方自己已经有的块，然后交换对方所没有的数据。此时不需要其他服务器参与，分散了单个线路上的数据流量，因此减轻了服务器负担。
- 下载者每得到一个块，需要算出下载块的Hash验证码与种子文件中的对比，如果一样则说明块正确，不一样则需要重新下载这个块。这种规定是为了解决下载内容准确性的问题。
- 一般的HTTP/FTP下载，发布文件仅在某个或某几个服务器，下载的人太多，服务器的带宽很易不胜负荷，变得很慢。而BitTorrent协议下载的特点是，下载的人越多，提供的带宽也越多，下载速度就越快。同时，拥有完整文件的用户也会越来越多，使文件的“寿命”不断延长。

http://www.bittorrent.org/beps/bep_0005.html
http://azard.me/blog/2015/10/24/introduction-to-bittorrent/
https://zh.wikibooks.org/zh-cn/BitTorrent%E5%8D%8F%E8%AE%AE%E8%A7%84%E8%8C%83

## IPFS
- 星际文件系统是一种点对点的分布式文件系统，旨在连接所有有相同的文件系统的计算机设备。在某些方面，IPFS类似于web, 但web是中心化的，而IPFS是一个单一的Bittorrent群集，用git仓库分布式存储。换句话说，IPFS提供了高吞吐量的内容寻址块存储模型，具有内容寻址的超链接。这形成了一个广义的Merkle DAG数据结构，可以用这个数据结构构建版本文件系统，区块链，甚至是永久性网站。IPFS结合了分布式哈希表，带有激励机制的块交换和自我认证命名空间。IPFS没有单故障点，节点不需要相互信任。

- 该文件系统可以通过多种方式访问，包括FUSE与HTTP。将本地文件添加到IPFS文件系统可使其面向全世界可用。文件表示基于其哈希，因此有利于缓存。文件的分发采用一个基于BitTorrent的协议。其他查看内容的用户也有助于将内容提供给网络上的其他人。IPFS有一个称为IPNS的名称服务，它是一个基于PKI的全局命名空间，用于构筑信任链，这与其他NS兼容，并可以映射DNS、.onion、.bit等到IPNS。