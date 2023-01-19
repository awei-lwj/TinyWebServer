# TinyWebServer

## Introduction

Linux下的一个C++轻量级Web服务器。

**Key Points**:

- 使用 **线程池** + **非阻塞socket** +  **epoll(ET与LT均实现)** + **事件处理(Reactor + 模拟Proactor)** 的并发模型
- 使用 **状态机**解析HTTP请求报文，支持解析**GET和POST请求**
- 使用 priority queue 实现的**最小堆结构管理定时器**，使用标记删除，以支持惰性删除，提高性能
- 使用 **RAII**手法封装互斥器(pthread_mutex_t)、 条件变量(pthread_cond_t)等线程同步互斥机制，管理文件描述符等资源，避免锁争用
- 访问**服务器数据库**实现web端**用户注册、登录功能**，可以**请求服务器图片和视频文件**
- 实现**同步/异步日志系统**，记录服务器运行状态
- 内置一个基于跳表实现的**轻量级key-value型存储引擎**，使用C++实现。可以实现的**操作**有：插入数据、删除数据、查询数据、文件落盘、文件加载数据以及数据库显示大小

**Tests**:

- 经过WebBench压力测试可以实现**上万的并发连接数据交换**
- 随机读写的情况下，所实现的轻量型key-value存储引擎，每秒可处理的的**写请求数量（QPS）**：12.2819 W，每秒可处理的**读请求数目(QPS)**: 14.5819 W。

**TODO**:

- WebServer以及内置的key-value存储引擎的测试不是全自动化的
- 加上一致性协议，提供分布式的存储服务
- 实现循环缓冲区来实现同步/异步日志系统
- 实现类 MutexLockGuard，避免锁争用问题
- 完善eventlooppool进行事件管理和使用C++11的shared_ptr的智能指针进行内存管理
- 完善skiplist来代替mysql数据库提供分布式服务

## Function Demonstration

### 1. Key-Value存储引擎

Key-Value存储引擎的stress test在下文，这里演示Key-Value相应的功能

SkipList API:

- [x] insert_element(进行element插入)
- [x] delete_element(进行element删除)
- [x] serach_element(进行element查找)
- [x] display_list(进行skiplist展示)
- [x] dump_file(数据落盘)
- [x] load_file(数据加载)
- [x] size(返回数据落盘)
  
