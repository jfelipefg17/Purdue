#include "prog_rdt.h"
#include <string.h>

struct msg_node {
    struct msg data;
    struct msg_node *next;
};

struct msg_node *A_queue_head = NULL;
struct msg_node *A_queue_tail = NULL;
int A_seq = 0;
int A_sending = 0;
struct pkt A_current_pkt;
int B_expected = 0;
int A_sent = 0;

int compute_checksum(struct pkt *p) {
    int sum = p->seqnum + p->acknum;
    int i;
    for (i = 0; i < 20; i++) {
        sum += (unsigned char) p->payload[i];
    }
    return sum;
}

int is_corrupt(struct pkt *p) {
    return compute_checksum(p) != p->checksum;
}

void send_next_A() {
    if (A_queue_head == NULL) return;
    struct msg m = A_queue_head->data;
    struct msg_node *old = A_queue_head;
    A_queue_head = A_queue_head->next;
    if (A_queue_head == NULL) A_queue_tail = NULL;
    free(old);
    struct pkt p;
    p.seqnum = A_seq;
    p.acknum = 0;
    memcpy(p.payload, m.data, 20);
    p.checksum = compute_checksum(&p);
    A_current_pkt = p;
    tolayer3(A, p);
    starttimer(A, 15.0);
    A_sending = 1;
}

int A_output(struct msg message) {
    if (TRACE >= 2) {
        printf("A_output: received message from layer5: %.20s\n", message.data);
    }
    struct msg_node *new_node = (struct msg_node *) malloc(sizeof(struct msg_node));
    if (new_node == NULL) return -1;
    new_node->data = message;
    new_node->next = NULL;
    if (A_queue_tail != NULL) {
        A_queue_tail->next = new_node;
    } else {
        A_queue_head = new_node;
    }
    A_queue_tail = new_node;
    if (!A_sending) {
        send_next_A();
    } else if (TRACE >= 2) {
        printf("A_output: sender busy, queued the message\n");
    }
    return 0;
}

int A_input(struct pkt packet) {
    if (TRACE >= 2) {
        printf("A_input: received packet seq=%d ack=%d checksum=%d payload=%.20s\n",
               packet.seqnum, packet.acknum, packet.checksum, packet.payload);
    }
    if (is_corrupt(&packet)) {
        if (TRACE >= 2) printf("A_input: packet corrupt, ignored\n");
        return 0;
    }
    if (packet.acknum == A_seq) {
        if (TRACE >= 2) printf("A_input: correct ACK for seq=%d, stopping timer, flipping seq to %d\n", A_seq,
                               1 - A_seq);
        stoptimer(A);
        A_seq = 1 - A_seq;
        A_sending = 0;
        send_next_A();
    } else if (TRACE >= 2) {
        printf("A_input: wrong ACK (expected %d), ignored\n", A_seq);
    }
    return 0;
}

int A_timerinterrupt() {
    if (TRACE >= 2) {
        printf("A_timerinterrupt: timeout, retransmitting packet seq=%d ack=%d checksum=%d payload=%.20s\n",
               A_current_pkt.seqnum, A_current_pkt.acknum, A_current_pkt.checksum, A_current_pkt.payload);
    }
    tolayer3(A, A_current_pkt);
    starttimer(A, 15.0);
    return 0;
}

int A_init() {
    A_seq = 0;
    A_queue_head = NULL;
    A_queue_tail = NULL;
    A_sending = 0;
    return 0;
}

int B_input(struct pkt packet) {
    if (TRACE >= 2) {
        printf("B_input: received packet seq=%d ack=%d checksum=%d payload=%.20s\n",
               packet.seqnum, packet.acknum, packet.checksum, packet.payload);
    }
    if (is_corrupt(&packet)) {
        if (TRACE >= 2) printf("B_input: packet corrupt, discarded\n");
        return 0;
    }
    struct pkt ack_pkt;
    ack_pkt.seqnum = 0;
    ack_pkt.acknum = 1 - B_expected;
    memset(ack_pkt.payload, 0, 20);
    ack_pkt.checksum = compute_checksum(&ack_pkt);
    if (packet.seqnum == B_expected) {
        if (TRACE >= 2)
            printf("B_input: correct seq=%d, delivering to layer5: %.20s, sending ACK %d\n",
                   packet.seqnum, packet.payload, B_expected);
        tolayer5(B, packet.payload);
        ack_pkt.acknum = B_expected;
        ack_pkt.checksum = compute_checksum(&ack_pkt);
        B_expected = 1 - B_expected;
    } else if (TRACE >= 2) {
        printf("B_input: wrong seq (expected %d), sending duplicate ACK %d\n", B_expected, ack_pkt.acknum);
    }
    tolayer3(B, ack_pkt);
    return 0;
}

