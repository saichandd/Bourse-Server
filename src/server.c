#include "csapp.h"
#include "server.h"
#include "client_registry.h"
#include "protocol.h"
#include "debug.h"
#include "trader.h"
#include "exchange.h"

void alreadyRegistred(int fd){

    BRS_PACKET_HEADER hdr;
    hdr.type = BRS_NACK_PKT;
    hdr.size = htons(0);
    proto_send_packet(fd, &hdr, NULL);
}

void *brs_client_service(void *arg){
    // debug("---------BRS CLIENT SERVICE------------");
    int connfd = *((int *)arg);
    Pthread_detach(pthread_self());
    free(arg);

    if(creg_register(client_registry, connfd) == -1){
        //error
        debug("couldn't register");
        return NULL;
    }
    void *payload;

    //IKKADA malloc this in trader
    TRADER *this_trader = NULL;

    while(1){
        //SUCCESS
        BRS_PACKET_HEADER *hdr = malloc(sizeof(BRS_PACKET_HEADER));
        memset(hdr,0,sizeof(BRS_PACKET_HEADER));

        //Retrive data from client
        if(proto_recv_packet(connfd, hdr, &payload) == -1){ //EOF or error
            debug("EOF recerived || some other error on reciving packet");
            if(this_trader == NULL){
                // debug("trader is null");

                if (ntohs(hdr->size)>0){
                    free(payload);
                }
                free(hdr);
                break;
            }
            // debug("trader is null");
            if (ntohs(hdr->size)>0){
                free(payload);
            }
            free(hdr);
            trader_logout(this_trader);
            break;
        }

        if(hdr->type == 0){
            //I think this never happens
            debug("No type");
        }
        else if(hdr->type == BRS_LOGIN_PKT){
            debug("---BRS_LOGIN_PKT----");

            //if trying to login again
            if(this_trader != NULL){            //already logged in
                debug("Already logged in");
                trader_send_nack(this_trader);
                continue;
            }

            //Null terminate the payload - gets freed in removeTraderIfEmpty()
            char *payload_nullterminate = malloc(ntohs(hdr->size)+1);
            memcpy(payload_nullterminate, payload, ntohs(hdr->size));
            *(payload_nullterminate + ntohs(hdr->size)) = '\0';

            this_trader = trader_login(connfd, payload_nullterminate);

            //unsuccessful login
            if(this_trader == NULL){
                debug("---unsuccessful login----");
                alreadyRegistred(connfd);
            }
            else{
                debug("-------logged in---------");
                trader_send_ack(this_trader, NULL);
            }
        }
        else{
            // debug("----client transactions happening----");
            if(this_trader == NULL){        // check if logged in
                debug("Login required");
                continue;
            }
            if(hdr->type == BRS_STATUS_PKT){
                // BRS_STATUS_INFO infop;
                BRS_STATUS_INFO infop;
                exchange_get_status(exchange, &infop);
                trader_send_ack(this_trader, &infop);
            }
            else if(hdr->type == BRS_DEPOSIT_PKT){
                BRS_FUNDS_INFO *info_f = (BRS_FUNDS_INFO*)payload;
                trader_increase_balance(this_trader, ntohl(info_f->amount));

                BRS_STATUS_INFO info_s;
                exchange_get_status(exchange, &info_s);
                trader_send_ack(this_trader, &info_s);
            }
            else if(hdr->type == BRS_WITHDRAW_PKT){
                BRS_FUNDS_INFO *info_f = (BRS_FUNDS_INFO*)payload;
                int what_happened = trader_decrease_balance(this_trader, ntohl(info_f->amount));

                BRS_STATUS_INFO info_s;
                exchange_get_status(exchange, &info_s);

                if(what_happened == 0){
                    trader_send_ack(this_trader, &info_s);
                }else{
                    trader_send_nack(this_trader);
                }
            }
            else if(hdr->type == BRS_ESCROW_PKT){
                BRS_ESCROW_INFO *info_e = (BRS_ESCROW_INFO*)payload;
                trader_increase_inventory(this_trader, ntohl(info_e->quantity));

                BRS_STATUS_INFO info_s;
                exchange_get_status(exchange, &info_s);
                trader_send_ack(this_trader, &info_s);
            }
            else if(hdr->type == BRS_RELEASE_PKT){
                BRS_ESCROW_INFO *info_e = (BRS_ESCROW_INFO*)payload;
                int what_happened = trader_decrease_inventory(this_trader, ntohl(info_e->quantity));

                BRS_STATUS_INFO info_s;
                exchange_get_status(exchange, &info_s);

                if(what_happened == 0){
                    trader_send_ack(this_trader, &info_s);
                }else{
                    trader_send_nack(this_trader);
                }
            }
            else if(hdr->type == BRS_BUY_PKT){
                // debug("buy1");
                BRS_ORDER_INFO *info_o = (BRS_ORDER_INFO*)payload;
                quantity_t quantity = ntohl(info_o->quantity);
                funds_t price = ntohl(info_o->price);
                //should i convert here or no
                orderid_t order_id = exchange_post_buy(exchange, this_trader, quantity,price);

                // debug("buy2");
                BRS_STATUS_INFO info_s;
                //why this?
                info_s.orderid = order_id;
                // debug("buy3");
                exchange_get_status(exchange, &info_s);

                debug("orderid = %d", order_id);
                if(order_id == 0){      //if fail
                    // debug("buy3");
                    trader_send_nack(this_trader);
                }else{
                    // debug("buy4");
                    trader_send_ack(this_trader, &info_s);
                }
            }
            else if(hdr->type == BRS_SELL_PKT){
                BRS_ORDER_INFO *info_o = (BRS_ORDER_INFO*)payload;
                orderid_t order_id = exchange_post_sell(exchange, this_trader, ntohl(info_o->quantity), ntohl(info_o->price));

                 BRS_STATUS_INFO info_s;
                //why this?
                info_s.orderid = order_id;
                exchange_get_status(exchange, &info_s);

                if(order_id == 0){      //if fail
                    trader_send_nack(this_trader);
                }else{
                    trader_send_ack(this_trader, &info_s);
                }
            }
            else if(hdr->type == BRS_CANCEL_PKT){
                BRS_CANCEL_INFO *info_o = (BRS_CANCEL_INFO*)payload;

                quantity_t quantity;
                int what_happened = exchange_cancel(exchange, this_trader, ntohl(info_o->order), &quantity);

                BRS_STATUS_INFO info_s;
                info_s.quantity = quantity;
                exchange_get_status(exchange, &info_s);

                if(what_happened == 0){      //if fail
                    trader_send_ack(this_trader, &info_s);
                }else{
                    trader_send_nack(this_trader);
                }
            }
        }
        // free(payload);
        free(hdr);
    }


    if(creg_unregister(client_registry, connfd) == -1){
        debug("couldn't logout");
        return NULL;
    }

    debug("last but 1");

    Close(connfd);
    debug("last but 1");
    return NULL;
}




