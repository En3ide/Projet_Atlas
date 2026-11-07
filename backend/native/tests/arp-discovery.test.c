// On rejoue des échanges ARP en mémoire, sans ouvrir de socket réseau
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <linux/if_ether.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netpacket/packet.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static int simulated_clock_gettime(clockid_t clock_id, struct timespec *value);
static int simulated_poll(struct pollfd *fds, nfds_t count, int timeout);
static ssize_t simulated_sendto(int fd, const void *data, size_t size, int flags,
                                const struct sockaddr *destination, socklen_t length);
static ssize_t simulated_recvfrom(int fd, void *data, size_t size, int flags,
                                  struct sockaddr *sender, socklen_t *length);

#define main atlas_arp_cli_main
#define clock_gettime simulated_clock_gettime
#define poll simulated_poll
#define sendto simulated_sendto
#define recvfrom simulated_recvfrom
#include "../arp-discovery.c"
#undef main
#undef clock_gettime
#undef poll
#undef sendto
#undef recvfrom

enum simulation_mode { RESPOND, NEVER_RESPOND, RETRY_ONE, SEND_ERROR, RUN_OUT_OF_TIME };

static struct network_interface fixture = {
    .index = 7,
    .address = 0x0a2a00b6,  // 10.42.0.182
    .netmask = 0xffffff00,
    .mac = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55},
};

static const unsigned char device_mac[6] = {0x02, 0xaa, 0xbb, 0xcc, 0xdd, 0xee};
static enum simulation_mode mode;
static int64_t simulated_time;
static int64_t last_send;
static unsigned int sends[MAX_SCAN_ADDRESSES];
static size_t sent_count, first_receive_after_sends;
static unsigned char pending_reply[ARP_FRAME_SIZE];
static unsigned int pending_count;

static uint32_t ipv4(const char *text) {
    struct in_addr value;
    assert(inet_pton(AF_INET, text, &value) == 1);
    return ntohl(value.s_addr);
}

static struct scan_range target(const char *text) {
    struct scan_range range;
    assert(parse_target(text, &range));
    return range;
}

static void make_reply(unsigned char reply[ARP_FRAME_SIZE], uint32_t sender) {
    struct network_interface remote = fixture;
    remote.address = sender;
    memcpy(remote.mac, device_mac, sizeof(device_mac));
    build_request(reply, &remote, fixture.address);
    memcpy(reply, fixture.mac, 6);
    memcpy(reply + 32, fixture.mac, 6);
    reply[21] = 2;
}

static void reset_simulation(enum simulation_mode next_mode) {
    mode = next_mode;
    simulated_time = 0;
    last_send = -PROBE_INTERVAL_MS;
    sent_count = 0;
    first_receive_after_sends = 0;
    pending_count = 0;
    memset(sends, 0, sizeof(sends));
}

static int simulated_clock_gettime(clockid_t clock_id, struct timespec *value) {
    assert(clock_id == CLOCK_MONOTONIC);
    value->tv_sec = simulated_time / 1000;
    value->tv_nsec = (simulated_time % 1000) * 1000000;
    return 0;
}

static int simulated_poll(struct pollfd *fds, nfds_t count, int timeout) {
    assert(count == 1 && timeout >= 0);
    if (mode == RUN_OUT_OF_TIME) {
        simulated_time += MAX_SCAN_DURATION_MS;
        return 0;
    }
    if (pending_count != 0) {
        fds[0].revents = POLLIN;
        return 1;
    }
    simulated_time += timeout;
    return 0;
}

static ssize_t simulated_sendto(int fd, const void *data, size_t size, int flags,
                                const struct sockaddr *destination, socklen_t length) {
    (void)fd;
    assert(flags == 0 && size == ARP_FRAME_SIZE && length == sizeof(struct sockaddr_ll));
    const struct sockaddr_ll *link = (const struct sockaddr_ll *)destination;
    assert(link->sll_ifindex == (int)fixture.index);
    assert(simulated_time - last_send >= PROBE_INTERVAL_MS);
    last_send = simulated_time;
    if (mode == SEND_ERROR) {
        errno = EACCES;
        return -1;
    }

    const unsigned char *frame = data;
    assert(frame[21] == 1 && memcmp(frame + 22, fixture.mac, 6) == 0);
    uint32_t address;
    memcpy(&address, frame + 38, 4);
    address = ntohl(address);
    assert((address & fixture.netmask) == (fixture.address & fixture.netmask));
    assert(address != fixture.address);
    unsigned int index = address & ~fixture.netmask;
    assert(index < MAX_SCAN_ADDRESSES);
    assert(index != 0 && index != ~fixture.netmask);
    sends[index]++;
    sent_count++;

    if (mode == RESPOND ||
        (mode == RETRY_ONE && (index != 4 || sends[index] == 2))) {
        make_reply(pending_reply, address);
        pending_count = 2; // On injecte aussi un doublon pour chaque réponse
    }
    return (ssize_t)size;
}

