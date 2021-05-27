#include "forward.h"

struct pollfd fds[BACKLOG + 1];
char buf[BUFSIZE];
char enc_buf[ENC_BUFSIZE];

int nfds = 2;
int log = 1;
int port = 8080;
int exit_status = -1;


void server_close(int signal){
	for(int i = 0; fds[i].fd != 0; i++){
		if(fds[i].fd != CLOSED){
			close(fds[i].fd);
		}
	}
	printf("transmitter on [%d] terminated succesfully\n", port);
	exit(exit_status);
}

void check_args(int argc, char *argv[]){
	if(argc - 1 < MIN_ARGC){
		printf("%d args needed (port, log), but got only %d.\n",MIN_ARGC, argc - 1);
		exit(EXIT_FAILURE);
	}
	port = atoi(argv[1]);
	log = atoi(argv[2]);
}

void set_signal(){
	static struct sigaction act;
	act.sa_handler = server_close;
	sigaction(SIGINT, &act, NULL);
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
	fds[0].fd = *sc;
	fds[1].fd = *intrasc;
	fds[0].events = POLLIN;
	fds[1].events = POLLIN;

	printf("transmitter on [%d] set up and running\n", port);
}

void clearbufs(){
	memset(buf, 0, BUFSIZE);
	memset(enc_buf, 0 , ENC_BUFSIZE);
}

void test_for_poll_error(int i){
	if(
		fds[i].revents != POLLIN && 
		fds[i].revents != (POLLIN|POLLHUP)
		)
	{
		fprintf(stderr, "revents %d instead of POLLIN\n", fds[i].revents);
		perror("poll error");
		server_close(0);
	}
}

void accept_one_pending(int sc, int intrasc){
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
	send_operation(intrasc, nfds, CONNECT, enc_buf);
	nfds++;
}

void do_operation(){
	int connection_number = buf[1];
	if(buf[0] == CONNECT){
		printf("error: told to connect [%d]\n", connection_number);
		server_close(1);
	} else {
		close(fds[connection_number].fd);
		fds[connection_number].fd = CLOSED;
		if(log) printf("[%d] dropped from startpoint\n", connection_number);
	}
}

int main(int argc, char ** argv){
	check_args(argc, argv);
	set_signal();

	struct sockaddr_in addr;
	int sc, intrasc;
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
		for(int i = 0; i < snapshot_size; i++){
			if(fds[i].revents == 0){
				continue;
			}

			test_for_poll_error(i);

			printf("#################\n");
			if(fds[i].fd == sc){
				if(log) printf("accept\n");
				accept_one_pending(sc, intrasc);
			} else if(fds[i].fd == intrasc){
				if(log) printf("back\n");
				clearbufs();
				int enc_len = read_to_buf(enc_buf, ENC_BUFSIZE, i, fds, enc_buf, intrasc);
				if(log) printf("%d was read\n", enc_len);
				process_whole_enc_buf(enc_len, enc_buf, buf, fds);
			} else {
				if(log) printf("to\n");
				clearbufs();
				read_to_buf(buf, BUFSIZE, i, fds, enc_buf, intrasc);
				forward_to_intra(intrasc, i);
			}
		}
	}

	exit_status = 0;
	server_close(0);

	return EXIT_SUCCESS;
}