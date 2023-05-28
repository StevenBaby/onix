import sys
import logging
import warnings
from scapy.all import (
    TunTapInterface,
    Ether,
    ARP,
    IP,
    UDP,
    TCP,
    ICMP,
)

logging.getLogger("scapy").setLevel(logging.ERROR)
warnings.filterwarnings("ignore", module="scapy")

logging.basicConfig(
    stream=sys.stdout,
    level=logging.DEBUG,
    format='[%(asctime)s] [%(module)s:%(lineno)d] %(levelname)s %(message)s',)

logger = logging.getLogger("net")


rmac = '5a:5a:5a:5a:5a:44'
raddr = '192.168.111.44'

lmac = '5a:5a:5a:5a:5a:33'
laddr = '192.168.111.33'

bmac = '5a:5a:5a:5a:5a:22'
baddr = '192.168.111.22'

hmac = '5a:5a:5a:5a:5a:11'
haddr = '192.168.111.11'

gateway = '192.168.111.2'
gmac = None

broadcast = "ff:ff:ff:ff:ff:ff"

taps = {i: None for i in range(3)}


def get_tap(i: int = 1) -> TunTapInterface:
    global taps
    if i not in taps:
        return None
    if not taps[i]:
        taps[i] = TunTapInterface(f'tap{i}')
    return taps[i]


def get_gmac(tap):
    global gmac
    if gmac:
        return gmac
    pkt = Ether(src=bmac, dst=broadcast)
    pkt /= ARP(pdst=gateway)
    out = tap.sr1(pkt, verbose=False)
    gmac = out[Ether].src
    return gmac
