masala(1) -- P2P name resolution daemon
=======================================

## SYNOPSIS

`masala`  [-d] [-q] [-h hostname] [-k password] [-i interface ] [-p port] [-ba server] [-bp port] [-u username]

## DESCRIPTION

**masala** is a P2P name resolution daemon. It organizes IPv6 addresses in a
distributed hash table. The basic design has been inspired by the Kademlia DHT.
The communication between nodes is realized by using bencode encoded messages on
top of UDP. There are 4 message types: PING, FIND, LOOKUP and ANNOUNCE. By
default, masala sends the first packet to a multicast address. So there is no
configuration necessary within your broadcast domain (LAN) and will continue to do so
in intervals until another node is found. With a bootstrap server, it is also possible
to connect nodes around the globe. The following features are optional:

  * An interactive shell and a tool to send commands to masala. This way scripts can easily issue commands: `masla-ctl lookup foo.p2p`
  * A simple DNS server interface that can be used like a local upstream DNS server.
  * A simple web server interface can resolve queries: `http://localhost:8080/foo.p2p`
  * Name Service Switch (NSS) support through /etc/nsswitch.conf.

## OPTIONS

  * `-h, --hostname` *hostname*:
    The hostname whose sha1 hash will be announced. Can also be a 20 Byte hexadecimal string. (Optional)

  * `-p, --port` *port*:
	Bind to this port. (Default: UDP/8337)

  * `-i, --interface` *interface*:
	Bind to this interface (Default: &lt;any&gt;)

  * `-ba, --bootstrap-addr` *server*:
	Use server as a bootstrap server. The server can be an IPv6 address, a FQHN like www.example.net or even a IPv6 multicast address. (Default: ff0e::1)

  * `-bp, --bootstrap-port` *port*:
	The bootstrap server will be addressed at this port. (Default: UDP/8337)

  * `-u, --user` *username*:
    When starting as root, use -u to change the UID. (Optional)

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
	Bind the DNS server to this interface (Default: &lt;any&gt;).

  * `-wa, --web-addr`:
	Bind the Web server interface to this address (Default: ::1).

  * `-wp, --web-port`:
	Bind the Web server interface to this port (Default: 8080).

  * `-wi, --web-ifce`:
	Bind the Web server to this interface (Default: &lt;any&gt;).


## EXAMPLES

Announce the hostname *fubar.p2p*:

	$ masala -h fubar.p2p

## BUGS

  * Cannot resolve own host id without other nodes present.
  * Lack of IPv6 support by the providers.
