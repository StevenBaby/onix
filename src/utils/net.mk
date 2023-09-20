IFACE:=$(shell sudo ip -o -4 route show to default | awk '{print $$5}')

TAP0:=/sys/class/net/tap0

.SECONDARY: $(TAP0)

# 网桥 IP 地址
GATEWAY:=172.16.16.1

$(TAP0):
	sudo ip tuntap add mode tap $(notdir $@) user $(USER)
	sudo ip addr add $(GATEWAY)/24 dev $(notdir $@)
	sudo ip link set dev $(notdir $@) up

	sudo sysctl net.ipv4.ip_forward=1
	sudo iptables -t nat -A POSTROUTING -s $(GATEWAY)/24 -o $(IFACE) -j MASQUERADE
	sudo iptables -A FORWARD -i $(notdir $@) -o $(IFACE) -j ACCEPT

tap0: $(TAP0)
	-
