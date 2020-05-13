#! /bin/bash

# Netsetgo is a simple utility that allow us to support network
# functionalities for our network namespace. In particoular:
#       1 - It creates a bridge called 'brg0'
#       2 - It creates Virtual Ethernet Device
#           (http://man7.org/linux/man-pages/man4/veth.4.html)
#       3 - veth is attached to the bridge
#       4 - veth is moved to the new Network namespace
#       5 - a default route is added to the new Network namespace

# moving netsetgo in local bin folder
sudo cp netsetgo /usr/local/bin/

# changing file owner and group of /usr/local/bin/netsetgo.
# owner -> root
# group -> root
sudo chown root:root /usr/local/bin/netsetgo

# change file mode bits.
# user(owner) -> 7    (read, write, execution)
# group       -> 5    (read, execution)
# general     -> 5    (read, execution)
# the first 4 stands for:
# Setuid and Setguid - they are Unix flags that allow users
# to run an executable with the permissions of the executable's
# owner or group respectively and to change behaviour in directories.
sudo chmod 4755 /usr/local/bin/netsetgo