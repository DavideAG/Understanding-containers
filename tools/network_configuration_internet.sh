#! /bin/bash

# adding a router called r1
polycubectl router add r1

# adding a port called to_ns in the same network of the namespace
polycubectl r1 ports add to_ns ip=10.10.10.1/24

# adding a port called to_internet
polycubectl r1 ports add to_internet

# connecting to_ns port to brg0 veth
polycubectl connect r1:to_ns brg0

# connecting to_internet to the physical interface
# change wlp58s0 with the right interface name!
polycubectl connect r1:to_internet wlp58s0

# default route -> check it using 'ip route'
polycubectl r1 route add 0.0.0.0/0 192.168.43.1

# adding a nat
polycubectl nat add nat1

# attaching nat to to_internet portt
polycubectl attach nat1 r1:to_internet

# enable masquerade
polycubectl nat1 rule masquerade enable
