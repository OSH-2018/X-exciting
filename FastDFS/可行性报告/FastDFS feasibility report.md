#                                             可行性报告

## 一、项目综述

- 本项目旨在FastDFS的基础上，综合其他分布式文件系统的优点，对现有的FastDFS进行改进优化。
- 实现Fast DFS的完全去中心化以及负载的动态均衡。 



## 二、理论依据

### 1. DHT模型——去中心化的理论依据

1)传统Hash Table简介

![1523537685610](C:\Users\hangy\Desktop\FastDFS\可行性报告\picture\1523537685610.png)

- 散列表的含义

  散列表是用来存储键-值对的一种容器，有了散列表可以快速的通过key来获得value

- 散列表的是实现

  在散列表这种数据结构中，会包含N个bucket，对于某个具体的散列表，N的数量通常是固定的，标号为0 ~ N - 1。桶用来存储键值对。

- 散列表中数据的获取

  待查数据的key值 --- hash( key ) ---> 利用key的散列值在buckets中查找数据

  ​

2) DHT概述

![1523532562853](C:\Users\hangy\Desktop\FastDFS\可行性报告\picture\1523532562853.png)

- DHT主要用于存储大量数据，在实际应用场景中，直接对所存储的每一个数据计算hash值，然后用hash值作为key，数据本身作为value。
- 与传统Hash Table的主要区别是：
  - 传统散列表主要用于单机上的某个软件中，DHT用于分布式文件系统，可以认为buckets分布在不同的服务器上。
  - 传统散列表所对应的buckets数目是不变的，但是由于DHT往往用于互联网大数据的存储，所以节点的数量在频繁的变化。

3) DHT的主要工作

- 完全去除中央服务器
- 将数据均匀分布到每个节点

4) DHT的设计与结构

- DHT的结构可以分为几个部分，其中最重要的基础就是散列值范围。考虑到DHT的应用场景，其需要承载的数据量通常比较大，为了防止冲突，一般将散列值范围( keyspace )设定得尽可能大；通常采用长度大于128bit的散列值( 2^128已经比地球上所有电子文档的总述还要高出好几个数量级，因此是完全够用的)。
- DHT中每一个节点分配唯一的一个ID。很多DHT设计会让节点的ID和数据的key值同构，这样在散列值空间足够大的时候可以忽略随机碰撞，进而保证ID的唯一性。
- 大部分DHT使用的算法都是通过一致性哈希或者非一致性哈希算法的变体来根据key定位节点。无论是一致性哈希算法还是非一致性哈希算法，在删除或添加一个节点时，只需要改变拥有相邻ID的节点所拥有的keys值，而其他的节点无需改变。而传统的哈希表在插入或删除节点后需要对几乎整个哈希表做出改变。
- 哈希算法
  - 一致性哈希
    - 一致性哈希用函数δ( k1, k2 )来描述key值k1与k2之间的距离（这个距离函数与两个存储节点的实际物理距离无关）。
    - 每一个节点分配到一个key值称为ID，具有ID ix的节点将具有一组key值{ km }，对于这一组key中的每一个km，ix都是使得δ( km, ix )最小的取值。
  - 非一致性哈希
    - 在非一致性哈希中，所有用户使用同一个哈希函数来给可用的存储节点分配Key值，同时拥有同一张存储节点的表{ S1, S2, ..., Sn }
    - 在给定key值k的情况下，一个用户会计算n各哈希权重，wi = h( Si, k )，并将使得wi最大的节点Si拥有key值k。这样一个存储节点就拥有m(用户个数)个key值。
- 路由算法的权衡
  - 由于DHT中的节点数非常多，并且节点都是动态变化的，因此不可能让每一个节点都记录所有其他节点的信息；因此每个节点通常只知道少数一些节点的信息。
  - 在这种情况下，就需要尽可能利用已知的节点来转发数据。此时就需要路由算法。
- 距离算法
  - 某些DHT系统还会定义一种距离算法，来计算节点与节点的距离、数据与数据的距离、节点与数据的距离
  - 可以发现，之前所提到节点ID与数据key值的同构也是为计算不同对象之间的距离提供方便。


5) DHT的工作机制

- DHT的核心功能——保存数据 & 获取数据

>void put ( KEY k, VALUE v ); //保存“键-值对”
>
>VALUE get( KEY k ); //根据Key值获取数据

- 保存数据

  ①当某个节点得到了加入的新数据的(key, value)后，它会先根据自己的ID和新数据的key计算自己与新数据之间的“距离”，然后再计算他知道的其他节点与这个该数据的距离。

  ②若计算结果表明该节点距该数据最近，就将该数据保存再自己这里；否则将数据发送给距离最小的节点。

  ③收到该数据的节点重复①②过程

