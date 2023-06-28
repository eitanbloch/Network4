#include <iostream>
#include <vector>
#include <queue>
#include <netinet/in.h>
#include <ctime>
#include <arpa/inet.h>
#include <cstring>
#include <atomic>
#include <thread>
#include <mutex>
#include <poll.h>
#include <map>
#include <algorithm>

using namespace std;

int clientsSocket, server1Socket, server2Socket, server3Socket, client1Socket, client2Socket, client3Socket, client4Socket, client5Socket;
sockaddr_in clientsAddr, server1Addr, server2Addr, server3Addr;
struct pollfd serverSockfds[3];
const char *IP_SERVER1 = "192.168.0.101";
const char *IP_SERVER2 = "192.168.0.102";
const char *IP_SERVER3 = "192.168.0.103";

enum RequestContentType {
    MUSIC_REQUEST,
    PICTURE_REQUEST,
    VIDEO_REQUEST,
};

struct Request {
    int clientSockfd;
    RequestContentType type;
    int requestTime;
    time_t arrivalTime;
    int server_checking;
    char msg[3];
};

enum ServerType {
    MUSIC_SERVER,
    VIDEO_SERVER
};

struct Server {
    const uint8_t serverID;
    const ServerType type;
    const int serverSockfd;
    atomic<bool> isAvailable;
    atomic<int> currentClientSockfd;

    Server(uint8_t serverID, ServerType type, int serverSockfd) : serverID(serverID), type(type),
                                                                  serverSockfd(serverSockfd) {
        isAvailable.store(1);
        currentClientSockfd.store(-1);
    }

    Server(const Server &server) : serverID(server.serverID), type(server.type), serverSockfd(server.serverSockfd) {
        this->isAvailable.store(server.isAvailable.load());
        this->currentClientSockfd.store(server.currentClientSockfd.load());
    }
};

struct sockaddrComparator {
    bool operator()(const sockaddr &lhs, const sockaddr &rhs) const {
        return memcmp(&lhs, &rhs, sizeof(lhs)) < 0;
    }
};

mutex mtx;

vector<Request> pendingRequests;
vector<Server> servers;
map<sockaddr, int, sockaddrComparator> clientsQuantity;

void setupConnectionWithServers() {
    server1Socket = socket(AF_INET, SOCK_STREAM, 0);
    server2Socket = socket(AF_INET, SOCK_STREAM, 0);
    server3Socket = socket(AF_INET, SOCK_STREAM, 0);

    server1Addr.sin_family = AF_INET;
    server1Addr.sin_port = htons(80);
    inet_pton(AF_INET, IP_SERVER1, &server1Addr.sin_addr);

    server2Addr.sin_family = AF_INET;
    server2Addr.sin_port = htons(80);
    inet_pton(AF_INET, IP_SERVER2, &server2Addr.sin_addr);

    server3Addr.sin_family = AF_INET;
    server3Addr.sin_port = htons(80);
    inet_pton(AF_INET, IP_SERVER3, &server3Addr.sin_addr);

    connect(server1Socket, (sockaddr *) &server1Addr, sizeof(server1Addr));
    connect(server2Socket, (sockaddr *) &server2Addr, sizeof(server2Addr));
    connect(server3Socket, (sockaddr *) &server3Addr, sizeof(server3Addr));

    serverSockfds[0].fd = server1Socket;
    serverSockfds[0].events = POLLIN;
    serverSockfds[1].fd = server2Socket;
    serverSockfds[1].events = POLLIN;
    serverSockfds[2].fd = server3Socket;
    serverSockfds[2].events = POLLIN;

    servers.push_back(Server(0, MUSIC_SERVER, server1Socket));
    servers.push_back(Server(1, VIDEO_SERVER, server2Socket));
    servers.push_back(Server(2, VIDEO_SERVER, server3Socket));
}

RequestContentType getContentType(const char *msg) {
    if (msg[0] == 'M') {
        return MUSIC_REQUEST;
    } else if (msg[0] == 'V') {
        return VIDEO_REQUEST;
    }
    return PICTURE_REQUEST;
}

