IFACE:=eth0

BR0:=/sys/class/net/br0
TAPS:=	/sys/class/net/tap0

.SECONDARY: $(TAPS) $(BR0)

IP0:=172.16.16.10
MAC0:=5a:5a:5a:5a:5a:22

$(BR0):
	# 要先将/etc/sysctl.conf文件中net.ipv4.ip_forward=1前的注释去掉
	sudo sysctl -p
	sudo iptables -t nat -A POSTROUTING -s $(IP0)/24 -o $(IFACE) -j MASQUERADE

br0: $(BR0)
	-

/sys/class/net/tap%:
	sudo ip tuntap add mode tap $(notdir $@)
	sudo ip link set dev $(notdir $@) address $(MAC0)
	sudo ifconfig $(notdir $@) $(IP0) promisc up

tap%: /sys/class/net/tap%
	-

clean-tap:
	sudo ip tuntap delete mode tap tap0




