FROM archlinux

RUN \
    --mount=type=bind,src=.,dst=/tmp,rw \
    echo "Build starting..." \
    && cp /tmp/pacman.conf /etc/pacman.conf \
    && cp /tmp/mirrorlist /etc/pacman.d/mirrorlist \
    && cp /tmp/sudoers /etc/sudoers \
    && rm -f /etc/localtime \
    && ln -s /usr/share/zoneinfo/Asia/Shanghai /etc/localtime \
    && useradd -d /home/onix -m -G root -p onix -s /bin/zsh -u 1000 onix \
    && pacman -Sy \
    && pacman -S sudo --noconfirm \
    && pacman -S zsh --noconfirm \
    && pacman -S git --noconfirm \
    && pacman -S curl --noconfirm \
    && pacman -S wget --noconfirm \
    && pacman -S make --noconfirm \
    && pacman -S nasm --noconfirm \
    && pacman -S gcc --noconfirm \
    && pacman -S gdb --noconfirm \
    && pacman -S base-devel --noconfirm \
    && pacman -S bochs --noconfirm \
    && pacman -S qemu-full --noconfirm \
    && pacman -S libcanberra --noconfirm \
    && pacman -Scc --noconfirm \
    && rm -rf /var/log/* \
    && rm -rf /var/cache \
    && rm -rf /var/lib/pacman/sync \
    && rm -rf /root/.bash_history \
    && rm -rf /root/.zsh_history \
    && chown onix:onix -R /home/onix

USER onix

WORKDIR /home/onix

# 配置 zsh / oh my zsh
RUN --mount=src=.,dst=/tmp,rw \
    echo "Setup oh my zsh..." \
    && sh -c "$(curl -fsSL https://raw.githubusercontent.com/ohmyzsh/ohmyzsh/master/tools/install.sh)" \
    && git clone https://github.com/zsh-users/zsh-autosuggestions ${ZSH_CUSTOM:-~/.oh-my-zsh/custom}/plugins/zsh-autosuggestions \
    && git clone https://github.com/zsh-users/zsh-syntax-highlighting.git ${ZSH_CUSTOM:-~/.oh-my-zsh/custom}/plugins/zsh-syntax-highlighting \
    && cp /tmp/.zshrc /home/onix/.zshrc

WORKDIR /home/onix/onix/src

CMD [ "/usr/bin/zsh" ]
