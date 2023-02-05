# Kiwitun - a simple IPIP tunneling daemon

  

Kiwitun is an easy to use module-independent IPIP (4in4) and IP6IP (6in4) tunneling daemon.

The main purpose of this project was to provide IPIP tunneling to Linux systems with no Loadable Kernel Module (LKM) support. The program is also shell- and file-independent, as it uses only well-defined kernel interfaces (netlink and rtnetlink) for communication.

  

Kiwitun also provides periodic hostname to IP resolution for the remote endpoint.

  

## Compilation

### Prerequisites

Kiwitun uses Cmake as its build system. Following packages are need to be installed before compilation:
```
gcc
cmake
make
```

For Debian-based distributions (Debian, Ubuntu etc.) you will also need:
```
build-essential
```
### Downloading and building

Download, build and compile kiwitun with following commands:

```bash
git clone  https://github.com/sq8vps/kiwitun
cd  kiwitun
mkdir build && cd  build
cmake ..
make
```
From now on you should be able to run kiwitun from current directory (*build*).  
**Notice**: If you are building kiwitun without Cmake you need to link *pthread* library to the executable.

### Installation

To make kiwitun accessible from any directory you need to install it with:

```bash
sudo install  kiwitun  /usr/local/bin
```

## Usage

Kiwitun **must** be run as root. Non-root users cannot create TUN interfaces.

### Available switches

Tunneling modes:

-  ```-4, --4in4``` - enable IPIP (4in4) tunneling.
-  ```-6, --6in4``` - enable IP6IP (6in4) tunneling.  

At least one tunneling mode must be selected to start kiwitun.

Tunnel settings:
-  ```-r, --remote=address``` - use given hostname or IP as a remote endpoint address. The routing table is used when remote hostname/address is not set.
-  ```-l ,--local=address``` - use given IP as a local endpoint address. Kernel selects appropriate address if not set.
-  ```-t, --ttl=value``` - use given TTL/hop limit value for encapsulated and ICMP packets.
-  ```-i, --ifname=name``` - use given TUN interface name. Kernel selects appropriate name if not set.

Other settings:

-  ```--refresh=time``` - resolve remote endpoint hostname every given period of time (in minutes). Default refresh period is used when not set explicitly.
-  ```-d, --no-daemon``` - do not run as a daemon.
-  ```--log-level=level``` - set logging level. Lower value means less logging. Valid values are 0 to 7 (values higher than 7 are clipped to 7). ```--log-level=7``` is equivalent to ```--verbose```. Setting it to 0 should disable logging completely.
-  ```-v, --verbose``` - verbose/debug mode: print/log everything. Equivalent to ```--log-level=7```.

Version and help:

-  ```--version``` - print version information
-  ```-h, --help``` - print help page  

### Kiwitun with fixed remote endpoint
Typical point-to-point tunnels use fixed remote endpoints,  i.e. the remote endpoint address is set when creating a tunnel and can't be changed afterwards. This also means that only packets originating from given remote endpoint are passed and others are dropped.  
In this case kiwitun can be run with:
```bash
sudo kiwitun [-4, -6] -r <remote_address>
```
Where ```<remote_address>``` is the remote endpoint hostname or address.   
Interface (inner) address **must** be set with:
```bash
sudo ip address add <tunaddress/mask> dev tun0
```
Where ```<tunaddress/mask>``` is the address and netmask (CIDR format) and ```tun0``` is the tunnel interface name (define it with ```-i <name>``` when starting kiwitun or use ```ip link``` to check its name).  
To route another address (or network) via the tunnel:
```bash
sudo ip route add <address/mask> dev tun0
```
Where ```<address/mask>``` is the address to route and its netmask (CIDR format) and ```tun0``` is the tunnel interface name.  
Add appropriate firewall rules if needed.

### Kiwitun with dynamic remote endpoint
The tunnel can also have dynamically chosen remote endpoints. The remote endpoint address is chosen by checking the destination address of a packet being tunnelled. This also requires appropriate routes to be set in the OS.  
In this case kiwitun can be run with:
```bash
sudo kiwitun [-4, -6]
```
Interface (inner) address **should** be set with:
```bash
sudo ip address add <tunaddress/mask> dev tun0
```
Where ```<tunaddress/mask>``` is the address and netmask (CIDR format) and ```tun0``` is the tunnel interface name (define it with ```-i <name>``` when starting kiwitun or use ```ip link``` to check its name).  
Appropriate routes **must** be added in order to give encapsulator remote endpoint address for every destination address.  
For IPIP (4in4) tunnels:
```bash
sudo ip route add <address/mask> via <remote> dev <tun0> onlink
```
For IP6IP (6in4) tunnels:
```bash
sudo ip route add <address/mask> via ::ffff:<remote> dev <tun0> onlink
```
Where ```<address/mask>``` is the destination address and netmask (CIDR format), ```<remote>``` is the remote endpoint IPv4 address and  ```tun0``` is the tunnel interface name.  
Add appropriate firewall rules if needed.  
**Notice 1**: Default gateways are not used as they don't point to any actual remote endpoint. Packets with unresolvable remote endpoint are dropped and ICMP Destination Unreachable is returned to the sender.  
**Notice 2**: As there is no fixed remote address, all valid encapsulated packets received will be decapsulated and sent further. Appropriate firewall rules must be added to filter out unwanted packets.

### Examples
#### IPIP tunnel with fixed remote endpoint hostname
- Tunnel (inner) address is 10.0.0.1/30, the other side is 10.0.0.2/30
- Tunnel name is *ktun0*
- Remote endpoint is at *tun.domain.com*
- 10.1.0.0/24 is routed via the tunnel
```bash
sudo kiwitun -4 -r tun.domain.com -i ktun0
sudo ip address add 10.0.0.1/30 dev ktun0
sudo ip route add 10.1.0.0/24 dev ktun0
```
  
#### IPIP+IP6IP tunnel with dynamic remote endpoint
- Tunnel (inner) address is 10.0.0.1/30
- Tunnel name is *ktun0*
- 10.1.0.0/24 is tunneled via remote endpoint 1.2.3.4
- fc00:1::/32 is tunneled via remote endpoint 5.6.7.8 
```bash
sudo kiwitun -4 -6 -i ktun0
sudo ip address add 10.0.0.1/30 dev ktun0
sudo ip route add 10.1.0.0/24 via 1.2.3.4 dev ktun0 onlink
sudo ip route add fc00:1::/32 via ::ffff:5.6.7.8 dev ktun0 onlink
```
## Versions, changes, bugs etc.
See [changelog](changelog.md).

## Contributing
Any contributions are appreciated. Feel free to create issue tickets for any problem or feature request.

## License
Kiwitun is licensed under [GNU GPL 3.0](LICENSE).
