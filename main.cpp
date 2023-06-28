#include <fcntl.h>
#include <iostream>
#include <vector>
#include <queue>
#include <netinet/in.h>
#include <ctime>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstring>
#include <atomic>
#include <thread>
#include <mutex>
#include <poll.h>
#include <map>
#include <algorithm>
#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>

using namespace std;
const char *SERVER_IP[] = {"192.168.0.101", "192.168.0.102", "192.168.0.103"};
const char *CLIENT_IP[] = {"10.0.0.101", "10.0.0.102", "10.0.0.103", "10.0.0.104", "10.0.0.105"};

enum TYPE{
    MUSIC=0,
    PICTURE=1,
    VIDEO=2,
};

struct Task{
    int index;
    int client_id;
    TYPE type;
    int time;
};

struct Server{
    int id;
    int fd;
    sockaddr_in addr;
    bool is_music;
    Task task;
    bool is_busy;

    Server(int id){
        this->id = id;
        fd = socket(AF_INET, SOCK_STREAM, 0);
        addr = sockaddr_in();
        addr.sin_family = AF_INET;
        addr.sin_port = htons(80);
        is_music = (id == 2);
        is_busy = false;

        inet_pton(AF_INET, SERVER_IP[id], &addr.sin_addr);
        if(connect(fd, (sockaddr *) &addr, sizeof(addr)) == -1){
            cerr << "problem with connect with server " << id << endl;
            exit(0);
        }
    }

};


struct Client{
    int fd;
    int id;
    Task *task;

    Client(int id){
        this->id = id;
    }
};

vector<Task> task_list;
vector<Server> server_list;
vector<Client> client_list;

// initialize connections
void init(){
    //setup connection with servers
    server_list.emplace_back(Server(0));
    server_list.emplace_back(Server(1));
    server_list.emplace_back(Server(2));
    //setup connection with clients
    client_list.emplace_back(Client(0));
    client_list.emplace_back(Client(1));
    client_list.emplace_back(Client(2));
    client_list.emplace_back(Client(3));
    client_list.emplace_back(Client(4));

}

bool are_matching(Task *task, Server *server){
    return (task->type == MUSIC && server->is_music) || (task->type != MUSIC && !server->is_music);
}

void send_task_to_server(Server &server, Task task){

}

void match_task_to_server(int server_id){
    Task *best_task = nullptr;
    Server &server = server_list[server_id];
    for (auto task:task_list){
        if (are_matching(&task, &server)){
            if (best_task == nullptr || task.time < best_task->time){
                best_task = &task;
            }
        }
    }

    if (best_task){
        send_task_to_server(server, *best_task);
        return;
    }

    if (server.is_music == MUSIC){


    }
    else {

    }

}

int main(){
    init();
    Server loadBalancerServer = Server(-1);
    int clientSocket;
    struct sockaddr_in clientAddress;
    socklen_t clientLength;

    // Create server socket

    // Set up server address
    // Bind the server socket to the specified address and port
    if (bind(loadBalancerServer.fd, (struct sockaddr*)&loadBalancerServer.addr, sizeof(loadBalancerServer.addr)) == -1) {
        std::cerr << "Failed to bind to port" << std::endl;
        return 1;
    }

    // Set server socket as non-blocking
    if (fcntl(loadBalancerServer.fd, F_SETFL, O_NONBLOCK) == -1) {
        std::cerr << "Failed to set server socket as non-blocking" << std::endl;
        return 1;
    }

    // Listen for incoming connections
    if (listen(loadBalancerServer.fd, SOMAXCONN) == -1) {
        std::cerr << "Failed to listen for connections" << std::endl;
        return 1;
    }


    while (true) {
        // Accept client connection
        clientLength = sizeof(clientAddress);
        clientSocket = accept(loadBalancerServer.fd, (struct sockaddr*)&clientAddress, &clientLength);
        if (clientSocket != -1) {
            std::cout << "Client connected" << std::endl;

            // Set client socket as non-blocking
            if (fcntl(clientSocket, F_SETFL, O_NONBLOCK) == -1) {
                std::cerr << "Failed to set client socket as non-blocking" << std::endl;
                return 1;
            }

            // Send a welcome message to the client
            const char* message = "Welcome to the server!";
            if (send(clientSocket, message, strlen(message), 0) == -1) {
                std::cerr << "Failed to send message to client" << std::endl;
            }

            // Close the client socket
            close(clientSocket);
        }

        // Add your client connection handling logic here

    }

    // Close the server socket
    close(loadBalancerServer.fd);

    return 0;
}



