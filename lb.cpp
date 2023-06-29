#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <queue>
#include <list>
#include <string>
#include <iostream>
#include <vector>

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

struct Task {
    int client_socket;
    TYPE type;
    int time;
    char data[2];

    Task() {
        client_socket = -1;
    }

    Task(int client_socket, char buffer[]) {
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

/*
typedef struct request {
    int video_server_price;
    int music_server_price;
    //client socket
    int client_socket;
    char original_request[2];

} request;


int servers_sockets[3] = {0};
*/

vector<Task> task_list;
vector<Server> server_list;


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


/*
std::list<request> requests;

std::queue<int> video_server_1_queue;

std::queue<int> video_server_2_queue;

std::queue<int> music_server_queue;

bool working[3] = {false, false, false};

char types[4] = "VVM";

std::queue<int> server_queues[3] = {video_server_1_queue, video_server_2_queue, music_server_queue};

int initial_workloads[3] = {0, 0, 0};
*/

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
            task_list.emplace_back(client_socket, buffer);

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

            server.task = Task(); // ? might not be needed
            server.is_busy = false;
        }
    }
}

/*
void balance_load_tomer() {
    requests.begin();

    if (!requests.empty()) {
        int len = requests.size();

        for (int i = 0; i < 3; i++) {
            if (requests.empty()) {
                break;
            }

            if (!working[i]) {

                int maximum_fitting_time = -1;
                int minimum_bad_time = 1000000;
                int chosen_indx = 100;
                bool found_good = false;
                request chosen_request;
                int chosen_reqiest_workload = 0;
                std::list<request>::iterator it = requests.begin();

                for (int req = 0; req < len; req++) {


                    request current_req = requests.front();
                    char req_type = current_req.original_request[0];
                    int req_time = current_req.original_request[1] - '0';

                    if (req_type == types[i]) {

                        if (req_time > maximum_fitting_time) {
                            maximum_fitting_time = req_time;
                            chosen_indx = req;
                            chosen_request = current_req;
                            found_good = true;
                            chosen_reqiest_workload = req_time;

                        }

                    }

                    else {

                        if (!found_good) {

                            //printf("no good\n");

                            if (types[i] == 'V') {

                                req_time = current_req.video_server_price;

                            }

                            else {


                                req_time = current_req.music_server_price;


                            }

                            int real_finish_time = req_time + initial_workloads[i];

                            if (real_finish_time < minimum_bad_time) {

                                minimum_bad_time = req_time;

                                chosen_indx = req;

                                //print the chosen index

                                //printf("chosen index: %d\n", chosen_indx);

                                chosen_request = current_req;

                                chosen_reqiest_workload = req_time;

                            }

                        }

                    }

                    std::advance(it, 1);

                }

                if (i == 2 && !found_good) {

                    int req_time_on_0 = initial_workloads[0] + chosen_request.video_server_price;

                    int req_time_on_1 = initial_workloads[1] + chosen_request.video_server_price;

                    if (chosen_reqiest_workload > req_time_on_0 || chosen_reqiest_workload > req_time_on_1) {

                        continue;

                    }

                }

                else if (!found_good) {

                    int req_time_on_2 = initial_workloads[2] + chosen_request.music_server_price;

                    if (chosen_reqiest_workload > req_time_on_2 && chosen_request.original_request[0] == 'M') {

                        continue;

                    }

                }
                // ! ------------------------------
                // ! important part
                //now send to the relevant server the request from the chosen index

                if (send(servers_sockets[i], chosen_request.original_request, sizeof(chosen_request.original_request),
                         MSG_DONTWAIT) < 0) {
                    cout << "Error sending request to server" << endl;
                    exit(-1);

                }
                // ! end of important part
                // ! ------------------------------

                server_queues[i].push(chosen_request.client_socket);

                it = requests.begin();
                if (chosen_indx > 0) {
                    std::advance(it, chosen_indx);
                }
                initial_workloads[i] = chosen_reqiest_workload;

                requests.erase(it);

                working[i] = true;

            }

        }

    }
}
*/



bool are_matching(bool is_music, TYPE type) {
    return is_music == (type == MUSIC);
}



void send_task_to_server(Server& server, Task task) {
    if (send(server.socket, task.data, sizeof(task.data), MSG_DONTWAIT) < 0) {
        cout << "Error sending request to server" << endl;
        exit(-1);
    }
    server.task = task;
    server.is_busy = true;
}

void handle_server(int server_id) {
    auto& server = server_list[server_id];
    if (server.is_busy || task_list.empty())
        return;

    // start with sjf
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

    if (server.is_music){
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
