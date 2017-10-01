/*
 * proxy.c - CS:APP Web proxy
 *
 * TEAM MEMBERS:
 *     Zhu Bojun 515030910298 869502588@qq.com
 * 
 * IMPORTANT: Give a high level description of your code here. You
 * must also provide a header comment at the beginning of each
 * function that describes what that function does.
 *
 * For this naive proxy, I use the structure of the Producer-Consumer 
 * model to manange the concurrancy request. When the proxy accept a 
 * request from the client, insert it into the buffer, when the thread 
 * free, it take the request out from the buffer, and manange it.
 */ 

#include "csapp.h"

#define NTHREADS	4 
#define SBUFSIZE	16

typedef struct {
	/* Some information of client */
	int fd;
	struct sockaddr_in client_sock;
} conn_t;

typedef struct {
	int *buf;							/* Buffer array */
	struct sockaddr_in *client_sock;	/* Buffer of struct of sockaddr_in */
	int n;								/* Maximum number of slots */
	int front;							/* buf[(front+1)%n] is first item */
	int rear;							/* buf[rear%n] is last item */
	sem_t mutex;						/* Protects accesses to buf */
	sem_t slots;						/* Counts available slots */
	sem_t items;						/* Counts available items */
} sbuf_t;

sem_t mutex_log;

sem_t mutex_get_host;

sbuf_t sbuf;

FILE *logfile;

/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, int  *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
void doit(conn_t conn);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
int open_clientfd_ts(char *hostname, int port);
void write_log(struct sockaddr_in client_sock, char *uri, int size);

void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item, struct sockaddr_in * client_sock);
conn_t sbuf_remove(sbuf_t *sp);

void *thread(void *vargp);


ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen);
ssize_t Rio_readn_w(rio_t *rp, void *usrbuf, size_t nbytes);
void Rio_writen_w(int fd, void *usrbuf, size_t n);

/* 
 * main - Main routine for the proxy program 
 */
int main(int argc, char **argv)
{	
	int listenfd, connfd, port, clientlen, i;
	struct sockaddr_in clientaddr;
	pthread_t tid;

    /* Check arguments */
    if (argc != 2) {
		fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
		exit(0);
    }
	
	port = atoi(argv[1]);
	Signal(SIGPIPE, SIG_IGN);


	sbuf_init(&sbuf, SBUFSIZE);
	Sem_init(&mutex_get_host, 0, 1);
	Sem_init(&mutex_log, 0, 1);

	listenfd = Open_listenfd(port);

	for (i = 0; i < NTHREADS; i++)
		Pthread_create(&tid, NULL, thread, NULL);

	while(1) {
		connfd = Accept(listenfd, (SA *)&clientaddr, (socklen_t *)&clientlen);
		sbuf_insert(&sbuf, connfd, &clientaddr);
	}

	sbuf_deinit(&sbuf);
	exit(0);
}

/* Thread routine */
void *thread(void *vargp) {
	Pthread_detach(pthread_self());	/* Detach to avoid memory leak */
	conn_t conn;
	while(1) {
		conn = sbuf_remove(&sbuf);	/* Remove connfd from buffer */
		doit(conn);					/* Service client */
		Close(conn.fd);
	}
}

/* Create an empty, bounded, shared FIFO buffer with n slots */
void sbuf_init(sbuf_t *sp, int n) {
	sp->buf = Calloc(n, sizeof(int));
	sp->client_sock = Calloc(n, sizeof(struct sockaddr_in));
	sp->n = n;					/* Buffer holds max of n items */
	sp->front = sp->rear = 0;	/* Empty buffer iff front == rear */
	Sem_init(&sp->mutex, 0, 1);	/* Binary semaphore for locking */
	Sem_init(&sp->slots, 0, n);	/* Initially, buf has n empty slots */
	Sem_init(&sp->items, 0 ,0); /* Initially, buf has zero data items */
}

/* Clean up buffer sp */
void subf_deinit(sbuf_t *sp) {
	Free(sp->buf);
	Free(sp->client_sock);
}

/* Insert item onto the rear of shared buffer sp */
void sbuf_insert(sbuf_t *sp, int item, struct sockaddr_in *client_addr) {
	P(&sp->slots);							/* Wait for available slot */
	P(&sp->mutex);							/* Lock the buffer */
	sp->buf[(++sp->rear)%(sp->n)] = item;	/* Insert the item */
	memcpy(&sp->client_sock[(sp->rear)%(sp->n)], 
			client_addr, sizeof(struct sockaddr_in));
	V(&sp->mutex);							/* Unlock the buffer */
	V(&sp->items);							/* Announce available item */
}

conn_t sbuf_remove(sbuf_t *sp) {
	conn_t result;

	P(&sp->items);							/* Wait for available item */
	P(&sp->mutex);							/* Lock the buffer */
	result.fd = sp->buf[(++sp->front)%(sp->n)];	/* Remove item */
	result.client_sock = sp->client_sock[(sp->front)%(sp->n)];
	V(&sp->mutex);							/* Unlock the buffer */
	V(&sp->slots);							/* Announce available slot */
	return result;
}



