/*
 * Name: Emma Mirica
 * Project: TWAMP Protocol
 * Class: OSS
 * Email: emma.mirica@cti.pub.ro
 *
 * Source: timestamp.c
 * Note: contains helpful functions to get the timestamp
 * in the required TWAMP format.
 *
 */
#include "twamp.h"
#include <inttypes.h>
#include <sys/time.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <stdio.h>

//global var
float jitter = 0.0;
float jitter_up = 0.0;
float jitter_dw = 0.0;
uint16_t number_of_packets = 0;
uint16_t prev_difference = 0;
uint16_t prev_difference_up = 0;
uint16_t prev_difference_dw = 0;
uint64_t total_time_diiference = 0;

void timeval_to_timestamp(const struct timeval *tv, TWAMPTimestamp *ts)
{
    if (!tv || !ts)
        return;

    /* Unix time to NTP */
    ts->integer = tv->tv_sec + 2208988800uL;
    ts->fractional = (uint32_t) ( (double)tv->tv_usec * ( (double)(1uLL<<32)\
                                / (double)1e6) );

    ts->integer = htonl(ts->integer);
    ts->fractional = htonl(ts->fractional);
}

void timestamp_to_timeval(const TWAMPTimestamp *ts, struct timeval *tv)
{
    if (!tv || !ts)
        return;
    
    TWAMPTimestamp ts_host_ord;

    ts_host_ord.integer = ntohl(ts->integer);
    ts_host_ord.fractional = ntohl(ts->fractional);

    /* NTP to Unix time */
    tv->tv_sec = ts_host_ord.integer - 2208988800uL;
    tv->tv_usec = (uint32_t) (double)ts_host_ord.fractional * (double)1e6\
                             / (double)(1uLL<<32);
}

TWAMPTimestamp get_timestamp()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    TWAMPTimestamp ts;
    timeval_to_timestamp(&tv, &ts);
    return ts;
}

static uint64_t get_usec(const TWAMPTimestamp *ts)
{
    struct timeval tv;
    timestamp_to_timeval(ts, &tv);

    return tv.tv_sec * 1000000 + tv.tv_usec;
}

int get_actual_shutdown(const struct timeval *tv, const struct timeval *ts, const TWAMPTimestamp *t)
{
    /* If ts is 0 then no StopSessions message was received */
    if ((ts->tv_sec * 1000000 + ts->tv_usec) == 0)
        return 1;
    /* Else compute time difference */
    uint64_t current = tv->tv_sec * 1000000 + tv->tv_usec;
    uint64_t shutdown = ts->tv_sec * 1000000 + ts->tv_usec;
    uint64_t timeout = get_usec(t);

    /* This should be ok, as no difference is computed */
    if (current > shutdown + timeout)
        return 1;
    return 0;
}

/* This will check if the time difference is negative.
 * Most likely the time isn't synchronized on client and server */
static uint64_t get_time_difference(const TWAMPTimestamp *tv, const TWAMPTimestamp *ts)
{
    uint64_t tv_usec = get_usec(tv);
    uint64_t ts_usec = get_usec(ts);

    if (tv_usec < ts_usec) {
        fprintf(stderr, "Clocks aren't synchronized\n");
        return 0;
    }

    return tv_usec - ts_usec;
}

static float get_jitter(uint64_t time_difference){
    
    // This calculation based on the RFC3550 page 40
    // https://tools.ietf.org/rfcmarkup?rfc=3550&draft=&url=#section-6.4.1
    // D(i,j) = (Rj - Ri) - (Sj - Si) = (Rj - Sj) - (Ri - Si)
    // J(i) = J(i-1) + (|D(i-1,i)| - J(i-1))/16

    int diff = time_difference - prev_difference;
    if (diff < 0){
        diff = -1 * diff;
    }
    jitter = jitter + ((diff - jitter)/16);
    prev_difference = time_difference;
    return jitter;
}

