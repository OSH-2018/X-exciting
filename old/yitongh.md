# 调研报告

## 项目背景
1. 分布式文件系统
    - 分布式文件系统(Distributed File System，DFS)是指文件系统管理的物理存储资源不一定直接连接在本地节点上，而是通过计算机网络与节点相连文件系统管理的物理存储资源。[1]
2. 设计目标
    - DFS的设计目标是使用户使用该系统就像使用本地文件系统一样，而在系统内部进行文件的存储管理、数据传输等底层工作。[2]
3. FastDFS [3]
    - FastDFS是一个开源的分布式文件系统，她对文件进行管理，功能包括：文件存储、文件同步、文件访问（文件上传、文件下载）等，解决了大容量存储和负载均衡的问题。特别适合以文件为载体的在线服务，如相册网站、视频网站等等。
    - 其系统架构如下：
        - FastDFS的服务端由跟踪器和跟踪器（tracker）和存储节点（storage）构成。跟踪器负责负载均衡和调度，在内存中记录分组和Storage server的状态等信息。存储节点存储文件，完成文件管理的所有功能：存储、同步和提供存取接口。
        - 存储节点采用了分组（group）的组织方式，组与组之间的文件是相互独立的。一个组可以由一台或多台存储服务器组成，一个组下的存储服务器中的文件都是相同的，组中的多台存储服务器起到了冗余备份和负载均衡的作用。
    - 上传文件交互过程：
        1. client询问tracker上传到的storage，不需要附加参数；
        2. tracker返回一台可用的storage；
        3. client直接和storage通讯完成文件上传。
	- 下载文件交互过程：
        1. client询问tracker下载文件的storage，参数为文件标识（卷名和文件名）；
        2. tracker返回一台可用的storage；
        3. client直接和storage通讯完成文件下载。
	
## 立项依据
### 去中心化
- 通常所见的分布式文件系统大多是中心化的，如GFS。拥有中心节点对处理分布式一致性等问题有较好的优势，但是也带来了不少问题。对中心化的系统而言，中心节点如果故障，很大可能会导致系统的崩溃，即所谓单点故障问题。另一个较大的问题是扩展性较低，由于中心节点承担的是维护整个系统和数据请求调度的任务，当节点增多时会给中心节点带来较大的负担，同时也会影响性能。去中心化则能很好的解决这个问题。[4]
### 动态负载均衡
- 在去中心化系统中，通常使用的是结构化的P2P系统，其通常使用分布式哈希表（DHT）技术	，如Chord、Kademlia等。
- 在结构化P2P协议中，各结点通过承载一定的键值空间来分担网络负载。在理想情况下，各节点的文件资源均匀分布，各结点负载均匀分布，系统具有良好的负载均衡并能发挥最优性能。然而在实际网络中，事实并非如此，基于DHT算法的P2P系统存在严重的负载不平衡。引起负载不平衡主要有以下原因：[5]
    1. 各结点承载的键值空间相差很大。在大规模的结构化P2P系统中，基于DHT算法生成结点ID的方法使结点承载的最大键值空间是最小键值空间的O(logN)倍，例如Chord，Pastry等P2P协议。
    2. 各个节点的存储空间、带宽等方面不同，当一个比较大的键值空间被分配到一个性能比较低的节点时，会带来很大的负载不平衡。
    3. 各结点承载的文件数目相差较大。即使各结点承载的键值空间相同，各结点负载也不一定均匀。在具有n个结点和n个键值的情况下，将有Θ(logN/loglogN)个键值以很高的概率被分配到同一个结点上。
    4. 用户查询具有不平衡性。目前提出的结构化P2P模型均假设用户查询随机而均匀。然而P2P系统中的查询请求分布和HTTP请求分布具有很大的相似性，均服从Zipf分布定律。各关键字查询的概率存在数量级差异，承载热点文件的结点负载将远大于其他结点。这就导致有部分节点的负载会远远高于其他节点，导致整个系统的资源利用率不高。

## 相关工作
### 去中心化
- 科研：解决去中心化的技术目前主要有DHT技术。[6]	
    - DHT技术：DHT全称叫分布式哈希表(Distributed Hash Table)，是一种分布式存储方法。它提供的服务类似于hash表，键值对存储在DHT中，任何参与该结构的节点能高效的由键查询到数据值。这种键值对的匹配是分布式的分配到各个节点的，这样节点的增加与删减只会带来比较小的系统扰动。这样也可以有效地避免“中央集权式”的服务器（比如：tracker）的单一故障而带来的整个网络瘫痪。
        - DHT技术强调三个特点：
            - 离散性：构成系统的节点并没有任何中央式的协调机制。
            - 伸缩性：即使有成千上万个节点，系统仍然应该十分有效率。
            - 容错性：即使节点不断地加入、离开或是停止工作，系统仍然必须达到一定的可靠度。
        - 其关键技术为：任一个节点只需要与系统中的部分节点（通常为O(logN)个）沟通，当成员改变的时候，只有一部分的工作（例如数据或键的发送，哈希表的改变等）必须要完成。
        - 基本上，DHT技术就是一种映射key和节点的算法以及路由的算法。
    - DHT的结构：键空间分区和覆盖网络。
        - 键空间分区是指每一个节点掌管部分键空间。
        - 覆盖网络是指一个连接各个节点的抽象网络，它能使每个节点找到拥有特定键的节点。每个节点或者存储了该键，或者储存有离这个键更近（这个距离由具体算法定义）的节点链接。
    - DHT数据存储大致流程：假设键空间是一个160位的字符串，对一个文件名和文件数据data，用哈希算法生成文件名对应的160位的散列值（key）。然后向一个在DHT网络中的节点发送message put（key ,data）。这个节点通过覆盖网络最终能找到负责这个key的节点。
    - DHT文件获得大致流程：用户获得文件时，也是将文件名hash成一个key，然后向一个节点发送get（key）。这个信息会最终到达负责这个key的节点，并返回相应的数据。
    - DHT实现算法有Chord, Pastry, Kademlia等。

