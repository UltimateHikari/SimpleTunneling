#include "forward.h"
#define INTRASOCK_INDEX 0
#define INTRASERV_INDEX 1
#define MIN_ARGC 3

struct pollfd fds[BACKLOG + 1];
char buf[BUFSIZE];
char enc_buf[ENC_BUFSIZE];
int intrasc = CLOSED;
int intraserver;


int nfds = 2;
int logv = 1;
char* hostname;
char *portname;
int port = 8080;
int exit_status = -1;
char* exit_name = "reciever";

void check_args(int argc, char *argv[]){
	if(argc - 1 < MIN_ARGC){
		printf("%d args needed (hostname, port, logv), but got only %d.\n",MIN_ARGC, argc - 1);
		exit(EXIT_FAILURE);
	}
	hostname = argv[1];
	portname = argv[2];
	port = atoi(argv[2]);
	logv = atoi(argv[3]);
}

void init_server(int *intrasc, int *intraserver, struct sockaddr_in *addr, char buf[]){
	/**
	 * 0 intra socket
	 * 1 intra server
	 * others with port
	 */

	if((*intraserver = socket(AF_INET, SOCK_STREAM, DEFAULT_PROTOCOL)) == -1){
		perror("intra instantiation error");
	}
	struct sockaddr_in recv_addr;
	memset(&recv_addr, 0, sizeof(recv_addr));
	recv_addr.sin_family = AF_INET;
	recv_addr.sin_port = htons(RECV_PORT);
	recv_addr.sin_addr.s_addr = INADDR_ANY;
	if(bind(*intraserver, (struct sockaddr*)&recv_addr, sizeof(recv_addr)) == -1){
		perror("bind error");
	}
	if(listen(*intraserver, BACKLOG) == -1){
		perror("listen error");
	}
	
	memset(buf, 0, BUFSIZE);
	fds[INTRASOCK_INDEX].fd = *intrasc;
	fds[INTRASERV_INDEX].fd = *intraserver;
	fds[INTRASERV_INDEX].events = POLLIN;

	printf("reciever on [%d] set up and running\n", port);
}

void accept_one_pending_once(int sc){
	int cl;
	if(fds[INTRASOCK_INDEX].fd != CLOSED){
		printf("Error: connecting second transmitter\n");
		exit(EXIT_FAILURE);
	}
	if((cl = accept(sc, NULL, NULL)) == -1){
		perror("accept error");
	}
	fds[INTRASOCK_INDEX].fd = cl;
	fds[INTRASOCK_INDEX].events = POLLIN;
	intrasc = cl;
}

void connect_new(int i){
	int sc;
	if((sc = socket(AF_INET, SOCK_STREAM, DEFAULT_PROTOCOL)) == -1){
		perror("intra instantiation error");
	}
	struct addrinfo *result = NULL;
	struct addrinfo *ptr = NULL;
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	
	if(getaddrinfo(hostname, portname, &hints, &result)){
		perror(hostname);
	}

	if(connect(sc, (struct sockaddr*)result->ai_addr, sizeof(*(result->ai_addr))) == -1){
		perror("endpoint connect error");
		exit(EXIT_FAILURE);
	}
	fds[i].fd = sc;
	fds[i].events = POLLIN;
	printf("returned socket %d wrote to %d\n", sc, i);
		int flag = 1; 
	setsockopt(sc, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));
	nfds++;
}

void do_operation(){
	int connection_number = buf[1];
	if(buf[0] == CONNECT){
		connect_new(connection_number);
		if(logv) printf("[%d] connected to endpoint\n", connection_number);
	} else {
		if(logv) printf("[%d] dropping..\n");
		drop_by_index(connection_number);
		if(logv) printf("[%d] dropped from endpoint\n", connection_number);
	}
}

void switch_behaviour(int i){
	printf("#################\n");
	switch(i){

		case INTRASERV_INDEX:
			accept_one_pending_once(intraserver);
			if(logv) printf("accept\n");
			break;

		case INTRASOCK_INDEX:
			if(logv) printf("to\n");
			int enc_len = read_to_buf(enc_buf, ENC_BUFSIZE, i);
			process_whole_enc_buf(enc_len);
			break;

		default:
			if(logv) printf("back\n");
			int len = read_to_buf(buf, BUFSIZE, i);
			if(len == 0){
				send_operation(i, DROP);
			} else {
				forward_to_intra(i, len);
			}
			break;
	}
}

int main(int argc, char ** argv){
	check_args(argc, argv);
	set_signal();

	struct sockaddr_in addr;
	int enc_offset = 0;
	init_server(&intrasc, &intraserver, &addr, buf);

	for(;;){
		int res = poll(fds, nfds, TIMEOUT);

		if(res < 0){
			perror("poll error");
			server_close(0);
		}

		if(res == 0){
			continue;
		}

		int snapshot_size = nfds;
		for(int i = snapshot_size - 1; i > -1; i--){
			// for preventing closing before writing all data
			if(fds[i].revents == 0){
				continue;
			}
			test_for_poll_error(i);
			switch_behaviour(i);
		}
	}

	exit_status = 0;
	server_close(0);

	return EXIT_SUCCESS;
}
