#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <ctime>

#pragma comment(lib, "Ws2_32.lib")

struct packet {
    uint8_t header;
    uint8_t symbol;

    uint32_t MDEntryPXBid;
    uint32_t MDEntrySizeBid;
    const uint8_t MDEntryIDBid = 0;

    uint32_t MDEntryPXAsk;
    uint32_t MDEntrySizeAsk;
    const uint8_t MDEntryIDAsk = 0;
};

void send_udp_packets(const char* address, int port, uint8_t symbol) {
    WSADATA wsaData;
    SOCKET sendSocket = INVALID_SOCKET;
    sockaddr_in sendAddr;

    WSAStartup(MAKEWORD(2, 2), &wsaData);

    sendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sendAddr.sin_family = AF_INET;
    sendAddr.sin_port = htons(port);
    inet_pton(AF_INET, address, &sendAddr.sin_addr.s_addr);

    packet pkt;
    pkt.header = 0b01000000;
    pkt.symbol = symbol;
    pkt.MDEntrySizeBid = 1000;
    pkt.MDEntrySizeAsk = 1000;

    std::srand(static_cast<unsigned int>(std::time(nullptr)));

    while (true) {
        // Генерация случайных цен
        double bid_price = (std::rand() % 10000) / 100.0;
        double ask_price = bid_price + 1.0 + (std::rand() % 100) / 100.0;

        pkt.MDEntryPXBid = static_cast<uint32_t>(bid_price * 1000000);
        pkt.MDEntryPXAsk = static_cast<uint32_t>(ask_price * 1000000);

        sendto(sendSocket, reinterpret_cast<char*>(&pkt), sizeof(pkt), 0, (SOCKADDR*)&sendAddr, sizeof(sendAddr));

        std::cout << "Sent packet with bid: " << bid_price << " and ask: " << ask_price << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    closesocket(sendSocket);
    WSACleanup();
}

int main() {

    const char* server_address = "127.0.0.1";
    int port = 6035;
    uint8_t symbol = 1;

    send_udp_packets(server_address, port, symbol);

    return 0;
}