int B_timerinterrupt() { return 0; }

int B_init() {
    B_expected = 0;
    return 0;
}

int TRACE = 1;
int nsim = 0;
int nsimmax = 0;
float time = 0.000;
float lossprob;
float corruptprob;
float lambda;
int ntolayer3;
int nlost;
int ncorrupt;

int main() {
    struct event *eventptr;
    struct msg msg2give;
    struct pkt pkt2give;
    int i, j;
    init();
    A_init();
    B_init();
    for (;;) {
        eventptr = evlist;
        if (NULL == eventptr) {
            goto terminate;
        }
        evlist = evlist->next;
        if (evlist != NULL) {
            evlist->prev = NULL;
        }
        if (TRACE >= 2) {
            printf("\nEVENT time: %f,", eventptr->evtime);
            printf("  type: %d", eventptr->evtype);
            if (eventptr->evtype == 0) {
                printf(", timerinterrupt  ");
            } else if (eventptr->evtype == 1) {
                printf(", fromlayer5 ");
            } else {
                printf(", fromlayer3 ");
            }
            printf(" entity: %d\n", eventptr->eventity);
        }
        time = eventptr->evtime;
        if (eventptr->evtype == FROM_LAYER5) {
            j = nsim % 26;
            for (i = 0; i < 20; i++) {
                msg2give.data[i] = 97 + j;
            }
            if (TRACE > 2) {
                printf("          MAINLOOP: data given to student: ");
                for (i = 0; i < 20; i++) {
                    printf("%c", msg2give.data[i]);
                }
                printf("\n");
            }
            nsim++;
            if (eventptr->eventity == A) {
                A_output(msg2give);
            }
            if (nsim < nsimmax) {
                generate_next_arrival();
            }
        } else if (eventptr->evtype == FROM_LAYER3) {
            pkt2give.seqnum = eventptr->pktptr->seqnum;
            pkt2give.acknum = eventptr->pktptr->acknum;
            pkt2give.checksum = eventptr->pktptr->checksum;
            for (i = 0; i < 20; i++) {
                pkt2give.payload[i] = eventptr->pktptr->payload[i];
            }
            if (eventptr->eventity == A) {
                A_input(pkt2give);
            } else {
                B_input(pkt2give);
            }
            free(eventptr->pktptr);
        } else if (eventptr->evtype == TIMER_INTERRUPT) {
            if (eventptr->eventity == A) {
                A_timerinterrupt();
            } else {
                B_timerinterrupt();
            }
        } else {
            printf("INTERNAL PANIC: unknown event type \n");
        }
        free(eventptr);
    }
    return 0;
terminate:
    printf(
        " Simulator terminated at time %f\n after sending %d msgs from layer5\n",
        time, nsim);
    printf("A sent %d packets from layer 3\n", A_sent);
    return 0;
}

void init() {
    int i;
    float sum, avg;
    printf("-----  Stop and Wait Network Simulator Version 1.1 -------- \n\n");
    printf("Enter the number of messages to simulate: ");
    scanf("%d", &nsimmax);
    printf("Enter  packet loss probability [enter 0.0 for no loss]:");
    scanf("%f", &lossprob);
    printf("Enter packet corruption probability [0.0 for no corruption]:");
    scanf("%f", &corruptprob);
    printf("Enter average time between messages from sender's layer5 [ > 0.0]:");
    scanf("%f", &lambda);
    printf("Enter TRACE:");
    scanf("%d", &TRACE);
    srand(rand_seed);
    sum = 0.0;
    for (i = 0; i < 1000; i++) {
        sum = sum + jimsrand();
    }
    avg = sum / 1000.0;
    if (avg < 0.25 || avg > 0.75) {
        printf("It is likely that random number generation on your machine\n");
        printf("is different from what this emulator expects.  Please take\n");
        printf("a look at the routine jimsrand() in the emulator code. Sorry. \n");
        exit(0);
    }
    ntolayer3 = 0;
    nlost = 0;
    ncorrupt = 0;
    time = 0.0;
    generate_next_arrival();
}

float jimsrand() {
    double mmm = INT_MAX;
    float x;
    x = rand_r(&rand_seed) / mmm;
    return x;
}

void generate_next_arrival() {
    double x;
    struct event *evptr;
    if (TRACE > 2) {
        printf("          GENERATE NEXT ARRIVAL: creating new arrival\n");
    }
    x = lambda * jimsrand() * 2;
    evptr = (struct event *) malloc(sizeof(struct event));
    evptr->evtime = time + x;
    evptr->evtype = FROM_LAYER5;
    if (BIDIRECTIONAL && (jimsrand() > 0.5)) {
        evptr->eventity = B;
    } else {
        evptr->eventity = A;
    }
    insertevent(evptr);
}

