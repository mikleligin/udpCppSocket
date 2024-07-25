#include <iostream>
#include <fstream>
#include <sstream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <mutex>
#include <chrono>
#include <iomanip>

#pragma comment(lib, "Ws2_32.lib")

#define configName "config.txt"
#define errorLog "errorLog.txt"

std::mutex mtx;
double bidPrice1 = 0.0, askPrice1 = 0.0;
double bidPrice2 = 0.0, askPrice2 = 0.0;
uint8_t InstrNum = 0;
uint8_t type = 1;
double diff;
std::string logFileName;

#pragma pack(push, 1)
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
#pragma pack(pop)
void logger(double bid1, double ask1, double bid2, double ask2)
{
    // Получаем текущее время
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm;
    localtime_s(&now_tm, &now_c);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    // Записываем в конфиг с параметром добавления к существующим записям
    std::ofstream log_file(logFileName, std::ios_base::app);
    log_file << std::put_time(&now_tm, "(%H:%M:%S)") << '.' << std::setw(3) << std::setfill('0') << ms.count() << ';'
        << bid1 << ';' << ask1 << ';' << bid2 << ';' << ask2 << std::endl;
}

void logError(std::string address, int port, packet* pack, int id, int sizePacket, int needSizePacket ) {
    std::lock_guard<std::mutex> lock(mtx);
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::tm now_tm;
    localtime_s(&now_tm, &now_c);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    std::ofstream log_file(errorLog, std::ios_base::app);
    log_file << std::put_time(&now_tm, "(%H:%M:%S") << '.' << std::setw(3) << std::setfill('0') << ms.count() << ';' << ") "
        << address << " | " << port << " | header: " << ((pack->header>>6)&3) << " | symbol: " << (int)pack->symbol << " | id: " << id 
        << " | sizePacket: " << sizePacket << " | needSize:" << needSizePacket 
        << " | BID: "  << (double)pack->MDEntryPXBid / 1000000 << " | ASK: "  << (double)pack->MDEntryPXAsk/ 1000000  << " | diff:" << diff << std::endl;

}

void receivePackets(int port, int id, const char* address){

    // Создаем UDP приемник и биндим сокет адресу. 
    WSADATA wsaData;
    SOCKET recvSocket = INVALID_SOCKET;
    sockaddr_in recvAddr;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    recvSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    recvAddr.sin_family = AF_INET;
    recvAddr.sin_port = htons(port);
    inet_pton(AF_INET, address, &recvAddr.sin_addr.s_addr);
    bind(recvSocket, (SOCKADDR*)&recvAddr, sizeof(recvAddr));
    std::cout << "Thread " << id <<  " started\n";
    // Создаем буффер для структуры
    char msg[sizeof(packet)];
    int msgSize = sizeof(msg);
    while (true){
        int recvMsg = recv(recvSocket, msg, msgSize, 0);
        // Если есть возможная ошибка сокета - обрабатываем
        if (recvMsg == SOCKET_ERROR) {
            int error_code = WSAGetLastError();
            std::cerr << "recv failed : " << error_code << std::endl;
            if (error_code == WSAECONNRESET || error_code == WSAENOTCONN) {
                break;
            }
        }
        if (recvMsg == sizeof(packet)){
            // Принимаем структуру
            packet* pack = reinterpret_cast<packet*>(msg);

            // Сдвигаем биты в нужные позиции
            uint8_t MsgType = (pack->header >> 6)&3;
            uint8_t symbol = pack->symbol;

            // Достаем отсюда нужные данные и сверяем условия
            if ((MsgType == type) && (symbol == InstrNum)) {

                double bidPrice = pack->MDEntryPXBid / 1000000;
                double askPrice = pack->MDEntryPXAsk / 1000000;
                // Ставим мьютекс для защиты потоков
                std::lock_guard<std::mutex> lock(mtx);
                if (id == 1) {
                    if (abs(bidPrice - bidPrice1) >= diff || abs(askPrice - askPrice1) >= diff) {
                        logger(bidPrice, askPrice, bidPrice2, askPrice2);
                    }
                    bidPrice1 = bidPrice;
                    askPrice1 = askPrice;
                }
                else {
                    if (abs(bidPrice - bidPrice2) >= diff || abs(askPrice - askPrice2) >= diff) {
                        logger(bidPrice1, askPrice1, bidPrice, askPrice);
                    }
                    bidPrice2 = bidPrice;
                    askPrice2 = askPrice;
                    

                }
                continue;
            }
            
        }
        packet* pack = reinterpret_cast<packet*>(msg);
        logError(address, port, pack, id, recvMsg, msgSize);
    }
    closesocket(recvSocket);
    WSACleanup();
}
void readConfig(const std::string& filename, std::string& host1, short& port1, std::string& host2, short& port2, uint8_t& instr_num, double& diff, std::string& logFileName) {
    std::ifstream configFileOpen(filename);
    // Проверяем файл на наличие информции
    if (configFileOpen.fail())
    {
        std::cerr << "Config file is not exist\n";
        exit(1);
        //return;
    }
    // Передаем в функцию ссылки, чтобы прочитать из файла значения и сразу же их присвоить переменным
    configFileOpen >> host1 >> port1 >> host2 >> port2 >> instr_num >> diff >> logFileName;
}
int main(){
    std::string host1, host2;
    short port1 = 0;
    short port2 = 0;
    // Можно использовать несколько вариантов. Например запускать код из консоли и юзать argv[], но я пропишу явно, 
    // чтобы было видно работу с файлом
    readConfig(configName, host1, port1, host2, port2, InstrNum, diff, logFileName);
    // Приводим всё к рабочему виду и создаем потоки под каждый порт
    InstrNum -= 48;
    std::thread th1(receivePackets, port1, 1, host1.c_str());
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::thread th2(receivePackets, port2, 2, host2.c_str());
    th1.join();
    th2.join();
}
