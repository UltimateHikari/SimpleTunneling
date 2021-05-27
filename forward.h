#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
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
#define CLOSED -1
#define TIMEOUT 3000
#define RECV_PORT 8000
#define MBYTE 0b01111110
#define CONNECT 1
#define DROP 2

void check_args(int argc, char *argv[]);

int read_to_buf(char * buf, int bufsize, int i);
void process_whole_enc_buf(int enc_len);

void send_operation(int connection_number, int operation);
void forward_to_intra(int i, int raw_length);
void forward_to_endpoint(int i, int length);

void server_close(int signal);
void set_signal();
void test_for_poll_error(int i);
void do_operation();
void drop_by_index(int i);