- 获取数据

  ①当某个节点接收到查询数据的请求，也即接收到一个key值后，它首先根据自己的ID以及这个key值计算自己与该数据之间的距离，然后在计算它所知道的其他节点与这个key的距离。

  ②若计算结果表明该结点据该数据最近，该结点就在自己的存储空间中寻找该key值所对应的数据。如果没有找到则报错。

  ③若计算结果表明另一个节点N'据该数据最近，则该结点就把这个key值发送给节点N'。N‘在接收到key值之后，重复①②③的过程。




## 三、技术依据

## 1. Chord协议——DHT的一种技术实现

1) 概述

- Chord诞生于2001年，第一批DHT协议都是在概念涌现出来的。
- Chord是当今广泛采用的一种DHT协议，原理较为简单，相关资料丰富。


2) 原理介绍

- 环形拓扑结构

  - 为解决DHT应用场景的节点动态变化问题，Chord将散列值空间构成一个环。对于m bit的散列值，其范围是[ 0, 2 ^ m - 1 ]，即构成一个周长为2 ^ m的环。
  - 规定环移动的方向，逆时针或顺时针。根据所规定的方向从而决定环上每个节点的前驱和后继节点。
  - 对数据空间进行划分，每个数据单元都属于按照规定方向距离其最近的那个环上节点。

  ![1523543557820](C:\Users\hangy\Desktop\FastDFS\可行性报告\picture\1523543557820.png)

- 路由机制——Finger Table

  - Finger Table是一个列表，最多包含散列值bit数个数，每一项都是节点的ID。
  - 假设当前节点的ID是n，那么表中第i项的值为successor( (n + 2^i) mod 2^m )
  - 当收到请求后，就到FInger Table中周到最大的且不超过key的那一项，然后把key转发给这一项对应的节点

- 节点的加入

  - 任何一个新加入的节点A首先需要与DHT中已有的任意节点B建立连接
  - A随机生成一个散列值作为自己的ID（对于足够大的散列值空间，可以忽略冲突的概率）
  - A通过向B进行查询，找到ID在环上的后继C和前驱D
  - A通过与C和D进行互动（原理类似于在双向链表中插入一个元素），是实现节点的加入

- 节点的删除

  - 类似于在双向链表中删除元素

## 2. Fast DFS的基于存储节点性能的动态负载均衡算法

- 相关参数的定义

  - 定义storage server中组Si的性能为Pi，性能系数为ki。定义Pi = ki / k1(k1为第一组storage server的性能系数)

  - 定义第i组中的第j个storage server Sij的存储空间利用率为Rij，Rij = Uij / Tij

  - 定义Gi的负载为Li，Li =∑Lij = ∑Rij * Nij，（其中Nij为第i组第j个服务器的连接数）。所有storage server的分组信息和连接数都由tracker server保存

  - 定义PLij为第i组中第j个服务器的性能负载比，PLij
    = Pj / Lij。根据定义可以知道，当一个storage server的负载越大同时性能越差时，他的性能负载比越小
- 算法思路
    - client请求到达tracker server
    -   tracker server首先仍然按照现有的算法选出可用组
    -  计算每个组内的storage
      server的存储空间利用率和连接数，进而计算出各组的总负载Li
    - 选择负载最轻的两个组，计算每个storage server的性能负载比PLij，选择PLij较大的那个组来接受请求




## 四、创新性

- 实现了Fast DFS文件系统的完全去中心化，从而提升系统的安全性。

- 提升了存储系统的容错能力，即使有部分节点出现错误，整个系统并不会瘫痪。

- 提升了文件系统的可扩展性，即使有大量节点加入仍能保证系统的性能。如果整个系统中节点个数为N，那么对系统进行一次改变所需要的平均时间为O( log N )。

  ​

### *参考文献：

[1]: Stoica I, Morris R, Karger D, et al. Chord: A scalable peer-to-peer lookup service for internet applications[C]// Conference on Applications, Technologies, Architectures, and Protocols for Computer Communications. ACM, 2001:149-160.

[2]: Lakshman A, Malik P. Cassandra:a decentralized structured storage system[J]. Acm Sigops Operating Systems Review, 2010, 44(2):35-40.

[3]: 韩增曦. 分布式文件系统FastDFS的研究与应用[D]. 大连理工大学, 2014.

[4]: 贾亚茹, 刘向阳, 刘胜利. 去中心化的安全分布式存储系统[J]. 计算机工程, 2012, 38(3):126-129.