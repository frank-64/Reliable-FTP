# Reliable-FTP

# Overview

This project was coursework as part of the Operating Systems and Networks module during my second year at Newcastle University.

# Aim

This coursework gives you the opportunity to develop your skills in network socket programming using
the C programming language. The aim of this coursework is to build a Reliable File Transfer (RFT)
protocol that works on top of UDP and guarantees reliable delivery of a file between a client and a
server. 

# What I learned during this coursework

* The use sockets in C programming
* The use of socket and file descriptors
* The relationship between client and server across networks.
* Sendto() and recv() for use with data segments as packets and acknowledgements between the client and the server.
* How corruptions in the data segment are resolved and retransmitted after a timeout placed on the socket.
