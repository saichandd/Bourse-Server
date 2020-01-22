#include "trader.h"
#include <stdlib.h>
#include "csapp.h"
#include <unistd.h>
#include <sys/resource.h>
#include <pthread.h>
#include "debug.h"
#include <string.h>
#include <protocol.h>

TRADER *all_traders[MAX_TRADERS];

int count = 0;      //number of traders in(logggedin and loggedout)

//semaphore
typedef struct trader{
    int ref;
    int fd;
    funds_t balance;
    quantity_t inventory;
    pthread_mutex_t trader_mutex;
    char * name;
} TRADER;

int trader_init(void){
    //
    for(int i = 0; i < MAX_TRADERS; i++){
        all_traders[i] = NULL;
    }
    return 0;
}

void trader_fini(void){

    //free all traders
    // pthread_mutex_lock(&trader_mutex->lock);
    for (int i = 0; i<count; i++){
        if(all_traders[i] != NULL){
            pthread_mutex_lock(&all_traders[i]->trader_mutex);
            free(all_traders[i]->name);
            free(all_traders[i]);
            pthread_mutex_unlock(&all_traders[i]->trader_mutex);
        }

    }
    // pthread_mutex_unlock(&prev->lock);
}


TRADER* checkOldTraderAndLogin(int fd, char *name){

    for(int i = 0; i < MAX_TRADERS; i++){

        if(all_traders[i] != NULL && strcmp(all_traders[i]->name, name)==0){
            debug("---name1 = %d | %s | name2 = %s",i,
            all_traders[i]->name, name);
            trader_ref(all_traders[i], "re login");
            return all_traders[i];
        }
    }
    return NULL;
}

int getFirstAvailableIndex(){
    for(int i= 0; i< MAX_TRADERS;i++){
        if(all_traders[i] == NULL){
            return i;
        }
    }
    return -1;
}


TRADER *trader_login(int fd, char *name){
    //check if trader already exists?
    TRADER *oldTrader = checkOldTraderAndLogin(fd, name);

    if(oldTrader!= NULL){   //EXISTS
        //if oldtrader session exists
        if(oldTrader->fd != -1){
            return NULL;
        }

        oldTrader->fd = fd;
        debug("---returning old trader---");
        return oldTrader;
    }

    if(count >= MAX_TRADERS){
        debug("number of traders = %d", count);
        return NULL;
    }

    //Increase total traders
    count++;

    TRADER *trader = malloc(sizeof(TRADER));
    // trader->ref = 2;
    trader_ref(trader,"intilizing trader 1");
    trader_ref(trader,"intilizing trader 2");
    trader->fd = fd;
    trader->balance = 0;
    trader->inventory = 0;
    trader->name = name;

    pthread_mutexattr_t Attr;

    pthread_mutexattr_init(&Attr);
    pthread_mutexattr_settype(&Attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&trader->trader_mutex, &Attr);


    int ret = getFirstAvailableIndex();
    if(ret == -1){
        debug("Taders List Full");
        return NULL;
    }

    //add trader to array
    all_traders[ret] = trader;

    debug("trader logged in");
    return trader;
}


void trader_logout(TRADER *trader){

    pthread_mutex_lock(&trader->trader_mutex);
    for(int i = 0; i< MAX_TRADERS; i++){
        debug("x");
        if(all_traders[i] != NULL && strcmp(all_traders[i]->name, trader->name)==0){
            trader_unref(trader, "trader logged out");  //change ref to 1
            trader->fd = -1;
            break;
        }
    }
    pthread_mutex_unlock(&trader->trader_mutex);
}

TRADER *trader_ref(TRADER *trader, char *why){

    pthread_mutex_lock(&trader->trader_mutex);
    debug("increasing reference: %s", why);
    trader->ref++;
    pthread_mutex_unlock(&trader->trader_mutex);

    return trader;
}

void removeTraderIfEmpty(TRADER *trader){

    if(trader->ref != 0)
        return;

    for(int i=0; i< MAX_TRADERS; i++){
        if(all_traders[i] != NULL && strcmp(all_traders[i]->name, trader->name) == 0){
            free(trader->name);
            free(trader);
            all_traders[i] = NULL;
            return;
        }
    }
}

void trader_unref(TRADER *trader, char *why){
    pthread_mutex_lock(&trader->trader_mutex);

    debug("decreasing reference: %s", why);
    trader->ref--;

    removeTraderIfEmpty(trader);

    pthread_mutex_unlock(&trader->trader_mutex);
}

int trader_send_packet(TRADER *trader, BRS_PACKET_HEADER *pkt, void *data){
    pthread_mutex_lock(&trader->trader_mutex);

    if(proto_send_packet(trader->fd, pkt, data) == -1){
        debug("couldn't broadcast packet");
        return -1;
    }

    pthread_mutex_unlock(&trader->trader_mutex);
    return 0;
}

int trader_broadcast_packet(BRS_PACKET_HEADER *pkt, void *data){

    for(int i = 0; i < MAX_TRADERS; i++){
        if(all_traders[i] != NULL){
            trader_send_packet(all_traders[i], pkt, data);
        }
    }

    return 0;
}

int trader_send_ack(TRADER *trader, BRS_STATUS_INFO *info){
    debug("====trader_send_ack====");
    BRS_PACKET_HEADER hdr;
    hdr.type = BRS_ACK_PKT;


    if(info == NULL){
        hdr.size = htons(0);
        trader_send_packet(trader, &hdr, NULL);
    }else{
        hdr.size = htons(sizeof(BRS_STATUS_INFO));
        info->balance = htonl(trader->balance);
        info->inventory = htonl(trader->inventory);

        trader_send_packet(trader, &hdr, info);
    }

    return 0;
}

int trader_send_nack(TRADER *trader){
    debug("====trader_send_nack====");
    BRS_PACKET_HEADER hdr;
    hdr.type = BRS_NACK_PKT;
    hdr.size = 0;

    trader_send_packet(trader, &hdr, NULL);
    return 0;
}

void trader_increase_balance(TRADER *trader, funds_t amount){
    pthread_mutex_lock(&trader->trader_mutex);
    trader->balance += amount;
    pthread_mutex_unlock(&trader->trader_mutex);
}

int trader_decrease_balance(TRADER *trader, funds_t amount){
    pthread_mutex_lock(&trader->trader_mutex);

    if(trader->balance >= amount){
        trader->balance -= amount;
        pthread_mutex_unlock(&trader->trader_mutex);
        return 0;
    } else{
        debug("No sufficient funds");
        pthread_mutex_unlock(&trader->trader_mutex);
        return -1;
    }

}

void trader_increase_inventory(TRADER *trader, quantity_t quantity){
    pthread_mutex_lock(&trader->trader_mutex);
    trader->inventory += quantity;
    pthread_mutex_unlock(&trader->trader_mutex);
}

int trader_decrease_inventory(TRADER *trader, quantity_t quantity){

    pthread_mutex_lock(&trader->trader_mutex);

    if(trader->inventory >= quantity){
        trader->inventory -= quantity;
        pthread_mutex_unlock(&trader->trader_mutex);
        return 0;
    } else{
        debug("No sufficient funds");
        pthread_mutex_unlock(&trader->trader_mutex);
        return -1;
    }
}