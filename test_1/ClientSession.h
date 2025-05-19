#pragma once
#include <winsock2.h>
#include "RingBuffer.h"
#include "Packet.h"

class ClientSession {
public:
    ClientSession(SOCKET socket);

    void PostRecv();
    bool ExtractPacket(Packet& packet);

    // ������ �޼��� �߰�
    WSAOVERLAPPED& getOverlapped();
    SOCKET getClientSocket() const;
    RingBuffer& getRecvRingBuffer();

private:
    WSAOVERLAPPED over_;
    SOCKET clientSock_;
    RingBuffer recvRingBuf_;
};


