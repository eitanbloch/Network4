#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <queue>
#include <vector>
#include <iostream>
#include <map>

#define MAX_INT 2147483647
using std::cin;
using std::cout;
using std::cerr;
using std::endl;
using std::queue;
using std::string;
using std::vector;
using std::map;

const char *SERVER_IP[] = {"192.168.0.101", "192.168.0.102", "192.168.0.103"};
const char *CLIENT_IP[] = {"10.0.0.101", "10.0.0.102", "10.0.0.103", "10.0.0.104", "10.0.0.105"};
int sock;
struct sockaddr_in addr;
enum TYPE {
    MUSIC = 0,
    PICTURE = 1,
    VIDEO = 2,
};

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
    }
};

struct Server {
    int id;
    int fd;
    sockaddr_in addr;
    bool is_music;
    Task task;
    bool is_busy;

    Server(int id) {
        this->id = id;
        fd = socket(AF_INET, SOCK_STREAM, 0);
        addr = sockaddr_in();
        addr.sin_family = AF_INET;
        addr.sin_port = htons(80);
        is_music = (id == 2);
        is_busy = false;

        inet_pton(AF_INET, SERVER_IP[id], &addr.sin_addr);
        if (connect(fd, (sockaddr *) &addr, sizeof(addr)) == -1) {
            cerr << "problem with connect with server " << id << endl;
            exit(0);
        }
    }

};


struct Client {
    int fd;
    int id;
    Task *task;

    Client(int id) {
        this->id = id;
    }
};

vector<Task> task_list;
vector<Server> server_list;
vector<Client> client_list;

// initialize connections
void init() {
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

    // init socket
    socket(AF_INET, SOCK_STREAM, 0);
    // create a socket and bind it to port 80
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        cerr << "Problem binding socket to port 80" << endl;
        exit(0);
    }
}

bool are_matching(Task *task, Server *server) {
    return (task->type == MUSIC && server->is_music) || (task->type != MUSIC && !server->is_music);
}

void send_task_to_server(Server& server, Task& task) {
    server.task = task;
    server.is_busy = true;
    send(server.fd, &task.data, task.data.length(), 0);
    // remove task from task list
    for (int i = 0; i < task_list.size(); i++) {
        if (task_list[i].client_id == task.client_id) {
            task_list.erase(task_list.begin() + i);
            break;
        }
    }
    cout << "Task: " << task.data << " sent to server " << server.id << endl;
}

void match_task_to_server(int server_id) {
    Task *best_task = nullptr;
    Server& server = server_list[server_id];
    // sjf
    for (auto task: task_list) {
        if (are_matching(&task, &server)) {
            if (best_task == nullptr || task.time < best_task->time) {
                best_task = &task;
            }
        }
    }

    if (best_task) {
        send_task_to_server(server, *best_task);
        return;
    }

    // if music server
    if (server.is_music == MUSIC) {
        int min_wasting = MAX_INT;
        for (auto task: task_list) {
            if (task.type != MUSIC) {
                int wasted_time = task.time * (task.type == PICTURE ? 1 : 2);
                if (best_task == nullptr || wasted_time < min_wasting) {
                    best_task = &task;
                    min_wasting = wasted_time;
                }
            }
        }
        // * might need to check if null
        send_task_to_server(server, *best_task);
        return;
    }
    // if video server
    for (auto task: task_list) {
        if (best_task == nullptr || task.time < best_task->time) {
            best_task = &task;
        }
    }
    // * might need to check if null
    send_task_to_server(server, *best_task);


}

int get_tasks(){
    int counter = 0;
    bool connection = true;
    while (connection) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(sock, &readfds);
        struct timeval timeout{0, 0};
        // set to non-blocking
        select(sock + 1, &readfds, nullptr, nullptr, &timeout);

        connection = FD_ISSET(sock, &readfds);
        if (connection) {
            //accept connection from an incoming client
            struct sockaddr_in client_addr;
            int client_socket = accept(sock, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr);
            if (client_socket < 0) {
                return -1;
            }
            // read request
            char buffer[3] = {0};
            int valread = (int) read(client_socket, buffer, 3);
            if (valread < 0) {
                return -1;
            }
            // create task:
            string client_ip = inet_ntoa(client_addr.sin_addr);
            Task task = Task(buffer, client_ip[client_ip.length() - 1] - '0');
            task_list.push_back(task);
            // connect task to client
            Client &client = client_list[task.client_id];
            client.task = &task;
            client.fd = client_socket;
            // ! might need to add addr to client

            cout << "Task: " << task.data << " received from client " << task.client_id << endl;
            counter++;
        }
    }
    return counter;
}

void send_to_client(Task &task, char *buff){
    Client &client = client_list[task.client_id];
    send(client.fd, buff, 3, 0);
    close(client.fd);
    cout << "Task: " << task.data << " sent to client " << client.id << endl;
    client.fd = -1;
    client.task = nullptr;
}

bool server_is_free(Server &server){
    if (!server.is_busy)
        return true;
    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(server.fd, &readfds);
    struct timeval timeout{0, 0};
    // set to non-blocking
    select(server.fd + 1, &readfds, nullptr, nullptr, &timeout);
    if (FD_ISSET(server.fd, &readfds)){
        char buffer[3] = {0};
        int valread = (int) read(server.fd, buffer, 3);
        if (valread < 0) {
            cout << "Error reading from server " << server.id << endl;
            return false;
        }
        cout << "Task: " << server.task.data << " finished by server " << server.id << endl;
        // send to client
        send_to_client(server.task, buffer);
        server.is_busy = false;
        return true;
    }
    return false;
}
void load_balancer() {
    int task_received = get_tasks(); // get tasks from clients
    while(task_received == 0)
        task_received = get_tasks();
    int free_server_count = 0;
    while (true) {
        free_server_count = 0;
        for (auto& server: server_list){
            if (server_is_free(server)){
                match_task_to_server(server.id);
                free_server_count++;
            }
            // ? maybe more efficient to check all servers first
        }
        task_received = get_tasks();
        if (task_received == 0 && free_server_count == server_list.size() && task_list.empty())
            break;
    }
}

int main() {
    init();
    load_balancer();

    return 0;
}



