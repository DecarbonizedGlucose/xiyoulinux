套接字：
    成对出现，有俩缓冲区
    s_fd1     s_fd2

转换函数集 h: host  n: net  l: long  s: short
    s用于转换端口号（16位），l用于转换Ip地址（32位）
    uint16_t htons(uint16_t host_unit16);
        32   htonl     32
        16   ntohs     16
        32   ntohl     32

ip地址转换函数
    int inet_pton(int af, const char* src, void* dst); 192.168.0.1（点分十进制，给人看的） -> 二进制
        return 1 on success, 0 当src不是一个有效ip字符串的地址，比如地址不存在，-1 失败并设置errno
        af: AF_INET(ipv4), AF_INET6(ipv6)
        src: 传入，ip地址（点分十进制的字符串）    本地字节序
        dst：传出，转换后的网络字节序的IP地址      网络字节序

    const char* inet_ntop(int af, const void* src, char* dst, socklen_t size); 二进制数 -> 点分十进制
        return dst on success, NULL on failure
        src: 网络字节序ip地址
        dst: 转化后本地字节序的ip
        size: dst的大小

sockaddr地址结构
    struct sockaddr (已停止使用)
    历史遗留问题，需要强转

    struct in_addr { // 这也是历史遗留问题，其他成员死光光了
        in_addr_t s_addr;    ipv4的32位整形地址
    };

    struct sockaddr_in {
        sa_family_in   sin_family; // AF_INET AF_INET6 ...
        in_port_t      sin_port;   // 网络字节序端口号，用htons初始化
        struct in_addr sin_addr;   // 网络地址，用inet_pton
        unsigned char  __pad[X];   // 
    };

    struct sockaddr_in addr;                           // 这里in是internet
    addr.sin_family = AF_INET 或 AF_INET6; // man 7 ip
    addr.sin_port htons(9527);
        //int dst; // 实际开发中不这么写这三行
        //inet_pton(AF_INET, "192.168.22.45", (void*)&dst)
        //addr.sin_addr.s_addr = dst;
    【*】addr.sin_addr.s_addr = htonl(INADDR_ANY); // 而是这么写 取出系统中任意有效的IP地址，整形
    bind(fd, (struct sockaddr*)&addr, sizeof(addr));   // connect函数用法类似
    //这里bind是把fd跟地址绑定一起

各个功能函数
    <sys/socket.h>
    int socket(int domain, int type, int protocol);
        domain: AF_INET/AF_UNIX/AF_INET6
        type: 数据传输协议，流式协议SOCK_STREAM（一般）/抱式协议SOCK_DGRAM
        ptotocol: 代表协议，一般传0
        return新fd 成功， 失败-1 error
    <arpa/inet.h>
    int bind(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
        服务端绑定
        return成功0失败-1,errno
    int listen(int fd, int num);
        设置监听上限: 同时建立连接(进行三次握手)的客户端上限数 num最大是128（系统限制）
        return成功0失败-1,errno
    int accept(int sockfd, struct sockaddr* addr, socklen_t* addrlen);
        addrlen: 传入addr大小，传出客户端addr的实际大小
        addr: 传出与客户端建立连接的地址结构（IP+port）
        阻塞等待客户端连接
        成功返回一个与客户端成功连接的socket fd
        失败 -1 errno
    int connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen);
        使用现有的socket与服务器建立连接
        addr: 服务器的地址结构
        return 0 / -1 errno

socket模型创建基本流程
【*】1 client & 1 server, 一共有3个套接字。一对配药，一个独立（用于服务端检查监听上限）
    server:
        socket()
        bind()
        listen()
        accept()
        loop
            read()
            数据处理逻辑
            write()
        end
        read() 读到0,结束
        close()
    
    client:
        socket()
        connwct()
        loop
            write()
            read()
        end
        close()

三次握手：
    主动发起：发送SYN标志位，请求建立连接。携带【序号】，数据字节数(0)，滑动窗口大小。
    被动接受：发送ACK标志位，携带SYN标志位。携带【序号】，【确认序号】，数据字节数(0)，滑动窗口大小。
    主动发起：发送ACK标志位，应答服务器连接请求，携带【确认序号】。

端口复用：
    在socket和bind之间加入
    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    相关解释比较玄乎，不再过多阐述