static float get_jitter_up(uint64_t time_difference){

    // This calculation based on the RFC3550 page 40
    // https://tools.ietf.org/rfcmarkup?rfc=3550&draft=&url=#section-6.4.1
    // D(i,j) = (Rj - Ri) - (Sj - Si) = (Rj - Sj) - (Ri - Si)
    // J(i) = J(i-1) + (|D(i-1,i)| - J(i-1))/16

    int diff = time_difference - prev_difference_up;
    if (diff < 0){
        diff = -1 * diff;
    }
    jitter_up = jitter_up + ((diff - jitter_up)/16);
    prev_difference_up = time_difference;
    return jitter_up;
}

static float get_jitter_dw(uint64_t time_difference){

    // This calculation based on the RFC3550 page 40
    // https://tools.ietf.org/rfcmarkup?rfc=3550&draft=&url=#section-6.4.1
    // D(i,j) = (Rj - Ri) - (Sj - Si) = (Rj - Sj) - (Ri - Si)
    // J(i) = J(i-1) + (|D(i-1,i)| - J(i-1))/16

    int diff = time_difference - prev_difference_dw;
    if (diff < 0){
        diff = -1 * diff;
    }
    jitter_dw = jitter_dw + ((diff - jitter_dw)/16);
    prev_difference_dw = time_difference;
    return jitter_dw;
}

static uint64_t get_average_rtt(uint64_t time_difference){
    total_time_diiference = total_time_diiference + time_difference;
    return total_time_diiference / number_of_packets;
}

void print_metrics(uint32_t j, uint16_t port, const ReflectorUPacket *pack) {

    /* Get Time of the received TWAMP-Test response message */
    TWAMPTimestamp recv_resp_time = get_timestamp();

    uint16_t remote_processing_time = get_time_difference(&pack->time, &pack->receive_time);

    /* Print different metrics */

    /* Compute round-trip */
    fprintf(stderr, "Round-trip time for TWAMP-Test packet %d for port %hd is %" PRIu64 " [usec]\n",
            j, port, get_time_difference(&recv_resp_time, &pack->sender_time) - remote_processing_time);
    fprintf(stderr, "Receive time - Send time for TWAMP-Test"
            " packet %d for port %d is %" PRIu64 " [usec]\n", j, port,
            get_time_difference(&pack->receive_time, &pack->sender_time));
    fprintf(stderr, "Reflect time - Send time for TWAMP-Test"
                    " packet %d for port %d is %" PRIu64 " [usec]\n", j, port,
            get_time_difference(&pack->time, &pack->sender_time));
    fprintf(stderr, "Now time - Reflect time for TWAMP-Test"
                    " packet %d for port %d is %" PRIu64 " [usec]\n", j, port,
            get_time_difference(&recv_resp_time, &pack->time));
    fprintf(stderr, "Remote processing time %" PRIu64 " [usec]\n",
            remote_processing_time);

    //control
    number_of_packets++;

    if (number_of_packets == 1){
        prev_difference = get_time_difference(&recv_resp_time, &pack->sender_time) - remote_processing_time;
        prev_difference_dw = get_time_difference(&recv_resp_time, &pack->time);
        prev_difference_up = get_time_difference(&pack->receive_time, &pack->sender_time);
    } else {
        fprintf(stderr, "Jitter rtt: %f usec \n", get_jitter(get_time_difference(&recv_resp_time, &pack->sender_time) - remote_processing_time));
        fprintf(stderr, "Jitter dw: %f usec \n", get_jitter_dw(get_time_difference(&recv_resp_time, &pack->time)));
        fprintf(stderr, "Jitter up: %f usec \n", get_jitter_up(get_time_difference(&pack->receive_time, &pack->sender_time)));
    }

    fprintf(stderr, "Average RTT : %" PRIu64 " usec \n", get_average_rtt(get_time_difference(&recv_resp_time, &pack->sender_time)));
}