static ssize_t simulated_recvfrom(int fd, void *data, size_t size, int flags,
                                  struct sockaddr *sender, socklen_t *length) {
    (void)fd;
    assert(flags == MSG_DONTWAIT && size >= ARP_FRAME_SIZE);
    if (pending_count == 0) {
        errno = EAGAIN;
        return -1;
    }
    if (first_receive_after_sends == 0) first_receive_after_sends = sent_count;
    assert(*length >= sizeof(struct sockaddr_ll));
    struct sockaddr_ll *link = (struct sockaddr_ll *)sender;
    memset(link, 0, sizeof(*link));
    link->sll_ifindex = (int)fixture.index;
    link->sll_pkttype = PACKET_HOST;
    *length = sizeof(*link);
    memcpy(data, pending_reply, ARP_FRAME_SIZE);
    pending_count--;
    return ARP_FRAME_SIZE;
}

static void test_ranges(void) {
    struct scan_range range = target("10.42.0.182/24");
    assert(range.is_cidr && range.first == ipv4("10.42.0.0"));
    assert(range.last == ipv4("10.42.0.255"));
    assert(range_is_local(&range, &fixture));
    assert(!is_target_address(ipv4("10.42.0.0"), &range, &fixture));
    assert(!is_target_address(ipv4("10.42.0.255"), &range, &fixture));
    assert(!is_target_address(fixture.address, &range, &fixture));
    assert(is_target_address(ipv4("10.42.0.1"), &range, &fixture));

    range = target("10.42.0.1/20");
    assert(range.last - range.first + 1 == MAX_SCAN_ADDRESSES);
    assert(!range_is_local(&range, &fixture));
    range = target("10.43.0.0/24");
    assert(!range_is_local(&range, &fixture));

    range = target("10.42.0.4/31");
    assert(is_target_address(ipv4("10.42.0.4"), &range, &fixture));
    assert(is_target_address(ipv4("10.42.0.5"), &range, &fixture));
    range = target("10.42.0.4/32");
    assert(range.is_cidr && range.first == range.last);
    assert(is_target_address(range.first, &range, &fixture));
    range = target("10.42.0.4");
    assert(!range.is_cidr && range.first == range.last);

    const char *invalid[] = {"", "10.42.0", "999.1.1.1", "10.42.0.0/19",
        "10.42.0.0/33", "10.42.0.0/-1", "10.42.0.0/", "10.42.0.0/ 24",
        "10.42.0.0/24x", "10.42.0.0/24/24"};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        assert(!parse_target(invalid[i], &range));
    }
    puts("CIDR, bornes du réseau, IP locale, /31 et /32 : OK");
}

static void test_packets(void) {
    struct scan_range range = target("10.42.0.0/24");
    struct scan_result results[MAX_SCAN_ADDRESSES] = {0};
    unsigned char reply[ARP_FRAME_SIZE];
    uint32_t sender = ipv4("10.42.0.4");
    make_reply(reply, sender);
    assert(!collect_reply(reply, sizeof(reply), &range, &fixture, results));
    results[4].probed = true;

    for (size_t length = 0; length < ARP_FRAME_SIZE; length++) {
        unsigned char *short_packet = malloc(length ? length : 1);
        assert(short_packet != NULL);
        memcpy(short_packet, reply, length);
        assert(!collect_reply(short_packet, length, &range, &fixture, results));
        free(short_packet);
    }
    const size_t invalid_fields[] = {12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 32, 38};
    for (size_t i = 0; i < sizeof(invalid_fields) / sizeof(invalid_fields[0]); i++) {
        size_t offset = invalid_fields[i];
        reply[offset] ^= 0xff;
        assert(!collect_reply(reply, sizeof(reply), &range, &fixture, results));
        reply[offset] ^= 0xff;
    }
    make_reply(reply, ipv4("10.43.0.4"));
    assert(!collect_reply(reply, sizeof(reply), &range, &fixture, results));
    make_reply(reply, fixture.address);
    assert(!collect_reply(reply, sizeof(reply), &range, &fixture, results));
    make_reply(reply, sender);
    reply[22] |= 1;
    memcpy(reply + 6, reply + 22, 6);
    assert(!collect_reply(reply, sizeof(reply), &range, &fixture, results));
    memset(reply + 22, 0, 6);
    memset(reply + 6, 0, 6);
    assert(!collect_reply(reply, sizeof(reply), &range, &fixture, results));

    make_reply(reply, sender);
    assert(collect_reply(reply, sizeof(reply), &range, &fixture, results));
    assert(memcmp(results[4].mac, device_mac, 6) == 0);
    assert(!collect_reply(reply, sizeof(reply), &range, &fixture, results));
    reply[27] ^= 1;
    memcpy(reply + 6, reply + 22, 6);
    assert(!collect_reply(reply, sizeof(reply), &range, &fixture, results));
    assert(memcmp(results[4].mac, device_mac, 6) == 0);
    puts("Trames tronquées, adresses invalides, réponses non sollicitées et doublons : OK");
}

