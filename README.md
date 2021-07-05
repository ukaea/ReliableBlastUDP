# An implementation of Reliable Blast UDP

This is an basic implementation of a [paper](https://www.evl.uic.edu/cavern/papers/cluster2002.pdf) by Eric He, Jason Leigh, Oliver Yu,
Thomas DeFanti from the Electronic Visualization Laboratory, University of Illinois at Chicago.

Reliable Blast UDP is a protocol designed to send large files quickly over a network.

It works by primarily sending data via UDP. UDP is fast but not reliable, so both
the sender and receiver maintain and track the number of blocks that are sent and received.
If the receiver has not received a certain block it will tell the sender to send it again.

So far the implementation has only been tested on a single machine and not over a network.

However, metrics are comparable to the initial paper, even with a simulated version
of packet loss.