- 工业界：目前实现去中心化的分布式文件系统有ceph和 glusterfs。
    - ceph：
        - Ceph分布数据的过程：首先计算数据x的Hash值并将结果和PG（分区）数目取余，以得到数据x对应的PG编号。然后，通过CRUSH算法将PG映射到一组OSD中。最后把数据x存放到PG对应的OSD中。这个过程中包含了两次映射，第一次是数据x到PG的映射。如果把PG当作存储节点，那么这和文章开头提到的普通Hash算法一样。不同的是，PG是抽象的存储节点，它不会随着物理节点的加入或则离开而增加或减少，因此数据到PG的映射是稳定的。[7]
        - Ceph文件系统拥有优秀的性能、高可靠性和高可扩展性这三大特性。秀的性能是指数据能够在各个节点上进行均衡地分布；高可靠性表示在Ceph文件系统中没有单点故障，并且存储在系统中的数据能够保证尽可能的不丢失；高可扩展性即Ceph系统易于扩展，能够很容易实现从TB到PB级别的扩展。[8]
    - glusterfs：
        - GlusterFS采用独特的无中心对称式架构，与其他有中心的分布式文件系统相比，它没有专用的元数据服务集群。在文件定位的问题上，GlusterFS使用DHT算法进行文件定位，集群中的任何服务器和客户端只需根据路径和文件名就可以对数据进行定位和读写访问。换句话说，GlusterFS不需要将元数据与数据进行分离，因为文件定位可独立并行化进行。
        - 数据访问流程：[9]
            1. 使用Davies-Meyer算法计算32位hash值，输入参数为文件名
            2. 根据hash值在集群中选择子卷（存储服务器），进行文件定位
            3. 对所选择的子卷进行数据访问。
        - 存在的问题：GlusterFS目前主要适用大文件存储场景，对于小文件尤其是海量小文件，存储效率和访问性能都表现不佳。
        - 优势：使用弹性哈希算法，从而获得了接近线性的高扩展性，同时也提高了系统性能和可靠性。[10]
### 动态负载均衡
- 目前负载均衡的研究比较多，下面列出在P2P网络中进行负载均衡的技术。
    - 缓存技术
    - 动态副本法：主要针对由于资源访问热度不均而引起的网络负载不均问题。算法的核心思想是通过增加“热”文件副本数量，将客户端连接分散到多个节点中，实现  “热”点降温。不同动态副本算法之间的主要区别在于如何选择副本放置节点。[11]
    - 虚拟节点算法：主要针对因服务器配置不同而引起的存储负载不均问题。设计思想是通过虚拟节点而不是实际物理节点进行数据的储存和路由路径上的文件查询转发。算法将配置不同的服务器虚拟成配置一致的虚拟节点，并随机分布在哈希环上。每一个实际的服务器负责多个虚拟节点。当有节点负载过重时，将负载过重的物理节点的虚拟节点迁移到负载较轻的物理节点上。[12]
    - 动态路由表：针对资源查找路由产生的负载。在P2P系统中，拥有热点文件的节点会承担大量的文件查询请求，以及查询路由路径上的节点也会承担相应的请求消息路由负载。[13]

[1]:https://image.hanspub.org/Html/1-2690253_20184.htm#txtF7
[2]:https://en.wikipedia.org/wiki/Clustered_file_system#Distributed_file_systems
[3]:http://os.51cto.com/art/201210/359876.htm
[4]:http://www.raychase.net/2396
[5]:http://www.jos.org.cn/ch/reader/create_pdf.aspx?file_no=3226&journal_id=jos
[6]:https://blog.csdn.net/u012785382/article/details/70739325
[7]:http://www.cnblogs.com/shanno/p/3958298.html
[8]:http://www.wanfangdata.com.cn/details/detail.do?_type=degree&id=D823772
[9]:https://blog.csdn.net/liuaigui/article/details/6284551
[10]:http://blog.sae.sina.com.cn/archives/5141
[11]:http://www.chinacloud.cn/upload/2013-10/13102408211400.pdf
[12]:https://people.eecs.berkeley.edu/~istoica/papers/2003/lb-iptps03.pdf
[13]:https://wenku.baidu.com/view/d8d27220192e45361066f5fd.html