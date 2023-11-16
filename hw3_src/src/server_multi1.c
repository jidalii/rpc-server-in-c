/*******************************************************************************
* Multi-Threaded FIFO Server Implementation w/ Queue Limit
*
* Description:
*     A server implementation designed to process client requests in First In,
*     First Out (FIFO) order. The server binds to the specified port number
*     provided as a parameter upon launch. It launches multiple threads to
*     process incoming requests and allows to specify a maximum queue size.
*
* Usage:
*     <build directory>/server -q <queue_size> -w <workers> <port_number>
*
* Parameters:
*     port_number - The port number to bind the server to.
*     queue_size  - The maximum number of queued requests
*     workers     - The number of workers to start to process requests
*
* Author:
*     Renato Mancuso
*
* Affiliation:
*     Boston University
*
* Creation Date:
*     September 29, 2023
*
* Notes:
*     Ensure to have proper permissions and available port before running the
*     server. The server relies on a FIFO mechanism to handle requests, thus
*     guaranteeing the order of processing. If the queue is full at the time a
*     new request is received, the request is rejected with a negative ack.
*
*******************************************************************************/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <sched.h>
#include <signal.h>

/* Needed for wait(...) */
#include <sys/types.h>
#include <sys/wait.h>

/* Needed for semaphores */
#include <semaphore.h>

/* Include struct definitions and other libraries that need to be
 * included by both client and server */
#include "common.h"

#define BACKLOG_COUNT 100
#define USAGE_STRING				\
	"Missing parameter. Exiting.\n"		\
	"Usage: %s -q <queue size> <port_number>\n"

/* 4KB of stack for the worker thread */
#define STACK_SIZE (4096)

/* START - Variables needed to protect the shared queue. DO NOT TOUCH */
sem_t * queue_mutex;
sem_t * queue_notify;
/* END - Variables needed to protect the shared queue. DO NOT TOUCH */

struct request_meta {
	struct request request;
	struct timespec tsc_receipt;
};
typedef struct Node{
	struct request_meta req_meta;
    struct Node* next;
} Node;

struct queue {
	Node* front;
    Node* rear;
	int size;
	int num_req;
};

struct connection_params {
	int queue_size;
    int port_number;
	int num_workers;
};

struct worker_params {
	struct queue * the_queue;
	int conn_socket;
	int worker_done;
};

/* Helper function to perform queue initialization */
void queue_init(struct queue * the_queue, size_t queue_size)
{
	/* IMPLEMENT ME !! */
	
	the_queue->size = queue_size;
	the_queue->front = NULL;
	the_queue->rear = NULL;
	the_queue->num_req = 0;
}

/* Add a new request <request> to the shared queue <the_queue> */
int add_to_queue(struct request_meta to_add, struct queue * the_queue)
{
	int retval = 0;
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_mutex);
	/* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */

	/* WRITE YOUR CODE HERE! */
	/* MAKE SURE NOT TO RETURN WITHOUT GOING THROUGH THE OUTRO CODE! */

	/* Make sure that the queue is not full */
	if(the_queue->num_req >= the_queue->size) {
		/* What to do in case of a full queue */
		sem_post(queue_mutex);
		sem_post(queue_notify);
		return -1;
	} 
	else {
		Node* new_req = (Node*)malloc(sizeof(Node));
		new_req->req_meta = to_add;
		new_req->next = NULL;

		if(the_queue->num_req == 0){
			the_queue->front = new_req;
			the_queue->rear = new_req;
		} 
		else {
			/* If all good, add the item in the queue */
			the_queue->rear->next = new_req;
			the_queue->rear = new_req;
		}
		the_queue->num_req+=1;
		/* QUEUE SIGNALING FOR CONSUMER --- DO NOT TOUCH */
		sem_post(queue_notify);
	}
	/* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */
	sem_post(queue_mutex);
	/* QUEUE PROTECTION OUTRO END --- DO NOT TOUCH */
	return retval;
}

/* Add a new request <request> to the shared queue <the_queue> */
struct request_meta *  get_from_queue(struct queue * the_queue)
{
	struct request_meta * retval;
	retval = (struct request_meta *)malloc(sizeof(struct request_meta));
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_notify);
	sem_wait(queue_mutex);
	/* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */

	/* WRITE YOUR CODE HERE! */
	/* MAKE SURE NOT TO RETURN WITHOUT GOING THROUGH THE OUTRO CODE! */
	// 1. if queue is empty, return NULL
	if(the_queue->num_req == 0) {
		sem_post(queue_mutex);
		return NULL; // not sure
	}
	// 2. find the first request in queue
	*retval = the_queue->front->req_meta;

	// 3. update the queue front
	the_queue->front = the_queue->front->next;

	// 4. update the num_req
	the_queue->num_req--;

	/* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */
	sem_post(queue_mutex);
	/* QUEUE PROTECTION OUTRO END --- DO NOT TOUCH */
	return retval;
}

