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
	printf("reciever on [%d] terminated succesfully\n", port);
	exit(exit_status);
}

void check_args(int argc, char *argv[]){
	if(argc - 1 < MIN_ARGC){
		printf("%d args needed (dest port, log), but got only %d.\n",MIN_ARGC, argc - 1);
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
	fds[0].fd = *intrasc;
	fds[1].fd = *intraserver;
	fds[1].events = POLLIN;

	printf("reciever on [%d] set up and running\n", port);
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

void accept_one_pending_once(int sc, int *intrasc){
	int cl;
	if(fds[0].fd != CLOSED){
		printf("Error: connecting second transmitter\n");
		exit(EXIT_FAILURE);
	}
	if((cl = accept(sc, NULL, NULL)) == -1){
		perror("accept error");
	}
	fds[0].fd = cl;
	fds[0].events = POLLIN;
	*intrasc = cl;
}

void clearbufs(){
	memset(buf, 0, BUFSIZE);
	memset(enc_buf, 0 , ENC_BUFSIZE);
}

void connect_new(int i){
	int sc;
	if((sc = socket(AF_INET, SOCK_STREAM, DEFAULT_PROTOCOL)) == -1){
		perror("intra instantiation error");
	}
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = INADDR_ANY;
	if(connect(sc, (struct sockaddr*)&addr, sizeof(addr)) == -1){
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
		if(log) printf("[%d] connected to endpoint\n", connection_number);
	} else {
		close(fds[connection_number].fd);
		fds[connection_number].fd = CLOSED;
		if(log) printf("[%d] dropped from endpoint\n", connection_number);
	}
}

int main(int argc, char ** argv){
	check_args(argc, argv);
	set_signal();

	struct sockaddr_in addr;
	int intrasc = CLOSED, intraserver;
	int enc_offset = 0;
	init_server(&intrasc, &intraserver, &addr, buf);

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
			if(fds[i].fd == intraserver){
				accept_one_pending_once(intraserver, &intrasc);
				if(log) printf("accept\n");
			} else if(fds[i].fd == intrasc){
				clearbufs();
				if(log) printf("to\n");
				int enc_len = read_to_buf(enc_buf, ENC_BUFSIZE, i, fds, enc_buf, intrasc);
				if(log) printf("%d was read\n", enc_len);
				process_whole_enc_buf(enc_len, enc_buf, buf, fds);
			} else {
				if(log) printf("back\n");
				int len = read_to_buf(buf, BUFSIZE, i, fds, enc_buf, intrasc);
				forward_to_intra(intrasc, i);
			}
		}
	}

	exit_status = 0;
	server_close(0);

	return EXIT_SUCCESS;
}
