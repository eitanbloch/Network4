//Tikshoret hw4


//this code is for the load balancing of the clients to the server


#include <stdio.h>

#include <stdlib.h>

#include <string.h>

#include <unistd.h>

#include <sys/types.h>

//socket stuff

#include <sys/socket.h>

#include <netinet/in.h>

#include <arpa/inet.h>


//include the queue

#include <queue>

//now list

#include <list>


//define structs for our use


//struct for the server

typedef struct server {

	char* ip;

	int port;

	int load;

} server;


//struct for the client

typedef struct client {

	char* ip;

	int port;

} client;


/*//enum for requests types

typedef enum request_type {

	Music,

	Video,

	Photo

} request_type;*/



//struct for the request - type and one digit int of how much time it would take

typedef struct request {

	int video_server_price;

	int music_server_price;

	//client socket

	int client_socket;

	char original_request[2];

} request;


//we are the load balancer so there is no need for a struct for it


//return the server index we need to send to

int handle_request(request req, int* servers_load)

{

	return 1;

}




//the main loop that recieve connections from client and handle them

//returns 0 on success and -1 on failure

int main(int argc, char* argv[]) {


	//connect to servers: 192.168.0.101-192.168.0.103

	//port:80

	int video_server_1_socket = socket(AF_INET, SOCK_STREAM, 0);

	int video_server_2_socket = socket(AF_INET, SOCK_STREAM, 0);

	int music_server_socket = socket(AF_INET, SOCK_STREAM, 0);

	//now connect to the servers

	//connect to video server 1

	struct sockaddr_in video_server_1;

	video_server_1.sin_family = AF_INET; //this line means we are using ipv4

	video_server_1.sin_port = htons(80); //this line means we are using port 80

	video_server_1.sin_addr.s_addr = inet_addr("192.168.0.101"); //this line means we are using the ip

	//connect to video server 2

	struct sockaddr_in video_server_2;

	video_server_2.sin_family = AF_INET; //this line means we are using ipv4

	video_server_2.sin_port = htons(80); //this line means we are using port 80

	video_server_2.sin_addr.s_addr = inet_addr("192.168.0.102"); //this line means we are using the ip

	//connect to music server

	struct sockaddr_in music_server;

	music_server.sin_family = AF_INET; //this line means we are using ipv4

	music_server.sin_port = htons(80); //this line means we are using port 80

	music_server.sin_addr.s_addr = inet_addr("192.168.0.103"); //this line means we are using the ip

	//now connect to the servers

	//connect to video server 1

	if (connect(video_server_1_socket, (struct sockaddr*)&video_server_1, sizeof(video_server_1)) < 0) {

		//printf("Error connecting to video server 1\n");

		return -1;

	}

	//connect to video server 2

	if (connect(video_server_2_socket, (struct sockaddr*)&video_server_2, sizeof(video_server_2)) < 0) {

		//printf("Error connecting to video server 2\n");

		return -1;

	}

	//connect to music server

	if (connect(music_server_socket, (struct sockaddr*)&music_server, sizeof(music_server)) < 0) {

		//printf("Error connecting to music server\n");

		return -1;

	}


	//put them in an array

	int servers_sockets[3] = { video_server_1_socket,video_server_2_socket,music_server_socket };

	//printf("now we have the servers list\n");

	//we use one port so there can only be one port!

	//we will use port 80

	int load_balancer_socket = socket(AF_INET, SOCK_STREAM, 0);

	struct sockaddr_in load_balancer;

	load_balancer.sin_family = AF_INET; //this line means we are using ipv4

	load_balancer.sin_port = htons(80); //this line means we are using port 80

	load_balancer.sin_addr.s_addr = htonl(INADDR_ANY); //this line means we are using the ip

	//now we need to bind the socket to the ip and port

	if (bind(load_balancer_socket, (struct sockaddr*)&load_balancer, sizeof(load_balancer)) < 0) {

		//printf("Error binding socket\n");

		return -1;

	}

	//now we need to listen to the clients

	if (listen(load_balancer_socket, 5) < 0) {

		//printf("Error listening to clients\n");

		return -1;

	}

	

	//now we need to accept the clients

	struct sockaddr_in client;

	int client_size = sizeof(client);

	char buffer[2];

	

	//queue of requests

	std::list<request> requests;

	//now 3 queues, one for each server, that holds the client socket he is currently handling

	std::queue<int> video_server_1_queue;

	std::queue<int> video_server_2_queue;

	std::queue<int> music_server_queue;

	std::queue<int> server_queues[3] = { video_server_1_queue,video_server_2_queue,music_server_queue };

	bool working[3] = {false, false, false};

	char types[4] = "VVM";

	//int counter = 0;

	int initial_workloads[3] = { 0,0,0 };

	//the loop

	while (true)

	{

		//counter++;

		/*//accept new client connection 

		auto client_socket = accept(load_balancer_socket, (struct sockaddr*)&client, (socklen_t*)&client_size);

		if (client_socket < 0) {

			//printf("Error accepting client\n");

			return -1;

		}

		//now we need to read the request

		if (read(client_socket, buffer, 2) < 0) {

			//printf("Error reading from client\n");

			return -1;

		}

		//now we need to handle the request

		//we will use the first digit to determine the request type

		//and the second digit to determine the time it would take*/

		

		//non blocking version:

		//non blocking accept (select) new connection:

		fd_set readfds;

		FD_ZERO(&readfds);

		FD_SET(load_balancer_socket, &readfds);

		struct timeval timeout;

		timeout.tv_sec = 0;

		timeout.tv_usec = 0;

		int select_result = select(load_balancer_socket + 1, &readfds, NULL, NULL, &timeout);

		//if (select_result < 0) {

		//	//getting here means there was an error, if there was no connection ready it would return 0

		//	printf("Error selecting\n");

		//	return -1;

		//}

		bool new_connection = false;

		if (FD_ISSET(load_balancer_socket, &readfds)) {

			new_connection = true;

		}

		//read to buffer if there is a new connection

		if (new_connection) {

			//printf("new client connection\n");

			//accept new client connection 

			int client_socket = accept(load_balancer_socket, (struct sockaddr*)&client, (socklen_t*)&client_size);

			//if (client_socket < 0) {

			//	printf("Error accepting client\n");

			//	return -1;

			//}

			////now we need to read the request/ changed to recv

			if (recv(client_socket, buffer, 2,0) < 0) {

				//printf("Error reading from client\n");

				return -1;

			}

			//printf("request received: %s\n", buffer);

			//now we need to handle the request

			//we will use the first digit to determine the request type

			struct request r = { 1,1, client_socket, { buffer[0],buffer[1] } };

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

			//else {

			//	//error

			//	printf("Error reading request type\n");

			//	return -1;

			//}

			//now put the request in the queue

			requests.push_back(r);

			//printf("pushed new request\n");

		}





		





		

		//now the part where we check the status of the servers, and retrieve their messeges to the clients

		//loop through the 3 servers sockets

		for (int i = 0; i < 3; i++)

		{

			//cur server is i from the server list

			int cur_server_socket = servers_sockets[i];

			//check if there is a message from the server

			fd_set readfds;

			FD_ZERO(&readfds);

			FD_SET(cur_server_socket, &readfds);

			struct timeval timeout;

			timeout.tv_sec = 0;	

			timeout.tv_usec = 0;

			int select_result = select(cur_server_socket + 1, &readfds, NULL, NULL, &timeout);

			//if (select_result < 0) {

			//	//getting here means there was an error, if there was no connection ready it would return 0

			//	printf("Error selecting\n");

			//	return -1;

			//}

			bool new_message = false;

			if (FD_ISSET(cur_server_socket, &readfds)) {

				new_message = true;

			}

			//read to buffer if there is a new message

			if (new_message) {

				//printf("new message from server\n");

				//print the counter

				//printf("counter: %d\n", counter);

				//read the message

				char buffer[2];

				//there was only select, forgot to do accept

				//recv maybe okay because we know there is already answer waiting

				if (recv(cur_server_socket, buffer, 2, MSG_DONTWAIT) < 0) { //this is blocking

					//printf("Error reading from server\n");

					return -1;

				}

				//get the relevant client socket

				int client_socket = server_queues[i].front();

				//remove the client socket from the queue

				server_queues[i].pop();

				

				//now we need to send the message to the client

				//we will use the first digit to determine the request type

				//and the second digit to determine the time it would take

				//send the message to the client

				if (send(client_socket, buffer, 2, MSG_DONTWAIT) < 0) {

					//printf("Error sending message to client\n");

					return -1;

				}

				//never close server connection, close r.client_socket only

				if (close(client_socket) < 0) {

					//printf("Error closing client socket\n");

					return -1;

				}

				//free the 

				//now we need to update the server load

				//servers_load[i] -= buffer[1] - '0';

				working[i] = false;

				initial_workloads[i] = 0;

			}

			

		}





		//

		//we can change it to while, later

		//now we need to check if there are any requests in the queue

		requests.begin();

		if (!requests.empty()) {

			//printf("handling requests\n");

			int len = requests.size();

			for (int i = 0; i < 3; i++) {

				if (requests.empty())

				{

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

					for (int req = 0; req < len; req++)

					{


						//request current_req = requests[req];

						//list has no [] operand, soadvance first amd take front


						request current_req = requests.front();

						char req_type = current_req.original_request[0];

						int req_time = current_req.original_request[1] - '0';

						if (req_type == types[i])

						{

							if (req_time > maximum_fitting_time)

							{

								maximum_fitting_time = req_time;

								chosen_indx = req;

								chosen_request = current_req;

								found_good = true;

								chosen_reqiest_workload = req_time;

							}

						}

						else {

							if (!found_good)

							{

								//printf("no good\n");

								if (types[i] == 'V')

								{

									req_time = current_req.video_server_price;

								}

								else

								{

									

									req_time = current_req.music_server_price;

									

								}

								int real_finish_time = req_time + initial_workloads[i];

								if (real_finish_time < minimum_bad_time)

								{

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

					if (i == 2 && !found_good)

					{

						int req_time_on_0 = initial_workloads[0]+chosen_request.video_server_price;

						int req_time_on_1 = initial_workloads[1] + chosen_request.video_server_price;

						//print two alternatives weights

						//printf("req_time_on_0: %d\n", req_time_on_0);

						//printf("req_time_on_1: %d\n", req_time_on_1);

						//print initial workloads

						//printf("initial_workloads[0]: %d\n", initial_workloads[0]);

						//printf("initial_workloads[1]: %d\n", initial_workloads[1]);

						if (chosen_reqiest_workload > req_time_on_0 || chosen_reqiest_workload > req_time_on_1)

						{

							continue;

						}

					}

					else if (!found_good)

					{

						int req_time_on_2 = initial_workloads[2] + chosen_request.music_server_price;

						if (chosen_reqiest_workload > req_time_on_2 && chosen_request.original_request[0] == 'M')

						{

							continue;

						}

					}

					//now send to the relevant server the request from the chosen index

					if (send(servers_sockets[i], chosen_request.original_request, sizeof(chosen_request.original_request), /*0*/MSG_DONTWAIT) < 0) {

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

					if (chosen_indx > 0)

					{

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