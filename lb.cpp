#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <queue>
#include <string>
#include <iostream>
#include <vector>
#include <cmath>
using std::cin;
using std::cout;
using std::cerr;
using std::endl;
using std::queue;
using std::string;
using std::vector;


typedef struct sockaddr_in Addr;
const char *SERVER_IP[] = {"192.168.0.101", "192.168.0.102", "192.168.0.103"};
int sock;
Addr addr;

Addr client;
int client_size = sizeof(client);
char buffer[2];

enum TYPE {
    MUSIC = 0,
    PICTURE = 1,
    VIDEO = 2,
};
int client_completed[] = {0, 0, 0, 0, 0};
int sum_completed = 0;

struct Task {
    int client_id;
    int client_socket;
    TYPE type;
    int time;
    char data[2];

    Task() {
        client_id = -1;
        client_socket = -1;
    }

    Task(int client_id, int client_socket, char buffer[]) {
        this->client_id = client_id;
        this->client_socket = client_socket;
        if (buffer[0] == 'M') {
            type = MUSIC;
        }
        else if (buffer[0] == 'P') {
            type = PICTURE;
        }
        else {
            type = VIDEO;
        }
        time = atoi(buffer + 1);
        data[0] = buffer[0];
        data[1] = buffer[1];
    }
};

struct Server {
    int id;
    int socket;
    Addr addr;
    bool is_music;
    Task task;
    bool is_busy;

    Server(int id, int socket, Addr addr) {
        this->id = id;
        this->socket = socket;
        this->addr = addr;
        is_music = id == 0;
        is_busy = false;
    }
};


vector<Task> task_list;
vector<Server> server_list;


int get_client_id(Addr client_address){
    string client_ip = inet_ntoa(client_address.sin_addr);
    return client_ip[client_ip.length() - 1] - '1';
}

void start_server(int server_id, int socket) {
    Addr s_addr;
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(80);
    s_addr.sin_addr.s_addr = inet_addr(SERVER_IP[server_id]);
    if (connect(socket, (struct sockaddr *) &s_addr, sizeof(s_addr)) < 0) {
        cout << "There is a problem with the connection to server " << server_id << endl;
        exit(-1);

    }

    //servers_sockets[server_id] = socket;
    server_list.emplace_back(server_id, socket, s_addr);

}

void init() {
    // init servers
    int s1_socket = socket(AF_INET, SOCK_STREAM, 0);
    int s2_socket = socket(AF_INET, SOCK_STREAM, 0);
    int s3_socket = socket(AF_INET, SOCK_STREAM, 0);

    start_server(0, s1_socket);
    start_server(1, s2_socket);
    start_server(2, s3_socket);

    // init main socket
    sock = socket(AF_INET, SOCK_STREAM, 0);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        cout << "Error binding client socket" << endl;
        exit(-1);
    }
    if (listen(sock, 5) < 0) {
        cout << "Error listening to client socket" << endl;
        exit(-1);
    }

}


void get_tasks_from_clients() {
    bool new_connection = true;
    while (new_connection) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        struct timeval timeout{0, 0};

        select(sock + 1, &readfds, NULL, NULL, &timeout);

        new_connection = FD_ISSET(sock, &readfds);

        if (new_connection) {
            // accept
            int client_socket = accept(sock, (struct sockaddr *) &client, (socklen_t *) &client_size);
            if (client_socket < 0) {
                cout << "Error accepting client connection" << endl;
                exit(-1);
            }
            // read data
            if (recv(client_socket, buffer, 2, 0) < 0) {
                cout << "Error receiving client request" << endl;
                exit(-1);
            }

            cout << "Received request from client " << buffer[0] << buffer[1] << endl;

            //handle request
            task_list.emplace_back(get_client_id(client), client_socket, buffer);

        }
    }

}

void poll_servers() {
    for (auto& server: server_list) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server.socket, &readfds);
        struct timeval timeout{0, 0};

        select(server.socket + 1, &readfds, NULL, NULL, &timeout);

        if (FD_ISSET(server.socket, &readfds)) {

            //read response from server
            if (recv(server.socket, buffer, 2, MSG_DONTWAIT) < 0) {
                cout << "There was a problem with receiving data from server" << endl;
                exit(-1);
            }

            //send response back to client
            int client_socket = server.task.client_socket;

            //send the message to the client
            if (send(client_socket, buffer, 2, MSG_DONTWAIT) < 0) {
                cout << "Error sending response to client" << endl;
                exit(-1);
            }
            // close connection with client
            if (close(client_socket) < 0) {
                cout << "Error closing client socket" << endl;
                exit(-1);
            }
            // update logs
            client_completed[server.task.client_id]++;
            sum_completed++;

            // update server
            server.task = Task(); // ? might not be needed
            server.is_busy = false;
        }
    }
}


bool are_matching(bool is_music, TYPE type) {
    return is_music == (type == MUSIC);
}


void send_task_to_server(Server& server, Task task) {
    if (send(server.socket, task.data, sizeof(task.data), MSG_DONTWAIT) < 0) {
        cout << "Error sending request to server" << endl;
        exit(-1);
    }
    // update server
    server.task = task;
    server.is_busy = true;
    // update task list
    for (int i = 0; i < task_list.size(); i++) {
        if (task_list[i].client_socket == task.client_socket) {
            task_list.erase(task_list.begin() + i);
            break;
        }
    }
}

int get_score(Server& server, Task *task) {
    bool are_same = are_matching(server.is_music, task->type);
    int score = task->time;
    if (!are_same) {
        score *= (task->type == VIDEO ? 2 : 1); // if video server, then type is music
        score += 100;
    }

    double dist = client_completed[task->client_id] - (sum_completed / 5.0);
    score += (int) pow(3.0, dist);
    return score;
}

void handle_server(int server_id) {
    auto& server = server_list[server_id];
    if (server.is_busy || task_list.empty())
        return;

    Task *best_task = nullptr;
    int min_score = 1000000;
    for (auto &task: task_list){
        int score = get_score(server, &task);
        if (best_task == nullptr || score < min_score) {
            best_task = &task;
            min_score = score;
        }
    }
    send_task_to_server(server, *best_task);
    

/*    // start with sjf
    Task *best_task = nullptr;
    for (auto& task: task_list) {
        if (are_matching(server.is_music, task.type)) {
            if (best_task == nullptr || task.time < best_task->time) {
                best_task = &task;
            }
        }
    }

    if (best_task) {
        send_task_to_server(server, *best_task);
        return;
    }

    if (server.is_music) {
        // if no task found choose task with minimal waste
        int min_waste = 1000000;
        for (auto& task: task_list) {
            int waste = task.time * (task.type == PICTURE ? 1 : 2);
            if (waste < min_waste) {
                min_waste = waste;
                best_task = &task;
            }
        }
    }
    else {
        for (auto& task: task_list) {
            if (best_task == nullptr || task.time < best_task->time) {
                best_task = &task;
            }
        }
    }

    send_task_to_server(server, *best_task);
    */
}

void balance_load() {
    if (task_list.empty())
        return;

    if (task_list[0].type == MUSIC) {
        handle_server(0);
        handle_server(1);
        handle_server(2);
    }
    else {
        handle_server(2);
        handle_server(1);
        handle_server(0);
    }
}

int main() {
    init();

    while (true) {
        get_tasks_from_clients();
        poll_servers();
        balance_load();


    }


}