void insertevent(struct event *p) {
    struct event *q, *qold;
    if (TRACE > 2) {
        printf("            INSERTEVENT: time is %lf\n", time);
        printf("            INSERTEVENT: future time will be %lf\n", p->evtime);
    }
    q = evlist;
    if (NULL == q) {
        evlist = p;
        p->next = NULL;
        p->prev = NULL;
    } else {
        for (qold = q; q != NULL && p->evtime > q->evtime; q = q->next) {
            qold = q;
        }
        if (NULL == q) {
            qold->next = p;
            p->prev = qold;
            p->next = NULL;
        } else if (q == evlist) {
            p->next = evlist;
            p->prev = NULL;
            p->next->prev = p;
            evlist = p;
        } else {
            p->next = q;
            p->prev = q->prev;
            q->prev->next = p;
            q->prev = p;
        }
    }
}

void stoptimer(int AorB) {
    struct event *q;
    if (TRACE > 2) {
        printf("          STOP TIMER: stopping timer at %f\n", time);
    }
    for (q = evlist; q != NULL; q = q->next) {
        if ((q->evtype == TIMER_INTERRUPT && q->eventity == AorB)) {
            if (NULL == q->next && NULL == q->prev) {
                evlist = NULL;
            } else if (NULL == q->next) {
                q->prev->next = NULL;
            } else if (q == evlist) {
                q->next->prev = NULL;
                evlist = q->next;
            } else {
                q->next->prev = q->prev;
                q->prev->next = q->next;
            }
            free(q);
            return;
        }
    }
    printf("Warning: unable to cancel your timer. It wasn't running.\n");
}

void starttimer(int AorB, float increment) {
    struct event *q;
    struct event *evptr;
    if (TRACE > 2) {
        printf("          START TIMER: starting timer at %f\n", time);
    }
    for (q = evlist; q != NULL; q = q->next) {
        if ((q->evtype == TIMER_INTERRUPT && q->eventity == AorB)) {
            printf("Warning: attempt to start a timer that is already started\n");
            return;
        }
    }
    evptr = (struct event *) malloc(sizeof(struct event));
    evptr->evtime = time + increment;
    evptr->evtype = TIMER_INTERRUPT;
    evptr->eventity = AorB;
    insertevent(evptr);
}

void tolayer3(int AorB, struct pkt packet) {
    struct pkt *mypktptr;
    struct event *evptr, *q;
    float lastime, x;
    int i;
    ntolayer3++;
    if (AorB == A) A_sent++;
    if (jimsrand() < lossprob) {
        nlost++;
        if (TRACE > 0) {
            printf("          TOLAYER3: packet being lost\n");
        }
        return;
    }
    mypktptr = (struct pkt *) malloc(sizeof(struct pkt));
    mypktptr->seqnum = packet.seqnum;
    mypktptr->acknum = packet.acknum;
    mypktptr->checksum = packet.checksum;
    for (i = 0; i < 20; ++i) {
        mypktptr->payload[i] = packet.payload[i];
    }
    if (TRACE > 2) {
        printf("          TOLAYER3: seq: %d, ack %d, check: %d ", mypktptr->seqnum,
               mypktptr->acknum, mypktptr->checksum);
        for (i = 0; i < 20; ++i) {
            printf("%c", mypktptr->payload[i]);
        }
        printf("\n");
    }
    evptr = (struct event *) malloc(sizeof(struct event));
    evptr->evtype = FROM_LAYER3;
    evptr->eventity = (AorB + 1) & 1;
    evptr->pktptr = mypktptr;
    lastime = time;
    for (q = evlist; q != NULL; q = q->next) {
        if ((q->evtype == FROM_LAYER3 && q->eventity == evptr->eventity)) {
            lastime = q->evtime;
        }
    }
    evptr->evtime = lastime + 1 + 9 * jimsrand();
    if (jimsrand() < corruptprob) {
        ncorrupt++;
        if ((x = jimsrand()) < .75) {
            mypktptr->payload[0] = 'Z';
        } else if (x < .875) {
            mypktptr->seqnum = 999999;
        } else {
            mypktptr->acknum = 999999;
        }
        if (TRACE > 0) {
            printf("          TOLAYER3: packet being corrupted\n");
        }
    }
    if (TRACE > 2) {
        printf("          TOLAYER3: scheduling arrival on other side\n");
    }
    insertevent(evptr);
}

void tolayer5(int AorB, const char *datasent) {
    (void) AorB;
    int i;
    if (TRACE > 2) {
        printf("          TOLAYER5: data received: ");
        for (i = 0; i < 20; i++) {
            printf("%c", datasent[i]);
        }
        printf("\n");
    }
}
