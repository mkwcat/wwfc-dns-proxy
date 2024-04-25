// WiiLink WFC DNS Proxy
// Copyright (c) 2024 mkwcat
//
// SPDX-License-Identifier: MIT

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>

#ifdef WIN32
#  include <windows.h>
typedef int socklen_t;
#else
#  include <arpa/inet.h>
#  include <cerrno>
#  include <netdb.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
#endif

int printError(const char* msg)
{
#ifdef WIN32
    wchar_t* s = NULL;
    FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL, WSAGetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR) &s, 0, NULL
    );
    std::fprintf(stderr, "%s: %S", msg, s);
    LocalFree(s);
#else
    int err = errno;
    std::fprintf(stderr, "%s: %s\n", msg, strerror(err));
#endif

    return 1;
}

void printLocalIP(sockaddr_in* server)
{
    // Test server connection
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        printError("Failed to create test socket");
        return;
    }

    if (connect(sock, (sockaddr*) server, sizeof(*server)) < 0) {
        printError("Failed to connect to server");
        return;
    }

    sockaddr_in local;
    socklen_t localLen = sizeof(local);
    if (getsockname(sock, (sockaddr*) &local, &localLen) < 0) {
        printError("Failed to get local address");
        return;
    }

    char* localAddress = inet_ntoa(local.sin_addr);
    if (localAddress == NULL) {
        printError("Failed to get local IP address");
        return;
    }

    std::printf(
        "Go into your DS or Wii's connection settings and enter the "
        "following:\n"
        "Auto-obtain DNS: No\n"
        "Primary DNS: %s\n"
        "Secondary DNS: 0.0.0.0 (or 1.1.1.1)\n"
        "\n"
        "Server is now running. Don't close this window.\n",
        localAddress
    );

#ifdef WIN32
    closesocket(sock);
#else
    close(sock);
#endif
}

int main(int argc, char* argv[])
{
    std::printf("WiiLink WFC DNS Proxy 1.0\n"
                "Copyright (c) 2024 mkwcat\n"
                "Source code: https://github.com/mkwcat/wwfc-dns-proxy\n"
                "\n");

    bool verbose = false;
    char defaultServerAddr[] = "violet.wiilink24.com:1053";
    char* serverAddress = defaultServerAddr;
    const char* bindAddress = "0.0.0.0";

    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "-h") == 0 ||
            std::strcmp(argv[i], "--help") == 0) {
            std::printf(
                "Usage: %s [-v] [-a serverAddress] [-b bindAddress]\n"
                "  -v --verbose: Verbose logging\n"
                "  -a --address: Server address (default: "
                "violet.wiilink24.com:1053)\n"
                "  -b --bind: Bind address (default: 0.0.0.0)\n",
                argv[0]
            );
            return 0;
        }

        if (std::strcmp(argv[i], "-v") == 0 ||
            std::strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (std::strcmp(argv[i], "-a") == 0 || std::strcmp(argv[i], "--address") == 0) {
            if (i + 1 >= argc) {
                std::printf(
                    "Missing argument for %s\n"
                    "For help, run %s --help\n",
                    argv[i], argv[0]
                );
                return 1;
            }

            serverAddress = argv[++i];
        } else if (std::strcmp(argv[i], "-b") == 0 || std::strcmp(argv[i], "--bind") == 0) {
            if (i + 1 >= argc) {
                std::printf(
                    "Missing argument for %s\n"
                    "For help, run %s --help\n",
                    argv[i], argv[0]
                );
                return 1;
            }

            bindAddress = argv[++i];
        } else {
            std::printf(
                "Unknown argument: %s\n"
                "For help, run %s --help\n",
                argv[i], argv[0]
            );
            return 1;
        }
    }

#ifdef WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return printError("Failed to initialize Winsock");
    }
