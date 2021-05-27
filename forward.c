#include "forward.h"

extern struct pollfd fds[];
extern char buf[];
extern char enc_buf[];
extern int intrasc;

extern int exit_status;
extern char* exit_name;
extern int port;

int encode(int connection_number){
	printf("encoding |%s|\n", buf);
	//returns length of buffer;
	enc_buf[0] = MBYTE;
	enc_buf[1] = connection_number;
	int i, enc_i;
	for(i = 0, enc_i = 2; buf[i] != '\0'; i++, enc_i++){
		enc_buf[enc_i] = buf[i];
		if(buf[i] == MBYTE){
			enc_i++;
			enc_buf[enc_i] = MBYTE;
		}
	}
	//extra byte in decoding comes from here
	enc_buf[enc_i] = '\0';
	enc_buf[enc_i + 1] = MBYTE;
	enc_buf[enc_i + 2] = 0;
	return enc_i + 3;
}

int decode(int *connection_number, int *enc_index_offset){
	//returns new length & shifts offset;
	int i, enc_i = *enc_index_offset;
	*connection_number = enc_buf[enc_i + 1];
	
	for(i = 0, enc_i = enc_i + 2;; i++, enc_i++){
		//TODO can fall out of buf here, relying on short messages - lazy impl
		if(enc_buf[enc_i] == MBYTE){
			if(enc_buf[enc_i + 1] != MBYTE){
				*enc_index_offset = enc_i + 2;printf("decodedd |%s|\n", buf);
				return i;
			} else {
				buf[i] = enc_buf[enc_i];
				enc_i++; //next is same
			}
		} else {
			buf[i] = enc_buf[enc_i];
		}
	}
	printf("decoded |%s|\n", buf);
}

void clearbufs(){
	memset(buf, 0, BUFSIZE);
	memset(enc_buf, 0 , ENC_BUFSIZE);
}

int read_to_buf(char * buf, int bufsize, int i){
	clearbufs();
	int res, cl;
	cl = fds[i].fd;
	if( (res = read(cl, buf, bufsize)) > 0){
		printf("read from [%d] %d bytes\n", i, res);
	}

	if(res == 0){
		printf("[%d] disconnected\n", i);
		close(cl);
		fds[i].fd = CLOSED;
		//send_operation(i, DROP);
	}

	if(res == -1){
		perror("read error:");
		server_close(0);
	}
	return res;
}

void process_whole_enc_buf(int enc_len){
	int connection_number, enc_offset = 0;
	while(enc_offset < enc_len){
		int length = decode(&connection_number, &enc_offset);
		printf("connection_number[%d] with length %d\n", connection_number, length);
		if(connection_number == 0){
			do_operation();
		}else{
			forward_to_endpoint(connection_number, length);
		}
		printf("processed %d/%d\n", enc_offset, enc_len);
	}
}

void send_to_intra(int length){
	int elength;
	if ((elength = write(intrasc, enc_buf, length)) != length) {
		if (elength > -1) 
			fprintf(stderr,"partial write: %d/%d\n", elength, length);
		else {
			perror("write error");
			exit(-1);
		}
	}
	printf("[intra] %d bytes sent\n", elength);
}

void send_operation(int connection_number, int operation){
	clearbufs();
	int length = 6;
	enc_buf[0] = MBYTE;
	enc_buf[1] = 0;
	enc_buf[2] = operation;
	enc_buf[3] = connection_number;
	enc_buf[4] = MBYTE;
	enc_buf[5] = 0;
	if(connection_number == MBYTE){
		enc_buf[5] = MBYTE;
		enc_buf[6] = 0;
		length = 7;
	}
	send_to_intra(length);
}

void forward_to_intra(int i){
	int length = encode(i);
	send_to_intra(length);
}

void forward_to_endpoint(int i, int length){
	int elength;
	if ((elength = write(fds[i].fd, buf, length)) != length) {
		if (elength > -1) 
			fprintf(stderr,"partial write: %d/%d\n", elength, length);
		else {
			perror("write error");
			exit(-1);
		}
	}

	printf("[%d] %d bytes sent\n", i, elength);
}

void server_close(int signal){
	for(int i = 0; fds[i].fd != 0; i++){
		if(fds[i].fd != CLOSED){
			close(fds[i].fd);
		}
	}
	printf("%s on [%d] terminated succesfully\n", exit_name, port);
	exit(exit_status);
}

void set_signal(){
	static struct sigaction act;
	act.sa_handler = server_close;
	sigaction(SIGINT, &act, NULL);
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