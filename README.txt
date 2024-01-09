=========================
Client - Server TCP - UDP
=========================

For the server implementation, I considered various details and
incorporated some of my own ideas. Multiplexing I/O is used to
manage all server inputs (UDP publishers, TCP subscribers, stdin).
Sockets are created for both UDP and TCP connections, with the TCP
socket being passive, listening for new connection requests from
subscribers.

The server receives messages from UDP publishers and converts them
into the required format for transmission to TCP subscribers
(as specified in the prompt). During unsubscription, it first checks
whether the TCP client was subscribed to the topic to avoid
unnecessary entries.

Upon the disconnection of a subscriber, their topics, along with SF
(Store and Forward) and the message number for potential retransmission
upon reconnection, are saved. For the subscriber implementation, a
communication socket is created with the subscriber, Nagle's algorithm
is disabled, and it is multiplexed with the keyboard to receive subscribe,
unsubscribe, and exit commands. When a message is received from the server,
it contains precisely the fields that the TCP client needs to display, and
the client promptly performs the display.
