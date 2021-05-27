#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <poll.h>


#define DEFAULT_PROTOCOL 0
#define BACKLOG 255
#define BUFSIZE 2048
#define ENC_BUFSIZE 2*BUFSIZE
#define MIN_ARGC 2
#define CLOSED -1
#define TIMEOUT 3000
#define RECV_PORT 8000
#define MBYTE 0b01111110
#define CONNECT 1
#define DROP 2

int encode(int connection_number, char* enc_buf, char* buf);
int decode(int *connection_number, int *enc_index_offset, char* enc_buf, char* buf);
int read_to_buf(char * buf, int bufsize, int i, struct pollfd* fds, char* enc_buf, int intrasc);
void server_close(int signal);
void forward_to_endpoint(int i, int length, struct pollfd* fds, char* buf);
void process_whole_enc_buf(int enc_len, char* enc_buf, char* buf, struct pollfd* fds);
void do_operation();
void send_operation(int intrasc, int connection_number, int operation, char*enc_buf);
void forward_to_intra(int intrasc, int i);