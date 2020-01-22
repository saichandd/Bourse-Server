#include <stdlib.h>

#include "client_registry.h"
#include "exchange.h"
#include "trader.h"
#include "debug.h"

#include "server.h"

#include "csapp.h"
#include <string.h>
#include <signal.h>

extern EXCHANGE *exchange;
extern CLIENT_REGISTRY *client_registry;

static void terminate(int status);

/*
 * "Bourse" exchange server.
 *
 * Usage: bourse <port>
 */

int *connfdp;

void sigup_handler(int x){
    //clean shutdown
    free(connfdp);
    terminate(0);
}

int main(int argc, char* argv[]){

    //IKKADA - where should i call the signal handler
    struct sigaction act;
    act.sa_handler = sigup_handler;

    sigemptyset(&act.sa_mask);
    act.sa_flags = SA_RESTART;

    sigaction(SIGHUP, &act, NULL);

    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.

    if(argc != 3){
        terminate(EXIT_FAILURE);
    }

    if(strcmp(argv[1], "-p") != 0){
        terminate(EXIT_FAILURE);
    }

    // Perform required initializations of the client_registry,
    // maze, and player modules.
    client_registry = creg_init();
    exchange = exchange_init();
    trader_init();

    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function brs_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.

    int listenfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    listenfd = Open_listenfd(argv[2]);

    while(1){
        clientlen=sizeof(struct sockaddr_storage);
        //IKKADA
        //brs_client_service should free this connfdp
        connfdp = Malloc(sizeof(int));
        *connfdp = Accept(listenfd, (SA *) &clientaddr, &clientlen);

        // if(*connfdp >= 0){
            Pthread_create(&tid, NULL, brs_client_service, connfdp);
        // } else{
            //errnno automatically set
            // terminate(EXIT_FAILURE);
        // }
    }

    // fprintf(stderr, "You have to finish implementing main() "
	   //  "before the Bourse server will function.\n");
    // terminate(EXIT_FAILURE);
    terminate(EXIT_SUCCESS);
}

/*
 * Function called to cleanly shut down the server.
 */
static void terminate(int status) {
    // Shutdown all client connections.
    // This will trigger the eventual termination of service threads.
    creg_shutdown_all(client_registry);

    debug("Waiting for service threads to terminate...");
    creg_wait_for_empty(client_registry);
    debug("All service threads terminated.");

    // Finalize modules.
    creg_fini(client_registry);
    exchange_fini(exchange);
    trader_fini();

    debug("Bourse server terminating");
    exit(status);
}
