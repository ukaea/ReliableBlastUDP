#pragma once
// Design goals
// --> Sockets get complicated, needs to be easily debuggable
// --> Needs an api that is semi-platform agnostic (given that most of the API is posix)

#ifdef _WIN32
#define _WIN32_WINNT  0x501 // Means I can use getaddrinfo....why i dunno
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <WinSock2.h>
#include <winsock.h>
#include <ws2tcpip.h>

#elif __linux__

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> 
#include <poll.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>

#endif

#ifdef _WIN32
// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib") // only works with msvc
#endif 

#ifdef _WIN32 

namespace rse {
    namespace sk {
        typedef SOCKET SocketHandle;
        const int SK_ERROR_SOCKET = SOCKET_ERROR;
        const unsigned int SK_INVALID_SOCKET = INVALID_SOCKET;
        const unsigned int SK_NO_ERROR = NO_ERROR;
    }
}
#elif __linux__
namespace rse {
    namespace sk {
        typedef int32_t SocketHandle;
        const int SK_ERROR_SOCKET = -1;
        const int SK_INVALID_SOCKET = -1;
        const int SK_NO_ERROR = 0;
    }
}
#endif

// For testing
//#define RSE_TEST_SOCKET_SEND_FAILED
//#define RSE_TEST_SOCKET_RECV_FAILED
//#define RSE_TEST_SOCKET_PACKET_LOSS

namespace rse {

    namespace sk {

        typedef int SocketError;

        // Prints a formatted string to the screen along with the last error
        void ErrorMessage(const char* message, ...) {

#ifdef _WIN32
            printf("Error [%d]: ", WSAGetLastError());
#elif __linux__
            printf("Error [%d]: ", errno);
#endif

            va_list args;
            va_start(args, message);
            vprintf(message, args);
            va_end(args);
            printf("\n");
        }

        // Starts up up Easy Sockets
        SocketError Startup() {

#ifdef _WIN32
            WSADATA wsaData;
            int returnCode = WSAStartup(MAKEWORD(2, 2), &wsaData);
            if (returnCode != 0) {
                ErrorMessage("WSAStartup failed with error [%d]", returnCode);
                return SK_ERROR_SOCKET;
            }
#endif

            return SK_NO_ERROR;
        }

        // Cleans up easy sockets
        void Cleanup() {
#ifdef _WIN32
            WSACleanup();
#endif
        }

        // Uses a hinted address info to find an address that matches the hostname and port number
        // The resulting address must be freed at some point
        SocketError GetAddrInfo(const char* hostname, const char* port, addrinfo* hintaddr, addrinfo** resultAddr) {

            int returnCode = getaddrinfo(hostname, port, hintaddr, resultAddr);
            if (returnCode != 0) {
                ErrorMessage("getaddrinfo failed with error [%d]", returnCode);
                return SK_ERROR_SOCKET;
            }

            return SK_NO_ERROR;
        }

        // Creates a socket. The isBlocking determines if the socket
        // has blocking or non-blocking IO
        SocketHandle Socket(addrinfo* addr, bool isBlocking) {

            SocketHandle sock = SK_INVALID_SOCKET;
            sock = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
            if (sock == SK_INVALID_SOCKET) {
                ErrorMessage("Socket failed [%d]", sock);
                return SK_INVALID_SOCKET;
            }

#ifdef _WIN32
            //-------------------------
            // Set the socket I/O mode: In this case FIONBIO
            // enables or disables the blocking mode for the 
            // socket based on the numerical value of iMode.
            // If iMode = 0, blocking is enabled; 
            // If iMode != 0, non-blocking mode is enabled.
            u_long iMode = !isBlocking;
            int result = ioctlsocket(sock, FIONBIO, &iMode);
            if (result != SK_NO_ERROR) {
                ErrorMessage("ioctlsocket failed with [%d]", result);
                return SK_INVALID_SOCKET;
            }
#elif __linux__

            int flags = fcntl(sock, F_GETFL, 0);
            flags = flags & (~O_NONBLOCK);
            int result;
            if (isBlocking)
                result = fcntl(sock, F_SETFL, flags);
            else
                result = fcntl(sock, F_SETFL, flags | O_NONBLOCK);

            if (result != SK_NO_ERROR) {
                ErrorMessage("fcntl failed with error %ld", result);
                return SK_INVALID_SOCKET;
            }
#endif

            return sock;
        }

        // Binds an address to a socket. This is typicall used for
        // a listen socket, where the server's/sockets address is bound to it 
        SocketError Bind(SocketHandle sock, addrinfo* socketsAddr) {

            int result = bind(sock, socketsAddr->ai_addr, (int)socketsAddr->ai_addrlen);
            if (result == SK_ERROR_SOCKET) {
                ErrorMessage("Bind failed with errror [%d]", result);
                return SK_ERROR_SOCKET;
            }

            return result;
        }