--------
![在这里插入图片描述](https://img-blog.csdnimg.cn/86aad683aaec4c3c9216abefda439f34.png#pic_center)![在这里插入图片描述](https://img-blog.csdnimg.cn/66a6e138c372428c9de4365a59b30f7d.png#pic_center)


### 2.TinyWebServer

- [x] 注册
- [x] 已有用户
- [x] 登录
- [x] 密码错误
- [x] 图片测试(海绵宝宝11.8M)，视频测试(Kruskal算法和Prim算法，22.5M)
- [x] Making friends with awei 


[video(video-ixKOhPg5-1674019344246)(type-csdn)(url-https://live.csdn.net/v/embed/270761)(image-https://video-community.csdnimg.cn/vod-84deb4/36ba9c7096e571edbfc76723a78f0102/snapshots/27141e426ea947aaa88c564a0a3def70-00005.jpg?auth_key=4827614704-0-0-e15be31db2d61fc71dc7b0194682568b)(title-TinyWebServer)]

## Framework

### 1.Key-Vaule Storage Enginee based on the skip list

![在这里插入图片描述](https://img-blog.csdnimg.cn/035de4884e9b404dab9784a48e8382a7.png#pic_center)


### 2. TinyWebServer
![在这里插入图片描述](https://img-blog.csdnimg.cn/feb99cf6dcba4d65ad5aa5ef6dea7750.png#pic_center)

## Configuration

- VMare Workstaion + Ubuntu 22.10
- C++ 11 Configuration
- MySQL
- make + cmake + shell
- Webbench

## Build

### 1. Key-Value Storgae Enginee

Stress Tests:

~~~shell
cd test/
sh skiplist_stress_test.sh // 数据量一开始设定为一百万
~~~

Key-Value enginee operation mode

~~~shell
make
../bin/skiplist_test
~~~

### 2.TinyWebServer

- 虚拟机测试环境
  - Ubuntu 22.10
  - mysql  Ver 8.0.31-0ubuntu2 for Linux on x86_64 ((Ubuntu))
  
- 游览器测试环境
  - Linux/Windows
  - Chrone/FireFox
  - 其他游览器没有测试

- 在进行**游览器测试**之前请先确保已经安装好MySQL以及相应的MySQL库

  ~~~shell
  // Install mysql and mysql dependency library
  sudo apt install mysql-server -y
  sudo apt install libmysqlclient-dev -y
  ~~~

  ~~~shell
  // 建立yourdb库
  create database yourdb;

  // 创建user表
  USE yourdb;
  CREATE TABLE user(
      username char(50) NULL,
      passwd char(50) NULL
  )ENGINE=InnoDB;

  // 添加数据
  INSERT INTO user(username, passwd) VALUES('name', 'passwd'); 
  ~~~

- 修改main.cpp中的数据库的初始化信息

  ~~~shell
  //数据库登录名,密码,库名
  string user = "root";
  string passwd = "root";
  string databasename = "yourdb";
  ~~~

- build以及启动server
  
  ~~~shell
  sh ./build.sh
  ./server
  ~~~

- 用ifconfig查找本机的ip地址，然后在游览器端口处进行相应的IP地址访问

  ~~~shell
  ifconfig

  // 游览器端
  ip:9096
  ~~~

- 在进行**Webbench测试**之前请先确保已经安装好Webbench以及相应的依赖库
  
  ~~~shell
  sudo apt install ctags
  cd test/Webbench
  make
  ~~~

## Command for server

~~~shell
./server [-p port] [-l LOGWrite] [-m TRIGMode] [-o OPT_LINGER] [-s sql_num] [-t thread_num] [-c close_log] [-a actor_model]
~~~

并非全部都要使用，个性化设置即可

- -p，自定义端口号
  - 默认9006

- -l，选择日志写入方式，默认同步写入
  - 0，同步写入
  - 1，异步写入
  
- -m，listenfd和connfd的模式组合，默认使用LT + LT
  - 0，表示使用LT + LT
  - 1，表示使用LT + ET
  - 2，表示使用ET + LT
  - 3，表示使用ET + ET
  
- -o，优雅关闭连接，默认不使用
  - 0，不使用
  - 1，使用
- -s，数据库连接数量
  - 默认为8
- -t，线程数量
  - 默认为8
- -c，关闭日志，默认打开
  - 0, 打开日志
  - 1，关闭日志
- -a，选择反应堆模型，默认Proactor
  - 0，Proactor模型
  - 1，Reactor模型

## Tests

### 1.Key-Value Storgae Enginee

Insert element operation
| 插入的数据规模(万条)| 耗费时间 |
|---|---|
| 1 | 0.0625142 |
| 10| 0.688054  |
| 15| 1.2213   |
| 20| 1.77769   |
| 50| 5.33906   |
| 100| 11.5311  |

每秒可以处理的写请求数目（QPS）：12.2819 W

Get element operation
| 读取的数据规模(万条)| 耗费时间 |
|---|---|
| 1 | 0.0443168 |
| 10| 0.96824  |
| 15| 1.12037   |
| 20| 1.17522   |
| 50| 4.14332   |
| 100| 9.7482  |

每秒可以处理的读请求数目（QPS）：14.5819 W

### 2.WebServer

在关闭服务器日志之后，使用Webbench对服务器进行压力测试，对listen_fd和connection_fd分别采用ET或LT模式，均可以实现近万的并发连接以下列出组合后的测试结果

- Proactor，TRIGMode = 0, Listen_fd = LT, connection_fd = LT，QPS = 202927
![在这里插入图片描述](https://img-blog.csdnimg.cn/712a247abb1a43148fbaeb819a895b87.png#pic_center)
- Proactor，TRIGMode = 1, Listen_fd = LT, connection_fd = ET, QPS = 216614
![在这里插入图片描述](https://img-blog.csdnimg.cn/fbf2d5ff8e8d493aa8a280634bd160f5.png#pic_center)
- Proactor，TRIGMode = 2, Listen_fd = ET, connection_fd = LT, QPS = 124588
![在这里插入图片描述](https://img-blog.csdnimg.cn/5a04c90fb2744ab19c636f9f7595e8bb.png#pic_center)

- Proactor，TRIGMode = 3, Listen_fd = ET, connection_fd = ET, QPS = 180345
![在这里插入图片描述](https://img-blog.csdnimg.cn/460cc98e84974c8584a391dfee005d7e.png#pic_center)


## Debug过程中的一些难点
  
- [x] 小的bug不多说
- [x] 由于并发模型构建的不完善，seg implement在查看了游侠的高性能网络编程以及学会了利用GDB调试来进行查看线程安全问题
- [x] 书上的http对大文件进行读取存在问题，在查阅资料和问了大佬之后得以修复

~~~cpp
// 原先代码
bool http_conn::write()
{
    int temp=0;
    int bytes_have_send=0;
    int bytes_to_send=m_write_idx;
    if(bytes_to_send==0)
    {
        modfd(m_epollfd,m_sockfd,EPOLLIN);
        init();
        return true;
    }
    while(1)
    {
        temp=writev(m_sockfd,m_iv,m_iv_count);
        if(temp<=-1)
        {
            if(errno==EAGAIN)
            {
                modfd(m_epollfd,m_sockfd,EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }
        bytes_to_send-=temp;
        bytes_have_send+=temp;
        if(bytes_to_send<=bytes_have_send)
        {
            unmap();
            if(m_linger)
            {
                init();
                modfd(m_epollfd,m_sockfd,EPOLLIN);
                return true;
            }
            else
            {
                modfd(m_epollfd,m_sockfd,EPOLLIN);
                return false;
            }
        }
    }
}

~~~

---

~~~cpp
// Debug
bool http_conn::write()
{
    int temp = 0;

    int newadd = 0;

    if (bytes_to_send == 0)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);
        init();
        return true;
    }

    while (1)
    {
        temp = writev(m_sockfd, m_iv, m_iv_count);

        if (temp >= 0)
        {
            bytes_have_send += temp;
            newadd = bytes_have_send - m_write_idx;
        }
        else
        {
            if (errno == EAGAIN)
            {
                if (bytes_have_send >= m_iv[0].iov_len)
                {
                    m_iv[0].iov_len = 0;
                    m_iv[1].iov_base = m_file_address + newadd;
                    m_iv[1].iov_len = bytes_to_send;
                }
                else
                {
                    m_iv[0].iov_base = m_write_buf + bytes_have_send;
                    m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
                }
                modfd(m_epollfd, m_sockfd, EPOLLOUT, m_TRIGMode);
                return true;
            }
            unmap();
            return false;
        }
        bytes_to_send -= temp;
        if (bytes_to_send <= 0)
        {
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN, m_TRIGMode);

            if (m_linger)
            {
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}


~~~
