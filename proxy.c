#include "proxy_parse.h"
#include "proxy_parse.c"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>    
#include <pthread.h>   
#include <assert.h>
#include <time.h>
#include <netdb.h>
#include <sys/mman.h>
#include <sys/wait.h>

static int *count;

int main(int argc, char * argv[]) 
{
	int act_server_socket, actual_client_socket, act_clinet_socket;  

	socklen_t addrlen;        

	char * buf;
	buf = (char *)malloc(4096*sizeof(char));
	char head[1024];    
	struct sockaddr_in client_address, server_address;
	struct hostent *name;

	char request[1024];    

	if ((act_server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)     //Creating the server socket which will listen for client requests
	{    
	  printf("The socket was not created\n");
	  exit(1);
	}

	printf("Socket Established \n");

	client_address.sin_family = AF_INET;    
	client_address.sin_addr.s_addr = INADDR_ANY;    
	client_address.sin_port = htons(atoi(argv[1]));   //Setting up the port on which the server will be listening    

	if (bind(act_server_socket, (struct sockaddr *) &client_address, sizeof(client_address)) == 0)     //Binding the server sockets to the adress that was defined above
	{    
	  printf("Binding Socket\n");
	}

	if (listen(act_server_socket, 10) < 0)         //Make the server to start listening to the requests of the clients 
	{    
	  perror("server: listen");    
	  exit(1);    
	}    

	count = mmap(NULL, sizeof *count, PROT_READ | PROT_WRITE, 
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	*count = 0;
	
	while(1)
	{
		if((actual_client_socket = accept(act_server_socket, (struct sockaddr *) &client_address, &addrlen)) < 0)
		{
			perror("Could not create new socket");
			break;
		}
		pid_t pid;

		if(*count <20)
		{	
			pid = fork();
		}
		else
		{
			wait(NULL);
			pid = fork();
		}
		if(pid !=0)
		{
			close(actual_client_socket);
			continue;
		}

		*count = *count + 1;

		char  tmp_buf[1024];
		bzero((char *)buf, sizeof(buf));
		while(1)
		{	
			bzero((char *)tmp_buf, sizeof(tmp_buf));
			if(recv(actual_client_socket, tmp_buf, 1024, 0)<= 0 )
			{
				printf("No message is received form the client");
				*count = *count -1;
				exit(1);
			}
			if(strlen(buf) + strlen(tmp_buf) > 4096)
			{
				buf = (char* )realloc(buf, 8192);
			}
			strcat(buf, tmp_buf);
			if(strstr(tmp_buf, "\r\n\r\n") != NULL)
				break;
		}

		printf("I have reached this point\n");
		
		printf("\nThe buffer containing the url:\n%s\n\n", buf);

		struct ParsedRequest *pr;
		pr = ParsedRequest_create();       //Here we are just making an struct
		
		int ret = ParsedRequest_parse(pr, buf, strlen(buf));

		if(ret != 0)
		{	
			char resp[1024];
			bzero((char*) resp, sizeof(resp));
			printf("Something went wrong while parsing\n");
			  strcat(resp, "HTTP/1.1 500 Internal Server Error\r\n");   
		      strcat(resp, "Content-Length: 155");
		      strcat(resp, "\r\n");
		      strcat(resp, "Content-Type: text/html");
		      strcat(resp, "\r\n");
		      strcat(resp, "Connection: close");
		      strcat(resp, "\r\n");
		      strcat(resp, "\r\n");
		      strcat(resp, "<html><head><TITLE>500 Internal Server Error</TITLE></head> \r\n<body><H1>500 Internal Server Error</H1>\r\nThere is some error on this server.\r\n</body></html>");

			write(actual_client_socket,resp,strlen(resp));
			*count = *count - 1;
			exit(1);
		}	    

	    bzero((char *)request, sizeof(request));

		strcat(request, pr->method);
		strcat(request, " ");
		strcat(request, pr->path);
		strcat(request, " ");
		strcat(request, pr->version);
		strcat(request, "\r\n");
		strcat(request, "Host: ");
		strcat(request, pr->host);
		strcat(request, "\r\n");
		strcat(request, "Connection: close\r\n");
		
		if(pr->headers[0].key == NULL)
		{
			printf("Well it just is that i dont have header");
		}

		else
		{	printf("Well what do you know i have headers\n");
			int list = 0;
			while(1)
			{
				if(pr->headers[list].key == NULL)
					break;
				if((strcmp(pr->headers[list].key, "Connection")!=0))
				{
					strcat(request, pr->headers[list].key);
					strcat(request, ": ");
					strcat(request, pr->headers[list].value );
					strcat(request, "\r\n");
				}
				list = list + 1;
			}
		}
		strcat(request, "\r\n");
		printf("The complete reques header has been made\n");
		printf("%s\n", request);

		act_clinet_socket=socket(AF_INET,SOCK_STREAM,0);
		server_address.sin_family = AF_INET;
 	    server_address.sin_port = htons(80);

 	    name = gethostbyname(pr->host);

 	    bcopy((char *)name->h_addr, (char *)&server_address.sin_addr.s_addr, name->h_length);


		 if(connect(act_clinet_socket,(struct sockaddr*)&server_address,sizeof(struct sockaddr)))
		{
			printf("Error : Connection Failed");
			*count = *count -1;
			exit(1);
		}
		int b;
		int a;
		int c;
		char buff[4096];

		
		c = write(act_clinet_socket,request,1024);
		while(1)
		{			
			bzero((char *)buff, sizeof(buff));
			b = recv(act_clinet_socket, buff, 4096, 0);
				
			if( b<=0)
			{	
				close(act_clinet_socket);
				break;
			}
			a = write(actual_client_socket,buff,b);
		}


		printf("I have finished the whole thing\n");		

		close(actual_client_socket);
		*count = *count -1;
		exit(0);
	} 
	return 0;
}