int getRequestTime(const char *msg) {
    return atoi(msg + 1);
}

int getProcessingTime(RequestContentType request_type, ServerType server_type, int requestTime) {
    switch (request_type) {
        case MUSIC_REQUEST:
            if (server_type == MUSIC_SERVER) {
                return requestTime;
            } else {
                return requestTime * 2;
            }
        case VIDEO_REQUEST:
            if (server_type == VIDEO_SERVER) {
                return requestTime;
            } else {
                return requestTime * 3;
            }
        case PICTURE_REQUEST:
            if (server_type == VIDEO_SERVER) {
                return requestTime;
            } else {
                return requestTime * 2;
            }
    }
}

void getRequestsFromClients() {
    clientsAddr.sin_family = AF_INET;
    clientsAddr.sin_port = htons(80);
    clientsAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(clientsSocket, (sockaddr *) &clientsAddr, sizeof(clientsAddr));
    listen(clientsSocket, 5);

    while (true) {
        socklen_t clientAddrLen = sizeof(sockaddr_in);
        int currentClientSocket = accept(clientsSocket, (sockaddr *) &clientsAddr, &clientAddrLen);
        sockaddr clientAddr;
        clientAddrLen = sizeof(clientAddr);
        getpeername(currentClientSocket, &clientAddr, &clientAddrLen);
        if (clientsQuantity.find(clientAddr) == clientsQuantity.end()) {
            clientsQuantity[clientAddr] = 0;
        }
        char msg[3] = {0};
        recv(currentClientSocket, msg, 2, 0);
        cout << "[*]    Received request from client with sockfd " << currentClientSocket << '\n'
             << "[*]    Request content: " << msg << '\n';
        Request request{};
        request.clientSockfd = currentClientSocket;
        request.type = getContentType(msg);
        request.requestTime = getRequestTime(msg);
        request.arrivalTime = time(nullptr);
        request.server_checking = -1;
        strncpy(request.msg, msg, 3);
        mtx.lock();
        pendingRequests.push_back(request);
        cout << "[*]    Queue size: " << pendingRequests.size() << '\n';
        mtx.unlock();
    }
}

void getFinishedRequestFromServer() {
    while (true) {
        poll(serverSockfds, 3, -1);
        for (int i = 0; i < 3; i++) {
            if (serverSockfds[i].revents & POLLIN) {
                char msg[3];
                recv(serverSockfds[i].fd, msg, 2, 0);
                int clientSockfd = servers[i].currentClientSockfd.load();
                cout << "[*]    Forwarding back from Server" << i + 1 << " to client with sockfd "
                     << clientSockfd << '\n';
                send(clientSockfd, msg, 2, 0);
                servers[i].currentClientSockfd.store(-1);
                servers[i].isAvailable.store(1);
            }
        }
    }
}

void print_queue_contents(vector<Request> &queue, int serverID) {
    cout << "[*]    Queue contents: " << serverID + 1 << ": ";
    for (auto &request: queue) {
        cout << request.msg << " ";
    }
    cout << '\n';
}

void forwardRequestToServer(Request &request, int serverID) {
    int clientSockfd = request.clientSockfd;
    cout << "[*]    Forwarding request from client with sockfd " << clientSockfd << " to server" << serverID + 1
         << '\n';
    cout << "[*]    Queue size: " << pendingRequests.size() << '\n';

    cout<<"middle"<<endl;
    print_queue_contents(pendingRequests, serverID);

    servers[serverID].currentClientSockfd.store(clientSockfd);
    sockaddr clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);
    getpeername(clientSockfd, &clientAddr, &clientAddrLen);
    clientsQuantity[clientAddr]++;
    send(servers[serverID].serverSockfd, request.msg, 2, 0);

    pendingRequests.erase(pendingRequests.begin());

    //servers[serverID].nextAvailableTime.store(computeNextAvailableTime(request.type, servers[serverID].type));
    // request.processingTime = ...
}

