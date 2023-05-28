IFACE:=onix

BR0:=/sys/class/net/br0
TAPS:=	/sys/class/net/tap0 \
		/sys/class/net/tap1 \
		/sys/class/net/tap2 \

.SECONDARY: $(TAPS) $(BR0)

# 网桥 IP 地址
BNAME:=br0
IP0:=192.168.111.22
MAC0:=5a:5a:5a:5a:5a:22
GATEWAY:=192.168.111.2

$(BR0):
	sudo ip link add $(BNAME) type bridge
	sudo ip link set $(BNAME) type bridge ageing_time 0

	sudo ip link set $(IFACE) down
	sudo ip link set $(IFACE) master $(BNAME)
	sudo ip link set $(IFACE) up

	sudo ip link set dev $(BNAME) address $(MAC0)
	sudo ip link set $(BNAME) up

	# sudo iptables -A FORWARD -i $(BNAME) -o $(BNAME) -j ACCEPT
	sudo iptables -F
	sudo iptables -X
	sudo iptables -P FORWARD ACCEPT
	sudo iptables -P INPUT ACCEPT
	sudo iptables -P OUTPUT ACCEPT

	sudo ip addr add $(IP0)/24 brd + dev $(BNAME) metric 10000
	sudo ip route add default via $(GATEWAY) dev $(BNAME) proto static metric 10000
	# sudo dhclient -v -4 br0

br0: $(BR0)
	-

/sys/class/net/tap%: $(BR0)
	sudo ip tuntap add mode tap $(notdir $@)
	sudo ip link set $(notdir $@) master $(BNAME)
	sudo ip link set dev $(notdir $@) up

tap%: /sys/class/net/tap%
	-