#endif

    // Create socket
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        return printError("Failed to create socket");
    }

    // Bind to port 53
    sockaddr_in bindAddr;
    bindAddr.sin_family = AF_INET;
    bindAddr.sin_port = htons(53);
    bindAddr.sin_addr.s_addr = inet_addr(bindAddress);
    if (bind(sock, (sockaddr*) &bindAddr, sizeof(bindAddr)) < 0) {
        char errStr[128];
        std::sprintf(errStr, "Failed to bind to %s", bindAddress);
        return printError(errStr);
    }

    sockaddr_in client;
    socklen_t clientLen = sizeof(client);

    sockaddr_in server;
    server.sin_family = AF_INET;

    char* serverAddressPort = std::strchr(serverAddress, ':');
    if (serverAddressPort == nullptr) {
        server.sin_port = htons(1053);
    } else {
        *serverAddressPort = '\0';
        server.sin_port = htons(std::atoi(serverAddressPort + 1));
    }

    hostent* host = gethostbyname(serverAddress);
    if (host == NULL) {
        return printError("Failed to resolve server address");
    }

    server.sin_addr.s_addr = *(int*) host->h_addr_list[0];
    if (server.sin_addr.s_addr == INADDR_NONE) {
        return printError("Invalid server address");
    }

    printLocalIP(&server);

    struct SocketInfo {
        int sock;
        struct sockaddr_in addr;
        time_t time;
    };

    std::vector<SocketInfo> sockets;

    while (true) {
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(sock, &fds);

        // Clear unused sockets
        int maxSock = sock;
        for (unsigned int i = 0; i < sockets.size(); i++) {
            SocketInfo info = sockets[i];
            if (std::time(NULL) - info.time > 5) {
                if (verbose) {
                    std::printf(
                        "Closing connection to %s:%d\n",
                        inet_ntoa(info.addr.sin_addr), ntohs(info.addr.sin_port)
                    );
                }
#if WIN32
                closesocket(info.sock);
#else
                close(info.sock);
#endif
                sockets.erase(sockets.begin() + i);
                i--;
            } else {
                FD_SET(info.sock, &fds);
                if (info.sock > maxSock) {
                    maxSock = info.sock;
                }
            }
        }

        if (select(maxSock + 1, &fds, NULL, NULL, NULL) < 0) {
            return printError("Failed to select socket");
        }

        // Handle data
        char buf[0x8000];
        unsigned int len = 0;
        if (FD_ISSET(sock, &fds)) {
            len = recvfrom(
                sock, buf, sizeof(buf), 0, (sockaddr*) &client, &clientLen
            );

            if (verbose) {
                std::printf(
                    "Received %d bytes from %s:%d\n", len,
                    inet_ntoa(client.sin_addr), ntohs(client.sin_port)
                );
            }

            int reqSock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (reqSock < 0) {
                return printError("Failed to create socket");
            }

            if (connect(reqSock, (sockaddr*) &server, sizeof(server)) < 0) {
                return printError("Failed to connect to server");
            }

            if (send(reqSock, buf, len, 0) < 0) {
                return printError("Failed to send data");
            }

            if (verbose) {
                std::printf(
                    "Sent %d bytes to %s:%d\n", len, inet_ntoa(server.sin_addr),
                    ntohs(server.sin_port)
                );
            }

            struct SocketInfo info;
            info.sock = reqSock;
            info.addr = client;
            info.time = time(NULL);
            sockets.push_back(info);
        } else {
            SocketInfo info;
            unsigned int i;
            for (i = 0; i < sockets.size(); i++) {
                info = sockets[i];
                if (FD_ISSET(info.sock, &fds)) {
                    len = recv(info.sock, buf, sizeof(buf), 0);
                    break;
                }
            }

            if (verbose) {
                std::printf(
                    "Reply %d bytes to %s:%d\n", len,
                    inet_ntoa(info.addr.sin_addr), ntohs(info.addr.sin_port)
                );
            }

#ifdef WIN32
            closesocket(info.sock);
#else
            close(info.sock);
#endif
            sockets.erase(sockets.begin() + i);

            if (sendto(
                    sock, buf, len, 0, (struct sockaddr*) &info.addr,
                    sizeof(info.addr)
                ) < 0) {
                return printError("Failed to send data");
            }
        }
    }
}