void dump_queue_status(struct queue * the_queue)
{
	/* QUEUE PROTECTION INTRO START --- DO NOT TOUCH */
	sem_wait(queue_mutex);
	/* QUEUE PROTECTION INTRO END --- DO NOT TOUCH */

	/* WRITE YOUR CODE HERE! */
	/* MAKE SURE NOT TO RETURN WITHOUT GOING THROUGH THE OUTRO CODE! */
	printf("Q:[");
	Node* trav = the_queue->front;
	while(trav != NULL) {
		printf("R%ld", trav->req_meta.request.req_id);
		if(trav->next != NULL) {
			printf(",");
		}
		trav = trav->next;
	}
	printf("]\n");

	/* QUEUE PROTECTION OUTRO START --- DO NOT TOUCH */
	sem_post(queue_mutex);
	/* QUEUE PROTECTION OUTRO END --- DO NOT TOUCH */
}

/* Main logic of the worker thread */
int worker_main (void * arg)
{
	struct timespec now;
	struct worker_params * params = (struct worker_params *)arg;
	int conn_socket;

	struct queue * the_queue;
	struct request_meta * req_meta;
	struct request request;
	ssize_t sent;
	struct response response;

	struct timespec tsc_start, tsc_receipt, tsc_completion;

	the_queue = params->the_queue;
	conn_socket = params->conn_socket;

	/* Print the first alive message. */
	clock_gettime(CLOCK_MONOTONIC, &now);
	printf("[#WORKER#] %lf Worker Thread Alive!\n", TSPEC_TO_DOUBLE(now));

	/* Okay, now execute the main logic. */
	while (!params->worker_done) {

		/* IMPLEMENT ME !! Main worker logic. */
		req_meta = get_from_queue(the_queue);
		
		request = req_meta->request;

		clock_gettime(CLOCK_MONOTONIC, &tsc_start);

		// get_elapsed_busywait(current_req_w->req.req_length.tv_sec, current_req_w->req.req_length.tv_nsec);
		get_elapsed_busywait(req_meta->request.req_length.tv_sec, req_meta->request.req_length.tv_nsec);

		clock_gettime(CLOCK_MONOTONIC, &tsc_completion);
		
		tsc_receipt = req_meta->tsc_receipt;
		

		printf("R%lu:%f,%f,%f,%f,%f\n",
               request.req_id,
               TSPEC_TO_DOUBLE(request.req_timestamp),
               TSPEC_TO_DOUBLE(request.req_length),
			   TSPEC_TO_DOUBLE(tsc_receipt),
               TSPEC_TO_DOUBLE(tsc_start),
               TSPEC_TO_DOUBLE(tsc_completion)
			   );

		dump_queue_status(params->the_queue);
		sent = send(conn_socket, &response, sizeof(response), 0);
		if (sent <= 0) break;
	}

	return EXIT_SUCCESS;
}

/* This function will start the worker thread wrapping around the
 * clone() system call*/
int start_worker(void * params, void * worker_stack)
{
	/* IMPLEMENT ME !! */
	int result = clone(worker_main, worker_stack+STACK_SIZE, CLONE_THREAD | CLONE_VM | CLONE_SIGHAND | CLONE_FS | CLONE_FILES | CLONE_SYSVSEM, params);
	return result;
}

/* Main function to handle connection with the client. This function
 * takes in input conn_socket and returns only when the connection
 * with the client is interrupted. */
void handle_connection(int conn_socket, struct connection_params conn_params)
{
	struct request * request;
	request = (struct request *)malloc(sizeof(struct request));
	struct request_meta * req_meta;
	req_meta = (struct request_meta *)malloc(sizeof(struct request_meta));
	struct queue * the_queue;
	// the_queue = (struct queue *)malloc(sizeof(struct queue));
	size_t in_bytes;

	/* The connection with the client is alive here. Let's get
	 * ready to start the worker thread. */
	void * worker_stack = malloc(STACK_SIZE);
	struct worker_params * worker_params;
	worker_params = (struct worker_params *)malloc(sizeof(struct worker_params));
	
	struct timespec tsc_receipt, tsc_reject;
	int worker_id, res;

	/* Now handle queue allocation and initialization */
	/* IMPLEMENT ME !!*/
	the_queue = (struct queue *)malloc(sizeof(struct queue));
	queue_init(the_queue, conn_params.queue_size);

	/* IMPLEMENT ME!! Write a loop to start and initialize all the
	 * worker threads ! */
	worker_params->conn_socket = conn_socket;
	worker_params->the_queue = the_queue;
	worker_params->worker_done = 0;

	int num = 0;
	while(num <conn_params.num_workers){
		worker_id = start_worker(worker_params, worker_stack);
		if (worker_id < 0) {
			/* HANDLE WORKER CREATION ERROR */
			perror("clone failed");
			free(worker_stack); // Free the allocated memory
			free(req_meta);
			free(request);
			free(worker_params);
			free(the_queue);
			close(conn_socket);
			return;
		}
		num++;
	}

	/* We are ready to proceed with the rest of the request
	 * handling logic. */
	do {
		/* IMPLEMENT ME: Receive next request from socket. */
		in_bytes = recv(conn_socket, request, sizeof(struct request), 0);
		clock_gettime(CLOCK_MONOTONIC, &tsc_receipt);

		/* Don't just return if in_bytes is 0 or -1. Instead
		 * skip the response and break out of the loop in an
		 * orderly fashion so that we can de-allocate the req
		 * and resp varaibles, and shutdown the socket. */

		/* IMPLEMENT ME: Attempt to enqueue or reject request! */
		if (in_bytes > 0) {
			req_meta->tsc_receipt = tsc_receipt;
			req_meta->request = *request;
			res = add_to_queue(*req_meta, the_queue);
			clock_gettime(CLOCK_MONOTONIC, &tsc_reject);
			if(res == -1) {
				printf("X%lu:%f,%f,%f", 
				request->req_id, 
				TSPEC_TO_DOUBLE(request->req_timestamp),
				TSPEC_TO_DOUBLE(request->req_length), 
				TSPEC_TO_DOUBLE(tsc_reject));
				dump_queue_status(the_queue);
			}
		}
	} while (in_bytes > 0);

	/* IMPLEMENT ME!! Write a loop to gracefully terminate all the
	 * worker threads ! */

	free(the_queue);

	free(req_meta);
	shutdown(conn_socket, SHUT_RDWR);
	close(conn_socket);
	printf("INFO: Client disconnected.\n");
}


