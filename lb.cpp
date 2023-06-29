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


struct Server {
    char *ip;
    int port;
    int load;
};

struct Client {
    char *ip;
    int port;
};

typedef struct request {
    int video_server_price;
    int music_server_price;
    //client socket
    int client_socket;
    char original_request[2];

} request;


struct Task {
    int client_id;
    TYPE type;
    int time;
    string data;

    Task() {
        client_id = -1;
    }

    Task(char buffer[], int client_id) {
        this->client_id = client_id;
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
        data = string(buffer, 3);
    }
};

int servers_sockets[3] = {0};

vector<Task> task_list;
vector<Server> server_list;
vector<Client> client_list;

typedef struct sockaddr_in Addr;
const char *SERVER_IP[] = {"192.168.0.101", "192.168.0.102", "192.168.0.103"};
int sock;
Addr addr;


void start_server(int server_id, int socket) {
    Addr s_addr;
    s_addr.sin_family = AF_INET;
    s_addr.sin_port = htons(80);
    s_addr.sin_addr.s_addr = inet_addr(SERVER_IP[server_id]);
    if (connect(socket, (struct sockaddr *) &s_addr, sizeof(s_addr)) < 0) {
        cout << "There is a problem with the connection to server " << server_id << endl;
        exit(-1);

    }
    servers_sockets[server_id] = socket;

}

int init() {
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

struct sockaddr_in client;

int client_size = sizeof(client);

char buffer[2];

std::list<request> requests;

std::queue<int> video_server_1_queue;

std::queue<int> video_server_2_queue;

std::queue<int> music_server_queue;

bool working[3] = {false, false, false};

char types[4] = "VVM";

std::queue<int> server_queues[3] = {video_server_1_queue, video_server_2_queue, music_server_queue};

int initial_workloads[3] = {0, 0, 0};

void get_tasks_from_clients() {
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(sock, &readfds);
    struct timeval timeout{0, 0};

    select(sock + 1, &readfds, NULL, NULL, &timeout);

    bool new_connection = FD_ISSET(sock, &readfds);

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

        struct request r = {1, 1, client_socket, {buffer[0], buffer[1]}};

        if (buffer[0] == 'V') {
            r.video_server_price = buffer[1] - '0';
            r.music_server_price = 3 * (buffer[1] - '0');

        }
        else if (buffer[0] == 'P') {
            r.video_server_price = buffer[1] - '0';
            r.music_server_price = 2 * (buffer[1] - '0');
        }
        else if (buffer[0] == 'M') {
            r.video_server_price = 2 * (buffer[1] - '0');
            r.music_server_price = buffer[1] - '0';
        }

        requests.push_back(r);

    }

}

void poll_servers() {
    for (int i = 0; i < 3; i++) {
        int server = servers_sockets[i];

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(server, &readfds);
        struct timeval timeout{0, 0};

        select(server + 1, &readfds, NULL, NULL, &timeout);

        if (FD_ISSET(server, &readfds)) {

            //read response from server
            if (recv(server, buffer, 2, MSG_DONTWAIT) < 0) { //this is blocking
                cout << "There was a problem with receiving data from server" << endl;
                exit(-1);
            }

            //send response back to client
            int client_socket = server_queues[i].front();
            server_queues[i].pop();

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

            working[i] = false;
            initial_workloads[i] = 0;
        }
    }
}


int main() {
    init();

    while (true) {
        get_tasks_from_clients();
        poll_servers();





        
        requests.begin();

        if (!requests.empty()) {

            //printf("handling requests\n");

            int len = requests.size();

            for (int i = 0; i < 3; i++) {

                if (requests.empty()) {

                    break;

                }

                if (!working[i]) {

                    //printf("working on server %c\n", types[i]);

                    int maximum_fitting_time = -1;

                    int minimum_bad_time = 1000000;

                    int chosen_indx = 100;

                    bool found_good = false;

                    request chosen_request;

                    int chosen_reqiest_workload = 0;

                    //find the longest fitting if any

                    //reset the iterator of requests list to the beginning

                    std::list<request>::iterator it = requests.begin();

                    //bool dont_take = false;

                    for (int req = 0; req < len; req++) {


                        //request current_req = requests[req];

                        //list has no [] operand, soadvance first amd take front


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

                        //print two alternatives weights

                        //printf("req_time_on_0: %d\n", req_time_on_0);

                        //printf("req_time_on_1: %d\n", req_time_on_1);

                        //print initial workloads

                        //printf("initial_workloads[0]: %d\n", initial_workloads[0]);

                        //printf("initial_workloads[1]: %d\n", initial_workloads[1]);

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

                    //now send to the relevant server the request from the chosen index

                    if (send(servers_sockets[i], chosen_request.original_request,
                             sizeof(chosen_request.original_request), /*0*/MSG_DONTWAIT) < 0) {

                        //printf("Error sending request to server %d\n", i);

                        //print what we attempted to send

                        //printf("request: %s\n", chosen_request.original_request);

                        return -1;

                    }

                    //now remove the request from the listrminal

                    //requests.erase(requests.begin() + chosen_indx);

                    //fix the above line, it give error: lb.cpp:527:36: error: no match for ‘operator[]’ (operand types are ‘std::list<request>’ and ‘int’)

                    //define iterator fitting for the chosen index

                    //now push the client socket to the relevant queue

                    ////printf("survived the send\n");

                    server_queues[i].push(chosen_request.client_socket);

                    it = requests.begin();

                    //print the chosen index

                    ////printf("chosen index: %d\n", chosen_indx);

                    if (chosen_indx > 0) {

                        std::advance(it, chosen_indx);

                    }

                    //print the original request and the server we sent to

                    //printf("sent request %s to server %d\n", chosen_request.original_request, i);

                    //if server is 2 also print the req time and initial workload

                    //if (i == 2)

                    //{

                    //	printf("req time: %d, initial workload: %d\n", chosen_reqiest_workload, initial_workloads[i]);

                    //}

                    //put the weight of the request in the relevant server

                    initial_workloads[i] = chosen_reqiest_workload;

                    requests.erase(it);

                    ////printf("erased");

                    //now set the server to working


                    working[i] = true;

                }

            }

        }


    }


}