/*
 * parse_uri - URI parser
 * 
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, int *port)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0) {
		hostname[0] = '\0';
		return -1;
    }
       
    /* Extract the host name */
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';
    
    /* Extract the port number */
    *port = 80; /* default */
    if (*hostend == ':')   
		*port = atoi(hostend + 1);
    
    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
		pathname[0] = '\0';
    }
    else {
		pathbegin++;	
		strcpy(pathname, pathbegin);
    }

    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring. 
 * 
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, 
		      char *uri, int size)
{
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    /* 
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 13, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;


    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s %d\n", time_str, a, b, c, d, uri, size);
}

/*
 * doit - manage the request of the client
 *
 * The input is the structure containing the fd and sockadddr_in,
 * parse it and send it to the server, then get the response of the
 * server and send it to the client
 */
void doit(conn_t conn) {
	char buf[MAXLINE], uri[MAXLINE], method[MAXLINE], version[MAXLINE];
	char hostname[MAXLINE], pathname[MAXLINE];
	char toserver[MAXLINE], toclient[MAXLINE];
	int port = 80;	/* default */
	int serverfd;
	int clientfd = conn.fd;
	size_t size, total = 0;
	rio_t rio_c, rio_s;

	Rio_readinitb(&rio_c, clientfd);

	/* Get line from the client and parse it */
	Rio_readlineb_w(&rio_c, buf, MAXLINE);
	sscanf(buf, "%s %s %s", method, uri, version);

	/* Determine the method */
	if (strcasecmp(method, "GET")) {
		clienterror(clientfd, method, "501", "Not Implemented", 
				"Tiny does not implement this method");
		return;
	}

	if (parse_uri(uri, hostname, pathname, &port) == -1)
		return;

	if ((serverfd = open_clientfd_ts(hostname, port)) < 0) {
		clienterror(clientfd, method, "403", "The address cannot find",
				"Tiny cannot get conneted to the address");
		return;
	}
	
	sprintf(toserver, "%s /%s HTTP/1.0\n", method, pathname);
	sprintf(toserver, "%sHost: %s\n\r\n", toserver, hostname);
	/* Send the request to the server */
	Rio_writen_w(serverfd, toserver, strlen(toserver));
	
	Rio_readinitb(&rio_s, serverfd);
	/* Get the response from the server and send them to the client */
	while((size = Rio_readn_w(&rio_s, toclient, MAXLINE)) > 0) {
		Rio_writen_w(clientfd, toclient, size);
		total += size;
	}
	/* Write the information to the log*/
	write_log(conn.client_sock, uri, total);
	Close(serverfd);
}

/* write_log - write information to the log file
 * The input is the structure of sockaddr_in, uri and the size of the content
 * getting from the server
 */
void write_log(struct sockaddr_in client_sock, char *uri, int size) {
	char logstring[MAXLINE];
	format_log_entry(logstring, &client_sock, uri, size);
	P(&mutex_log);						/* Lock the file */
	logfile = fopen("proxy.log", "a+");	/* Open the file */
	fprintf(logfile, "%s", logstring);	/* Write it */
	fclose(logfile);					/* Close file */
	V(&mutex_log);						/* Unlock it */
}

/* clienterror - Send the error information to the client
 * The input is the fd and some information about the error,
 * and send it to the client
 */
void clienterror(int fd, char *cause, char *errnum,
		char *shortmsg, char*longmsg) {
	char buf[MAXLINE], body[MAXBUF];
	
	/* Build the HTTP response body */
	sprintf(body, "<html><title>Tiny Error</title>");
	sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
	sprintf(body, "%s%s: %s\r\n", body, longmsg, cause);
	sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
	sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);

	/* Print the HTTP response */
	sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
	Rio_writen_w(fd, buf, strlen(buf));
	sprintf(buf, "Content-type: text/html\r\n");
	Rio_writen_w(fd, buf, strlen(buf));
	sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
	Rio_writen_w(fd, buf, strlen(buf));
	Rio_writen_w(fd, body, strlen(body));
}

/* open_clientfd_ts - use the lock-and-copy technique
 * when it calls gethostbyaddr to avoid data race
 */
int open_clientfd_ts(char *hostname, int port) {
	int clientfd;
	struct hostent *hp;
	struct sockaddr_in serveraddr;

	if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -1;			/* Check errno for cause of error */

	P(&mutex_get_host);		/* Lock it */

	/* Fill in the server's IP address and port */
	if ((hp = gethostbyname(hostname)) == NULL) {
		V(&mutex_get_host);	/* Unlock it */
		return -2;			/* Check h_errno for cause of error */
	}

	V(&mutex_get_host);		/* Unlock it */

	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	bcopy((char *)hp->h_addr_list[0], 
			(char *)&serveraddr.sin_addr.s_addr, hp->h_length);
	serveraddr.sin_port = htons(port);
	
	/* Establish a connection with the server */
	if (connect(clientfd, (SA *) &serveraddr, sizeof(serveraddr)) < 0)
		return -1;
	return clientfd;
}

/* Rio_readn_w - the wrapper of the function of rio_readn,
 * if it encounters error, just print the error information 
 * on the screen
 */
ssize_t Rio_readn_w(rio_t *rp, void *usrbuf, size_t nbytes) {
	ssize_t n;
	if ((n = rio_readnb(rp, usrbuf, nbytes)) < 0) {
		fprintf(stderr, "rio_read error: %s\n", strerror(errno));
		n = 0;
	}

	return n;
}

/* Rio_writen_w - the wrapper of the function of rio_writen,
 * if it encounters error, just print the error information
 * on the screen
 */
void Rio_writen_w(int fd, void *usrbuf, size_t n) {
	if (rio_writen(fd, usrbuf, n) != n)
		fprintf(stderr, "rio_writen error: %s\n", strerror(errno));
}

/* Rio_readlineb_w - the wrapper of the function of rio_readlineb,
 * if int encounters error, just print the error information
 * on the screen
 */
ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen) {
	ssize_t rc;

	if ((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0) {
		fprintf(stderr, "rio_readline error: %s\n", strerror(errno));
		rc = 0;
	}

	return rc;
}
