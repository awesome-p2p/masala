masala(1) -- P2P name resolution daemon
=======================================

## SYNOPSIS

`masala`  [-d] [-q] [-h hostname] [-k password] [-r realm] [-p port] [-ba server] [-bp port] [-u username]

## DESCRIPTION

**masala** is a P2P name resolution daemon. It organizes IPv6 addresses in a
distributed hash table. The basic design has been inspired by the Kademlia DHT.
The communication between nodes is realized by using bencode encoded messages on
top of UDP. There are 4 message types: PING, FIND, LOOKUP and ANNOUNCE. By
default, masala sends the first packet to a multicast address. So there is no
configuration necessary within your broadcast domain (LAN). With a bootstrap
server, it is also possible to connect nodes around the globe. A DNS server
can be used as a local upstream DNS server and tries to resolve any
hostname with *.p2p* at the end.

## OPTIONS

  * `-h, --hostname` *hostname*:
    By default /etc/hostname is used to determine the hostname.

  * `-k, --key` *password*:
	Setting a password results in encrypting each packet with AES256. The
	encrypted packet is encapsulated in bencode. With this action you
	effectively isolate your nodes from the rest of the world.

  * `-r, --realm` *realm*:
	Creating a realm affects the lookup process and the way how you announce
	your hostname to the swarm. It helps you to isolate your nodes and be part
	of a bigger swarm at the same time. This can be useful if you do not have
	your own bootstrap server and do not want to get mixed up with the rest of
	the swarm. You do not have problems with duplicate hostnames either as long
	as you do not share your realm with others.

  * `-p, --port` *port*:
	Listen to this port (Default: UDP/8337)

  * `-ba, --bootstrap-addr` *server*:
	Use server as a bootstrap server. The server can be an IPv6 address, a FQHN like www.example.net or even a IPv6 multicast address. (Default: ff0e::1)

  * `-bp, --bootstrap-port` *port*:
	The bootstrap server will be addressed at this port. (Default: UDP/8337)

  * `-u, --user` *username*:
    When starting as root, use -u to change the UID.

  * `-d, --daemon`:
	Start as a daemon and run in background. The output will be send to syslog.

  * `-q, --quiet`:
	Be quiet and do not log anything.

  * `--help`:
	Show a summary of all available command line parameters.

  * `-da, --dns-addr`:
	Bind the DNS server interface to this address (Default: ::1).

  * `-dp, --dns-port`:
	Bind the DNS server interface to this port (Default: 3444).

  * `-di, --dns-ifce`:
	Bind the DNS server interface to this interface (Default: &lt;any&gt;).

## EXAMPLES

Announce the hostname *fubar.p2p* with the encryption password *fubar*:

	$ masala -h fubar.p2p -k fubar

## BUGS

Lack of IPv6 support by the providers.
