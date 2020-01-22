#include <unistd.h>
#include "protocol.h"
#include <arpa/inet.h>
#include "debug.h"
#include <stdlib.h>
#include "csapp.h"

int checkRet(int ret){
    if(ret >= 0){
        return 0;
    } else{
        return -1;
    }
}

//incoming is network byte
int proto_send_packet(int fd, BRS_PACKET_HEADER *hdr, void *payload){


    struct timespec curTime;
    clock_gettime(CLOCK_MONOTONIC, &curTime);
    hdr->timestamp_sec = htonl(curTime.tv_sec);
    hdr->timestamp_nsec = htonl(curTime.tv_nsec);

    //change to network byte order
    int rio_hdr = rio_writen(fd, hdr, sizeof(BRS_PACKET_HEADER));
    // debug("rio_hdr - %d", rio_hdr);
    if(rio_hdr == -1){
        debug("ended in 1");
        // free(hdr);
        // free(payload);
        return -1;
    }

    // debug("here 1");

    //write payload
    if(ntohs(hdr->size) != 0){
        // debug("here 2");
        if(rio_writen(fd, payload, ntohs(hdr->size)) == -1){    //errno set by rio_writen
            debug("ended in 2");
            // free(hdr);
            // free(payload);
            return -1;
        }
    }
    // free(hdr);
    // free(payload);

    // debug("returning proto send packet");
    return 0;
}

int proto_recv_packet(int fd, BRS_PACKET_HEADER *hdr, void **payloadp){
    // debug("------ PROTO RECV PACKET -------------");

    int read = rio_readn(fd, hdr, sizeof(BRS_PACKET_HEADER));
    if(read == -1){
        //set errno set by rio_readn
        debug("somethin wrong");
        return -1;
    }
    else if(read == 0){         //EOF
        return -1;
    }
    // debug("printing values 1 %d\n", ntohs(hdr->size));

    //write payload
    if(ntohs(hdr->size) != 0){

        //IKKADA free at some other place
        debug("Payload pointer before- %p , size- %hi",*payloadp,ntohs(hdr->size));
        *payloadp = malloc(ntohs(hdr->size));
        debug("Payload pointer after- %p",*payloadp);

        int read_status = rio_readn(fd, *payloadp, ntohs(hdr->size));
        if(read_status  == -1){
            //errno set by rio_readn
            debug("somethin wrong");
            return -1;
        }
        // else if(read_status < ntohs(hdr->size)){
        //     debug("shott count");
        //     return -1;
        // }
    }

    // debug("before return");
    return 0;
}