int compareRequests(const Request &a, const Request &b) {
    int server_id = a.server_checking;
    if (servers[server_id].type == MUSIC_SERVER) {
        if (a.type < b.type)
            return -1;
        else if (a.type > b.type)
            return 1;
    } else if (servers[server_id].type == VIDEO_SERVER) {
        if (a.type > b.type)
            return -1;
        else if (a.type < b.type)
            return 1;
    }

    // a.type == b.type
    //1-quantity, 0-length
    double alpha = 0.66;
    int random = rand() % 100;
    if (random < alpha * 100) {
        random = 1;
    } else {
        random = 0;
    }

    if (random == 1) {
        sockaddr clientAddrA, clientAddrB;
        socklen_t clientAddrLen = sizeof(clientAddrA);
        getpeername(a.clientSockfd, &clientAddrA, &clientAddrLen);
        getpeername(b.clientSockfd, &clientAddrB, &clientAddrLen);

        if (clientsQuantity[clientAddrA] != clientsQuantity[clientAddrB]) {
            if (clientsQuantity[clientAddrA] < clientsQuantity[clientAddrB])
                return -1;
            else if (clientsQuantity[clientAddrA] > clientsQuantity[clientAddrB])
                return 1;
        } else {
            if (a.requestTime < b.requestTime)
                return -1;
            else if (a.requestTime > b.requestTime)
                return 1;
        }
    } else {
        if (a.requestTime < b.requestTime)
            return -1;
        else if (a.requestTime > b.requestTime)
            return 1;
        else {
            sockaddr clientAddrA, clientAddrB;
            socklen_t clientAddrLen = sizeof(clientAddrA);
            getpeername(a.clientSockfd, &clientAddrA, &clientAddrLen);
            getpeername(b.clientSockfd, &clientAddrB, &clientAddrLen);

            if (clientsQuantity[clientAddrA] != clientsQuantity[clientAddrB]) {
                if (clientsQuantity[clientAddrA] < clientsQuantity[clientAddrB])
                    return -1;
                else if (clientsQuantity[clientAddrA] > clientsQuantity[clientAddrB])
                    return 1;
            }
        }
    }
    return 0;
}

Request &findBestRequestForServer(Server &server) {
    for (auto &request: pendingRequests) {
        request.server_checking = server.serverID;
    }

    sort(pendingRequests.begin(), pendingRequests.end(), compareRequests);
    //reverse(pendingRequests.begin(), pendingRequests.end());
    return pendingRequests[0];
}


void LB_Algorithm() {
    while (true) {

        // this_thread::sleep_for(chrono::milliseconds(100));
        mtx.lock();
        if (pendingRequests.empty()) {
            mtx.unlock();
            continue;
        }

        for (auto &server: servers) {
            if (server.isAvailable.load() == 0) {
                continue;
            }
            server.isAvailable.store(0);
            Request &request = findBestRequestForServer(server);
            cout<<"before"<<endl;
            print_queue_contents(pendingRequests, server.serverID);
            forwardRequestToServer(request, server.serverID);
            cout<<"after"<<endl;
            print_queue_contents(pendingRequests, server.serverID);
        }

        mtx.unlock();
    }
}


int main() {
    time_t start = time(nullptr);
    clientsSocket = socket(AF_INET, SOCK_STREAM, 0);

    setupConnectionWithServers();
    cout << "[*]    Connection with servers established" << '\n'
         << "[*]    Starting threads" << '\n'
         << "[*]    Time elapsed: " << time(nullptr) - start << '\n';
    thread client_poller_thread(getRequestsFromClients);
    thread hamotzi_thread(getFinishedRequestFromServer);
    thread lb_thread(LB_Algorithm);
    client_poller_thread.join();
    hamotzi_thread.join();
    lb_thread.join();
    cout << "[*]    Threads finished" << '\n';
    cout << "[*]    Time elapsed: " << time(nullptr) - start << '\n';
    return 0;
}
