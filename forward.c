#include "forward.h"

extern struct pollfd* fds;
extern char buf[];
extern char enc_buf[];

int encode(int connection_number, char* enc_buf, char* buf){
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
	enc_buf[enc_i] = '\0';
	enc_buf[enc_i + 1] = MBYTE;
	enc_buf[enc_i + 2] = 0;
	return enc_i + 3;
}

int decode(int *connection_number, int *enc_index_offset, char *enc_buf, char* buf){
	//returns new length & shifts offset;
	int i, enc_i = *enc_index_offset;
	*connection_number = enc_buf[enc_i + 1];
	
	for(i = 0, enc_i = enc_i + 2;; i++, enc_i++){
		//TODO can fall out of buf here, relying on short messages - lazy impl
		if(enc_buf[enc_i] == MBYTE){
			if(enc_buf[enc_i + 1] != MBYTE){
				*enc_index_offset = enc_i + 2;
				return i;
			} else {
				buf[i] = enc_buf[enc_i];
				enc_i++; //next is same
			}
		} else {
			buf[i] = enc_buf[enc_i];
		}
	}	
}

int read_to_buf(char * buf, int bufsize, int i, struct pollfd* fds, char* enc_buf, int intrasc){
	int res, cl;
	cl = fds[i].fd;
	if( (res = read(cl, buf, bufsize)) > 0){
		printf("[%d] sent:\n", i);
	}

	if(res == 0){
		printf("[%d] disconnected\n", i);
		close(cl);
		fds[i].fd = CLOSED;
		send_operation(intrasc, i, DROP, enc_buf);
	}

	if(res == -1){
		perror("read error:");
		server_close(0);
	}
	return res;
}

void forward_to_endpoint(int i, int length, struct pollfd* fds, char* buf){
	printf("writing to %d from %d\n", fds[i].fd, i);
	if (write(fds[i].fd, buf, length) != length) {
		if (length > 0) 
			fprintf(stderr,"partial write");
		else {
			perror("write error");
			exit(-1);
		}
	}

	printf("[%d] %d bytes transmitted\n", i, length);
}

void process_whole_enc_buf(int enc_len){
	int connection_number, enc_offset = 0;
	while(enc_offset < enc_len){
		int length = decode(&connection_number, &enc_offset, enc_buf, buf);
		printf("connection_number[%d] with %d\n", connection_number, length);
		if(connection_number == 0){
			do_operation();
		}else{
			forward_to_endpoint(connection_number, length, fds, buf);
		}
		printf("processed %d/%d\n", enc_offset, enc_len);
	}
}

void forward_to_intra(int intrasc, int i){
	int length = encode(i, enc_buf, buf), elength;
	if (elength = write(intrasc, enc_buf, length) != length) {
		if (length > 0) 
			fprintf(stderr,"partial write: %d/%d", elength, length);
		else {
			perror("write error");
			exit(-1);
		}
	}
}

void send_operation(int intrasc, int connection_number, int operation){
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
	if (write(intrasc, enc_buf, length) != length) {
		if (length > 0) 
			fprintf(stderr,"partial write");
		else {
			perror("write error");
			exit(-1);
		}
	}
}