static void assert_output(const struct scan_range *range, const struct scan_result *results,
                          int expected_status, const char *expected_text) {
    FILE *output = tmpfile();
    assert(output != NULL);
    assert(print_results(output, range, results) == expected_status);
    rewind(output);
    char text[1024] = {0};
    size_t length = fread(text, 1, sizeof(text) - 1, output);
    assert(!ferror(output) && length == strlen(expected_text));
    assert(strcmp(text, expected_text) == 0);
    fclose(output);
}

static void test_output(void) {
    struct scan_range range = target("10.42.0.182/24");
    struct scan_result results[MAX_SCAN_ADDRESSES] = {0};
    assert_output(&range, results, 0, "{\"network\":\"10.42.0.0/24\",\"devices\":[]}\n");
    results[5].found = true;
    results[2].found = true;
    memcpy(results[5].mac, device_mac, 6);
    memcpy(results[2].mac, device_mac, 6);
    assert_output(&range, results, 0,
        "{\"network\":\"10.42.0.0/24\",\"devices\":["
        "{\"ipv4\":\"10.42.0.2\",\"mac\":\"02:aa:bb:cc:dd:ee\"},"
        "{\"ipv4\":\"10.42.0.5\",\"mac\":\"02:aa:bb:cc:dd:ee\"}]}\n");
    range = target("10.42.0.2");
    memset(results, 0, sizeof(results));
    assert_output(&range, results, 2, "");
    results[0].found = true;
    memcpy(results[0].mac, device_mac, 6);
    assert_output(&range, results, 0,
        "{\"ipv4\":\"10.42.0.2\",\"mac\":\"02:aa:bb:cc:dd:ee\"}\n");
    puts("JSON trié, plage vide et compatibilité mono-IP : OK");
}

static void test_exchange(void) {
    struct scan_range range = target("10.42.0.0/29");
    struct scan_result results[MAX_SCAN_ADDRESSES] = {0};
    reset_simulation(RETRY_ONE);
    assert(scan_network(-1, &range, &fixture, results) == 0);
    assert(first_receive_after_sends == 1 && sent_count == 7);
    for (unsigned int i = 1; i <= 6; i++) {
        assert(results[i].found);
        assert(sends[i] == (i == 4 ? 2u : 1u));
    }

    range = target("10.42.0.182/24");
    memset(results, 0, sizeof(results));
    reset_simulation(NEVER_RESPOND);
    assert(scan_network(-1, &range, &fixture, results) == 0);
    assert(sent_count == 253 * PROBE_PASSES);
    assert(sends[0] == 0 && sends[255] == 0 && sends[182] == 0);
    assert(simulated_time < MAX_SCAN_DURATION_MS);

    // On vérifie la plus grande plage autorisée, toujours sans trafic réel
    fixture.netmask = 0xfffff000;
    range = target("10.42.0.0/20");
    memset(results, 0, sizeof(results));
    reset_simulation(NEVER_RESPOND);
    assert(scan_network(-1, &range, &fixture, results) == 0);
    assert(sent_count == (MAX_SCAN_ADDRESSES - 3) * PROBE_PASSES);
    assert(sends[0] == 0 && sends[4095] == 0 && sends[182] == 0);
    assert(simulated_time < MAX_SCAN_DURATION_MS);
    fixture.netmask = 0xffffff00;

    range = target("10.42.0.1");
    memset(results, 0, sizeof(results));
    reset_simulation(RESPOND);
    assert(scan_network(-1, &range, &fixture, results) == 0);
    assert(sent_count == 1 && results[0].found);

    memset(results, 0, sizeof(results));
    reset_simulation(SEND_ERROR);
    assert(scan_network(-1, &range, &fixture, results) == 1);
    reset_simulation(RUN_OUT_OF_TIME);
    assert(scan_network(-1, &range, &fixture, results) == 1);
    puts("Envois cadencés, réception pendant l'envoi, relance, erreurs et délai global : OK");
}

int main(void) {
    test_ranges();
    test_packets();
    test_output();
    test_exchange();
    return 0;
}
