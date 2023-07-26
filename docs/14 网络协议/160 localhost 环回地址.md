# localhost 环回地址 

环回地址 [^loopback] 为 `127.0.0.1/8`，表示本机地址；`/8` 意思是说 `127.x.x.x`，都是本机地址，和 `127.0.0.1` 是一样的，当然，这个设计很傻，毫无疑问，浪费了很多地址。

`localhost` 是一个主机名，用来表示本机，一般 `localhost` 会被域名解析为 `127.0.0.1`；

## 参考

[^loopback]: <https://en.wikipedia.org/wiki/Localhost>
