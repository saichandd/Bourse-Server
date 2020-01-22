#include "csapp.h"
#include "client_registry.h"
#include "debug.h"
#include "trader.h"

typedef struct client_registry {
    int* fds;
    int n;          //MAX CLIENTS

    int count;
    sem_t count_mutex;
    sem_t mutex;
    sem_t items;
    sem_t slots;
} CLIENT_REGISTRY;


CLIENT_REGISTRY *creg_init(){
    debug("-----------CREG_INIT---------");

    CLIENT_REGISTRY* cr = malloc(sizeof(CLIENT_REGISTRY));
    cr->n = FD_SETSIZE;
    cr->fds = (int*) malloc(cr->n*sizeof(int));
    cr->count = 0;

    //count_muted help proper termination
    Sem_init(&cr->count_mutex, 0, 1);

    Sem_init(&cr->mutex, 0, 1);
    Sem_init(&cr->slots, 0, cr->n);
    Sem_init(&cr->items, 0, 0);

    for(int i=0; i < cr->n; i++){
        cr->fds[i] = -1;
    }

    return cr;
}


int searchForEmptySlot(CLIENT_REGISTRY *cr){
    for(int i=0; i < cr->n; i++){
        if(cr->fds[i] == -1){
            return i;
        }
    }
    return -1;
}

void creg_fini(CLIENT_REGISTRY *cr){
    free(cr->fds);
    free(cr);
}

//IKKADA - check for errors
int creg_register(CLIENT_REGISTRY *cr, int fd){
    debug("-------------CREG_REGISTERY--------------");
    P(&cr->slots); //wait for slot
    P(&cr->mutex);  //lock buffer

    int index = searchForEmptySlot(cr);
    if(index == -1){
        return -1;
    }
    cr->fds[index] = fd;

    //wait helper
    cr->count++;
    if(cr->count == 1)
        P(&cr->count_mutex);


    V(&cr->mutex);  //unlock buffer
    V(&cr->items);  //announce item got filled
    return 0;
}

//IKKADA - check for errors
int creg_unregister(CLIENT_REGISTRY *cr, int fd){
    debug("-------------CREG_UNREGISTER--------------");
    P(&cr->items);  //wait if item if there or not
    P(&cr->mutex);  //lock buffer

    for(int i = 0; i < cr->n; i++){
        if(cr->fds[i] ==  fd){
            cr->fds[i] = -1;

            //wait helper
            cr->count--;
            if(cr->count == 0){
                V(&cr->count_mutex);
            }

            V(&cr->mutex);  //unlock buffer
            V(&cr->slots);  //announce slot available
            return 0;
        }
    }

    V(&cr->mutex);  //unlock buffer
    V(&cr->slots);  //announce slot available
    return -1;
}


//IKKADA - not good implementation?
void creg_wait_for_empty(CLIENT_REGISTRY *cr){
    debug("---------------WAIT FOR EMPTY------------------");

    //stops because count_mutex is 0
    P(&cr->count_mutex);
}

void creg_shutdown_all(CLIENT_REGISTRY *cr){
    debug("--------------SHUTDOWN ALL-----------------");
    P(&cr->mutex);
    for(int i=0; i < cr->n; i++){
        shutdown(cr->fds[i], SHUT_RD);
    }
    V(&cr->mutex);
}