        // Listens for inbound connections on a listen socket
        SocketError Listen(SocketHandle sock, int backlog) {

            int result = listen(sock, backlog);
            if (result == SK_ERROR_SOCKET) {
                ErrorMessage("Bind failed with errror [%d]", result);
                return SK_ERROR_SOCKET;
            }

            return result;
        }

        // Accepts any connections that exist on the listen socket
        SocketHandle Accept(SocketHandle listenSock, sockaddr* addr, socklen_t* addrlen) {

            SocketHandle clientSock = accept(listenSock, addr, addrlen);
            if (clientSock == SK_INVALID_SOCKET) {
                ErrorMessage("Accept failed.");
                return SK_INVALID_SOCKET;
            }

            return clientSock;
        }

        // Calls posix connect() not to be confused with ConnectTo()
        SocketError Connect(SocketHandle sock, const sockaddr* addr, socklen_t namelen) {

            int result = connect(sock, addr, namelen);

            // NON-Blocking sockets do not connect immediately.
            // SO (on windows) we must check the error to see if this is the issue
            if (result == SK_ERROR_SOCKET) {

#ifdef _WIN32
                if (WSAGetLastError() == WSAEWOULDBLOCK) {
#elif __linux__
                if (errno == EINPROGRESS || errno == EAGAIN) {
#endif

                    // We need to check if the socket is actually connected 
                    // we do this by using select and checking if the socket is read for writing
                    fd_set checkSet;
                    FD_ZERO(&checkSet);
                    FD_SET(sock, &checkSet);
                    result = select(sock + 1, NULL, &checkSet, NULL, 0);

                    if (result == SK_ERROR_SOCKET) {
                        ErrorMessage("Connect failed with error [%d]", result);
                        return SK_ERROR_SOCKET;
                    }

                    // Determine if there is any other error
                    int options;
                    socklen_t optionLength = sizeof(options);
                    result = getsockopt(sock, SOL_SOCKET, SO_ERROR, (char*)&options, (socklen_t*)&optionLength);

                    if (result == SK_ERROR_SOCKET) {
                        ErrorMessage("Unable to connect socket");
                        return SK_ERROR_SOCKET;
                    }

                }
                else {
                    ErrorMessage("Connect failed with error [%d]", result);
                    return SK_ERROR_SOCKET;
                }
            }

            return SK_NO_ERROR;
        }

        void CloseSocket(SocketHandle sock) {

#ifdef _WIN32
            closesocket(sock);
#elif __linux__
            close(sock);
#endif
        }

        // Shuts down certain aspects of an established connection.
        // For instance we can shut down the send part of a connection 
        // by setting how to the right flag
        SocketError Shutdown(SocketHandle sock, int how) {

            int rc = shutdown(sock, how);
            if (rc == SK_ERROR_SOCKET) {
                ErrorMessage("Shutdown failed");
                return SK_ERROR_SOCKET;
            }

            return rc;
        }

        bool IsError(SocketError errorCode) {
            return errorCode == SK_ERROR_SOCKET;
        }

        bool IsInvalidSocket(SocketHandle handle) {
            return handle == SK_INVALID_SOCKET;
        }

        SocketHandle CreateUDPSocket() {
            SocketHandle sock = SK_INVALID_SOCKET;
            sock = socket(AF_INET, SOCK_DGRAM, 0);
            if (sock == SK_INVALID_SOCKET) return SK_INVALID_SOCKET;
            return sock;
        }

        SocketHandle CreateUDPSocketSender() {
            return CreateUDPSocket();
        }

        SocketHandle CreateUDPSocketReceiver(short port) {

            SocketHandle sock = CreateUDPSocket();
            if (sock == SK_INVALID_SOCKET) return SK_INVALID_SOCKET;

            sockaddr_in server_addr;
            memset(&server_addr, 0, sizeof(server_addr));
            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = INADDR_ANY; // this means it binds to all interfaces
            server_addr.sin_port = htons(port);

            if (bind(sock, (const sockaddr*)&server_addr, sizeof(server_addr)) == SK_ERROR_SOCKET) {
                return SK_INVALID_SOCKET;
            }

            return sock;
        }

        // Create a listen socket that will recieve incoming connections
        SocketHandle CreateListenSocket(const char* hostname, const char* port, bool isBlocking) {

            addrinfo hintaddr;
            memset(&hintaddr, 0, sizeof(addrinfo));
            hintaddr.ai_family = AF_INET; // IPV4 protocol
            hintaddr.ai_socktype = SOCK_STREAM; // TCP 
            hintaddr.ai_protocol = IPPROTO_TCP;
            hintaddr.ai_flags = AI_PASSIVE;

            addrinfo* resultAddr = nullptr;

            int returnCode = GetAddrInfo(hostname, port, &hintaddr, &resultAddr);
            if (IsError(returnCode)) {
                return SK_INVALID_SOCKET;
            }

            SocketHandle listenSocket = Socket(resultAddr, isBlocking);
            if (listenSocket == SK_INVALID_SOCKET) {
                freeaddrinfo(resultAddr);
                return SK_INVALID_SOCKET;
            }

            if (Bind(listenSocket, resultAddr) == SK_ERROR_SOCKET) {
                freeaddrinfo(resultAddr);
                return SK_INVALID_SOCKET;
            }

            freeaddrinfo(resultAddr);

            if (Listen(listenSocket, SOMAXCONN) == SK_ERROR_SOCKET) {
                CloseSocket(listenSocket);
                return SK_INVALID_SOCKET;
            }

            return listenSocket;
        }

        // Create a client socket that will be used to communicate with the specified server.
        // Returns an address structure (and it's length) that holds info about the server machine.
        SocketHandle CreateClientSocketForServer(const char* hostname, const char* port, sockaddr & outServerAddr, socklen_t & outServerAddrLen, bool is_blocking = false) {

            addrinfo hintaddr;
            memset(&hintaddr, 0, sizeof(addrinfo));
            hintaddr.ai_family = AF_INET; // IPv4 protocol
            hintaddr.ai_socktype = SOCK_STREAM;
            hintaddr.ai_protocol = IPPROTO_TCP;

            addrinfo* resultAddr = nullptr;
            int result = GetAddrInfo(hostname, port, &hintaddr, &resultAddr);
            if (result == SK_ERROR_SOCKET) {
                return SK_INVALID_SOCKET;
            }

            // I don't know why really to use the getaddrinfo linked 
            // list here but i'm just going to use it any way
            SocketHandle clientSocket = SK_INVALID_SOCKET;
            addrinfo* curAddr = nullptr;
            for (curAddr = resultAddr; curAddr != nullptr; curAddr = resultAddr->ai_next) {

                // Create a socket 
                clientSocket = Socket(curAddr, is_blocking);
                if (IsInvalidSocket(clientSocket)) {
                    freeaddrinfo(resultAddr);
                    return SK_INVALID_SOCKET;
                }

                break;
            }

            // this is guaranteed to not be NULL because of a check we do earlier
            // however I dont' want a warning
            if (curAddr != nullptr) {
                outServerAddr = *curAddr->ai_addr;
                outServerAddrLen = curAddr->ai_addrlen;
            }

            freeaddrinfo(resultAddr);

            return clientSocket;
        }

        // Accepts the first inbound connection to this ip and port that this socket is listening for
        // Returns a socket that will be used to sent and recv data from the client.
        SocketHandle AcceptFirstConnectionOnListenSocket(SocketHandle listenSocket) {

            // Accept a client 
            SocketHandle clientSocket = Accept(listenSocket, nullptr, nullptr);
            if (clientSocket == SK_INVALID_SOCKET) {
                return SK_INVALID_SOCKET;
            }

            return clientSocket;
        }

        inline SocketError SendTo(SocketHandle handle, const char* buffer, int len, int flags, const sockaddr * addr, int addrlen) {

            #ifdef RSE_TEST_SOCKET_PACKET_LOSS
                int r = rand() % 100;
                if (r < 1) {
                    return 0;
                } // don't send but pretend you did (to simulate lost packets in testing)
                return sendto(handle, buffer, len, flags, addr, addrlen);
            #else
                return sendto(handle, buffer, len, flags, addr, addrlen);
            #endif
        }

        inline SocketError RecvFrom(SocketHandle handle, char* buffer, int len, int flags, sockaddr * addr, int* addrlen) {
            return recvfrom(handle, buffer, len, flags, addr, (socklen_t*)addrlen);
        }

        inline SocketError Recv(SocketHandle handle, char* buffer, int len, int flags) {

            #ifdef RSE_TEST_SOCKET_RECV_FAILED
                return SK_ERROR_SOCKET;
            #else
                return recv(handle, buffer, len, 0);
            #endif
        }

        inline SocketError Send(SocketHandle handle, const char* data, int size, int flags) {

            #ifdef RSE_TEST_SOCKET_SEND_FAILED
                return SK_ERROR_SOCKET;
            #else 
                return send(handle, data, size, flags);
            #endif
        }

    }
};
