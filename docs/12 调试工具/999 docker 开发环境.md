# docker 开发环境 

> 注：docker 开发环境测试中，可能会有问题！！！

关于 docker 的学习参考 docker practice [^docker_practice]。

## 安装 docker 

1. 在 Archlinux 上安装 docker：

```sh
sudo pacman -S docker
sudo pacman -S docker-buildx
sudo pacman -S docker-compose
```

2. 启动 docker

```sh
# 自启动 docker
sudo systemctl enable docker

# 重启 docker
sudo systemctl restart docker
```
3. 添加用户到 docker 组

运行 `docker` 命令需要 `root` 权限，但直接使用 `root` 是墙裂不推荐的方式，我们应该尽可能避免使用 `root`，一个方法就是把当前用户加入 `docker` 用户组，这样就可以以当前用户身份执行 `docker` 命令了。

根据需要执行以下命令：

```sh
# 查看当前用户的用户组
groups

# 添加 docker 组
sudo groupadd docker

# 添加当前用户到 docker 组
sudo gpasswd -a $USER docker

# 刷新组
newgrp docker

# 再次查看当前用户的用户组
groups
```

## 一些常用的 docker 命令

查看 docker 状态

    docker info

检查正在运行的容器:

    docker ps

列出在主机运行的所有容器，为删除做准备:

    docker ps -a

停止一个运行的容器:

    docker stop <CONTAINER ID>

杀死还在运行的容器:

    docker kill <CONTAINER ID>

通过ID删除列出的所有容器:

    docker rm <CONTAINER ID>

列出所有的docker镜像:

    docker images

通过ID删除所有镜像:

    docker rmi <IMAGE ID>

查找镜像：

    docker search python

执行镜像：

    docker run python python -c "print('hello world')"

## 配置 docker 开发环境

docker 开发环境的搭建，以及 vscode 中的使用方法：

1. 首先 vscode 需要安装插件 Docker [^docker_plugin], Dev Containers [^dev_containers]；

2. 构建开发镜像：

        make build

    构建的过程需要访问 github，如果不能访问，请提前配置 http 代理，类似于：

        export http_proxy=http://192.168.1.1:11111
        export https_proxy=http://192.168.1.1:11111


3. 在支持 x11 的终端，禁用 X11 访问控制，运行开发镜像：

        xhost + 
        make run 

    如果已有容器，也就是上面只执行一次，可以使用 start 和 exec：

        xhost+
        make start
        make exec

4. 然后 Attach to Container，就可以和正常的 Remote 一样开发了，具体细节参考 [^dev_container]。

## 一些解释

为了与项目之前的行为方式尽可能一致，docker 环境基于 Archlinux 搭建，但是 docker 环境与真是客户机还是有一些区别。一下是遇到的问题，以及处理的方法及其解释。希望有用。

- X11 图形界面的问题 [^x11]，以及 X11 的解释 [^xhost]；

- 音频输出 [^audio]，之前使用的后端是 PauseAudio，但是看起来 docker 中配置的方式略显复杂，遂改成了更简单的 alsa，具体细节参考 [^alsa_pa]；

- qemu 警告 Libcanberra [^Libcanberra]，没有安装，运行 qemu 时，会报警告；

- 创建子网络及配置 [^net]，网络配置用于后期开发网络协议栈；

## 参考

[^docker_plugin]: <https://marketplace.visualstudio.com/items?itemName=ms-azuretools.vscode-docker>
[^dev_containers]: <https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers>
[^docker_practice]: <https://github.com/yeasy/docker_practice>
[^dev_container]: <https://code.visualstudio.com/docs/devcontainers/containers>
[^x11]: <https://github.com/sickcodes/Docker-OSX/issues/7>
[^audio]: <https://www.kraxel.org/blog/2020/01/qemu-sound-audiodev/>
[^alsa_pa]: <https://github.com/mviereck/x11docker/wiki/Container-sound:-ALSA-or-Pulseaudio>
[^Libcanberra]: <https://wiki.archlinux.org/title/Libcanberra>
[^net]: <https://stackoverflow.com/questions/43981224/docker-how-to-set-iface-name-when-creating-a-new-network>
[^xhost]: <https://linux.die.net/man/1/xhost>