/* Template implementation of the main function for the FIFO
 * server. The server must accept in input a command line parameter
 * with the <port number> to bind the server to. */
int main (int argc, char ** argv) {
	int sockfd, retval, accepted, optval, opt;
	in_port_t socket_port;
	struct sockaddr_in addr, client;
	struct in_addr any_address;
	socklen_t client_len;

	struct connection_params conn_params;

	/* Parse all the command line arguments */
	/* IMPLEMENT ME!! */
	/* PARSE THE COMMANDS LINE: */
	int option;
    while ((option = getopt(argc, argv, "q:w:")) != -1) {
        switch (option) {
			/* 1. Detect the -q parameter and set aside the queue size in conn_params */
            case 'q':
                // Parse and store the queue size
                conn_params.queue_size = atoi(optarg);
                break;
			/* 2. Detect the -w parameter and set aside the number of threads to launch */
            case 'w':
                // Parse and store the number of worker threads
                conn_params.num_workers = atoi(optarg);
                break;
            default:
                fprintf(stderr, "Usage: %s -q <queue_size> -w <workers> <port_number>\n", argv[0]);
                return EXIT_FAILURE;
        }
    }
	/* 3. Detect the port number to bind the server socket to (see HW1 and HW2) */
    if (optind >= argc) {
        fprintf(stderr, "Expected port number after options\n");
        return EXIT_FAILURE;
    }
	conn_params.port_number = atoi(argv[optind]);
    socket_port = conn_params.port_number;

    printf("Queue size: %d\n", conn_params.queue_size);
    printf("Number of workers: %d\n", conn_params.num_workers);
    printf("Port number: %d\n", socket_port);


	/* Now onward to create the right type of socket */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0) {
		ERROR_INFO();
		perror("Unable to create socket");
		return EXIT_FAILURE;
	}

	/* Before moving forward, set socket to reuse address */
	optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&optval, sizeof(optval));

	/* Convert INADDR_ANY into network byte order */
	any_address.s_addr = htonl(INADDR_ANY);

	/* Time to bind the socket to the right port  */
	addr.sin_family = AF_INET;
	addr.sin_port = htons(socket_port);
	addr.sin_addr = any_address;

	/* Attempt to bind the socket with the given parameters */
	retval = bind(sockfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));

	if (retval < 0) {
		ERROR_INFO();
		perror("Unable to bind socket");
		return EXIT_FAILURE;
	}

	/* Let us now proceed to set the server to listen on the selected port */
	retval = listen(sockfd, BACKLOG_COUNT);

	if (retval < 0) {
		ERROR_INFO();
		perror("Unable to listen on socket");
		return EXIT_FAILURE;
	}

	/* Ready to accept connections! */
	printf("INFO: Waiting for incoming connection...\n");
	client_len = sizeof(struct sockaddr_in);
	accepted = accept(sockfd, (struct sockaddr *)&client, &client_len);

	if (accepted == -1) {
		ERROR_INFO();
		perror("Unable to accept connections");
		return EXIT_FAILURE;
	}

	/* Initialize queue protection variables. DO NOT TOUCH. */
	queue_mutex = (sem_t *)malloc(sizeof(sem_t));
	queue_notify = (sem_t *)malloc(sizeof(sem_t));
	retval = sem_init(queue_mutex, 0, 1);
	if (retval < 0) {
		ERROR_INFO();
		perror("Unable to initialize queue mutex");
		return EXIT_FAILURE;
	}
	retval = sem_init(queue_notify, 0, 0);
	if (retval < 0) {
		ERROR_INFO();
		perror("Unable to initialize queue notify");
		return EXIT_FAILURE;
	}
	/* DONE - Initialize queue protection variables */

	/* Ready to handle the new connection with the client. */
	handle_connection(accepted, conn_params);

	free(queue_mutex);
	free(queue_notify);

	close(sockfd);
	return EXIT_SUCCESS;

}
