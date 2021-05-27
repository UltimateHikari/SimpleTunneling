#include "forward.h"
#define INTRASOCK_INDEX 1
#define OUTERSERV_INDEX 0
#define MIN_ARGC 2

struct pollfd fds[BACKLOG + 1];
char buf[BUFSIZE];
char enc_buf[ENC_BUFSIZE];
int intrasc, sc;

int nfds = 2;
int logv = 1;
int port = 8080;
int exit_status = -1;
char* exit_name = "transmitter";

void check_args(int argc, char *argv[]){
	if(argc - 1 < MIN_ARGC){
		printf("%d args needed (port, logv), but got only %d.\n",MIN_ARGC, argc - 1);
		exit(EXIT_FAILURE);
	}
	port = atoi(argv[1]);
	logv = atoi(argv[2]);
}

void init_server(int *sc, int *intrasc, struct sockaddr_in *addr){
	/**
	 * 0 for listening
	 * 1 for reciever
	 * others for clients
	 */
	if((*sc = socket(AF_INET, SOCK_STREAM, DEFAULT_PROTOCOL)) == -1){
		perror("out instantiation error");
	}
	memset(addr, 0, sizeof(*addr));
	addr->sin_family = AF_INET;
	addr->sin_port = htons(port);
	addr->sin_addr.s_addr = INADDR_ANY; //all interfaces
	if(bind(*sc, (struct sockaddr*)addr, sizeof(*addr)) == -1){
		perror("bind error");
	}
	if(listen(*sc, BACKLOG) == -1){
		perror("listen error");
	}

	if((*intrasc = socket(AF_INET, SOCK_STREAM, DEFAULT_PROTOCOL)) == -1){
		perror("intra instantiation error");
	}
	struct sockaddr_in recv_addr;
	memset(&recv_addr, 0, sizeof(recv_addr));
	recv_addr.sin_family = AF_INET;
	recv_addr.sin_port = htons(RECV_PORT);
	recv_addr.sin_addr.s_addr = INADDR_ANY;
	if(connect(*intrasc, (struct sockaddr*)&recv_addr, sizeof(recv_addr)) == -1){
		perror("intraconnect error");
		exit(EXIT_FAILURE);
	}
	fds[OUTERSERV_INDEX].fd = *sc;
	fds[INTRASOCK_INDEX].fd = *intrasc;
	fds[OUTERSERV_INDEX].events = POLLIN;
	fds[INTRASOCK_INDEX].events = POLLIN;

	printf("transmitter on [%d] set up and running\n", port);
}

void accept_one_pending(){
	int cl;
	if((cl = accept(sc, NULL, NULL)) == -1){
		perror("accept error");
	}
	if(nfds > BACKLOG){
		perror("too much connections, accepting but ignoring");
		return;
	}
	fds[nfds].fd = cl;
	fds[nfds].events = POLLIN;
	send_operation(nfds, CONNECT);
	nfds++;
}

void do_operation(){
	int connection_number = buf[1];
	if(buf[0] == CONNECT){
		printf("error: %s told to connect [%d]\n", exit_name, connection_number);
		server_close(1);
	} else {
		close(fds[connection_number].fd);
		fds[connection_number].fd = CLOSED;
		if(logv) printf("[%d] dropped from startpoint\n", connection_number);
	}
}

void switch_behaviour(int i){
	printf("#################\n");
	switch(i){

		case OUTERSERV_INDEX:
			if(logv) printf("accept\n");
			accept_one_pending();
			break;

		case INTRASOCK_INDEX:
			if(logv) printf("back\n");
			int enc_len = read_to_buf(enc_buf, ENC_BUFSIZE, i);
			process_whole_enc_buf(enc_len);
			break;

		default:
			if(logv) printf("to\n");
			int length = read_to_buf(buf, BUFSIZE, i);
			forward_to_intra(i, length);
	}
}

int main(int argc, char ** argv){
	check_args(argc, argv);
	set_signal();

	struct sockaddr_in addr;
	init_server(&sc, &intrasc, &addr);

	for(;;){
		int res = poll(fds, nfds, TIMEOUT);

		if(res < 0){
			perror("poll error");
			server_close(0);
		}

		if(res == 0){
			//printf("poll [%dms]: expired\n", TIMEOUT);
			continue;
		}

		int snapshot_size = nfds;
		for(int i = snapshot_size - 1; i > -1; i--){
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
