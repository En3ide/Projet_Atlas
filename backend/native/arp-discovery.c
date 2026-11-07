// Scan ARP pour récupérer les IP et MAC des machines du réseau local
#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <linux/if_ether.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <netpacket/packet.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#define ARP_FRAME_SIZE 42
#define MAX_SCAN_ADDRESSES 4096
#define PROBE_INTERVAL_MS 5
#define PROBE_PASSES 2
#define RETRY_DELAY_MS 500
#define RESPONSE_WAIT_MS 2000
#define MAX_SCAN_DURATION_MS 60000
#define MAX_PACKETS_PER_TICK 64

struct network_interface {
    unsigned int index;
    uint32_t address;
    uint32_t netmask;
    unsigned char mac[6];
};

struct scan_range {
    bool is_cidr;
    unsigned int prefix;
    uint32_t first;
    uint32_t last;
};

struct scan_result {
    bool probed;
    bool found;
    unsigned char mac[6];
};

static void fail(const char *message) {
    perror(message);
    exit(EXIT_FAILURE);
}

static int64_t milliseconds(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) fail("clock_gettime");
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static void usage(FILE *output, const char *program) {
    fprintf(output, "Usage : %s INTERFACE IPV4_OU_CIDR\n", program);
    fprintf(output, "Exemples : %s eth0 192.168.1.42\n", program);
    fprintf(output, "           %s eth0 192.168.1.0/24\n", program);
    fprintf(output, "Les plages sont limitées à /20 ou plus petit, sur le même réseau local.\n");
}

/**
 * Cette fonction lit l'IP ou la plage demandée et calcule ses bornes
 *
 * @param value L'adresse IPv4, avec ou sans préfixe CIDR
 * @param range La plage à remplir, avec les adresses en ordre hôte
 * @return true si la cible est valide et ne dépasse pas 4096 adresses
 */
static bool parse_target(const char *value, struct scan_range *range) {
    const char *slash = strchr(value, '/');
    size_t address_length = slash ? (size_t)(slash - value) : strlen(value);
    if (address_length == 0 || address_length >= INET_ADDRSTRLEN) return false;

    char address_text[INET_ADDRSTRLEN];
    memcpy(address_text, value, address_length);
    address_text[address_length] = '\0';

    struct in_addr address;
    if (inet_pton(AF_INET, address_text, &address) != 1) return false;

    unsigned long prefix = 32;
    if (slash) {
        // On accepte uniquement un préfixe numérique entre 20 et 32
        const char *prefix_text = slash + 1;
        size_t prefix_length = strlen(prefix_text);
        if (prefix_length == 0 || prefix_length > 2) return false;
        for (size_t i = 0; i < prefix_length; i++) {
            if (prefix_text[i] < '0' || prefix_text[i] > '9') return false;
        }
        prefix = strtoul(prefix_text, NULL, 10);
        if (prefix < 20 || prefix > 32) return false;
    }

    uint32_t mask = UINT32_MAX << (32 - (unsigned int)prefix);
    uint32_t ipv4 = ntohl(address.s_addr);
    *range = (struct scan_range) {
        .is_cidr = slash != NULL,
        .prefix = (unsigned int)prefix,
        .first = ipv4 & mask,
        .last = (ipv4 & mask) | ~mask,
    };
    return true;
}

static void read_interface(const char *name, struct network_interface *network) {
    network->index = if_nametoindex(name);
    if (network->index == 0) fail("if_nametoindex");

    // On récupère les informations de notre interface, sans lire le cache ARP
    int info = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (info < 0) fail("socket");

    struct ifreq request = {0};
    memcpy(request.ifr_name, name, strlen(name) + 1);
    if (ioctl(info, SIOCGIFFLAGS, &request) < 0) fail("SIOCGIFFLAGS");
    if (!(request.ifr_flags & IFF_UP)) {
        fprintf(stderr, "L'interface demandée n'est pas active.\n");
        exit(EXIT_FAILURE);
    }
    if (ioctl(info, SIOCGIFADDR, &request) < 0) fail("SIOCGIFADDR");
    network->address = ntohl(((struct sockaddr_in *)&request.ifr_addr)->sin_addr.s_addr);
    if (ioctl(info, SIOCGIFNETMASK, &request) < 0) fail("SIOCGIFNETMASK");
    network->netmask = ntohl(((struct sockaddr_in *)&request.ifr_netmask)->sin_addr.s_addr);
    if (ioctl(info, SIOCGIFHWADDR, &request) < 0) fail("SIOCGIFHWADDR");
    if (request.ifr_hwaddr.sa_family != ARPHRD_ETHER) {
        fprintf(stderr, "Il faut une interface compatible Ethernet pour utiliser ARP.\n");
        exit(EXIT_FAILURE);
    }
    memcpy(network->mac, request.ifr_hwaddr.sa_data, sizeof(network->mac));
    close(info);
}

static bool range_is_local(const struct scan_range *range,
                           const struct network_interface *network) {
    uint32_t subnet = network->address & network->netmask;
    return (range->first & network->netmask) == subnet &&
           (range->last & network->netmask) == subnet;
}

static bool is_target_address(uint32_t address, const struct scan_range *range,
                              const struct network_interface *network) {
    if (address < range->first || address > range->last ||
        address == network->address) return false;

    // On laisse de côté les adresses réseau et broadcast, sauf pour /31 et /32
    if (range->is_cidr && range->prefix <= 30 &&
        (address == range->first || address == range->last)) return false;

    uint32_t subnet = network->address & network->netmask;
    if (~network->netmask >= 3 &&
        (address == subnet || address == (subnet | ~network->netmask))) return false;

    return true;
}

/**
 * Cette fonction prépare une requête ARP pour une IP du réseau
 *
 * @param frame La trame Ethernet et ARP à remplir
 * @param network Les informations de notre interface
 * @param target L'IP à interroger, en ordre hôte
 */
static void build_request(unsigned char frame[ARP_FRAME_SIZE],
                          const struct network_interface *network, uint32_t target) {
    memset(frame, 0, ARP_FRAME_SIZE);
    memset(frame, 0xff, 6);              // On diffuse la requête sur le réseau
    memcpy(frame + 6, network->mac, 6);  // Notre MAC dans l'en-tête Ethernet
    frame[12] = 0x08; frame[13] = 0x06; // On indique que c'est de l'ARP
    frame[14] = 0x00; frame[15] = 0x01; // On utilise Ethernet
    frame[16] = 0x08; frame[17] = 0x00; // On cherche une adresse IPv4
    frame[18] = 6; frame[19] = 4;       // 6 octets pour la MAC, 4 pour l'IP
    frame[20] = 0x00; frame[21] = 0x01; // 1 = requête ARP
    memcpy(frame + 22, network->mac, 6);

    uint32_t source_ip = htonl(network->address);
    uint32_t target_ip = htonl(target);
    memcpy(frame + 28, &source_ip, 4);
    // La MAC cible reste à zéro puisqu'on cherche à la récupérer
    memcpy(frame + 38, &target_ip, 4);
}

/**
 * Cette fonction récupère les réponses qui correspondent à nos requêtes
 *
 * @param reply La trame reçue
 * @param size La taille de cette trame
 * @param range La plage interrogée
 * @param network Les informations de notre interface
 * @param results Les résultats rangés par IP, pour éviter les doublons
 * @return true si on vient de découvrir une nouvelle IP
 */
static bool collect_reply(const unsigned char *reply, size_t size,
                          const struct scan_range *range,
                          const struct network_interface *network,
                          struct scan_result *results) {
    if (size < ARP_FRAME_SIZE ||
        reply[12] != 0x08 || reply[13] != 0x06 ||
        reply[14] != 0x00 || reply[15] != 0x01 ||
        reply[16] != 0x08 || reply[17] != 0x00 ||
        reply[18] != 6 || reply[19] != 4 ||
        reply[20] != 0x00 || reply[21] != 0x02) return false;

    uint32_t sender_ip, target_ip;
    memcpy(&sender_ip, reply + 28, 4);
    memcpy(&target_ip, reply + 38, 4);
    sender_ip = ntohl(sender_ip);
    target_ip = ntohl(target_ip);

    // On garde les réponses adressées à notre interface, avec une MAC unicast valide
    const unsigned char zero_mac[6] = {0};
    if (target_ip != network->address ||
        memcmp(reply + 32, network->mac, 6) != 0 ||
        memcmp(reply + 6, reply + 22, 6) != 0 ||
        (reply[22] & 1) != 0 ||
        memcmp(reply + 22, zero_mac, 6) == 0 ||
        !is_target_address(sender_ip, range, network)) return false;

    size_t index = (size_t)(sender_ip - range->first);
    if (!results[index].probed || results[index].found) return false;

    results[index].found = true;
    memcpy(results[index].mac, reply + 22, 6);
    return true;
}

static void print_device(FILE *output, uint32_t address, const unsigned char mac[6]) {
    struct in_addr ipv4 = {.s_addr = htonl(address)};
    char text[INET_ADDRSTRLEN];
    if (!inet_ntop(AF_INET, &ipv4, text, sizeof(text))) fail("inet_ntop");
    fprintf(output,
            "{\"ipv4\":\"%s\",\"mac\":\"%02x:%02x:%02x:%02x:%02x:%02x\"}",
            text, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static int print_results(FILE *output, const struct scan_range *range,
                         const struct scan_result *results) {
    if (!range->is_cidr) {
        // On garde le format utilisé par scanHost() pour une seule IP
        if (!results[0].found) {
            fprintf(stderr, "Aucune réponse ARP reçue pour cette IP.\n");
            return 2;
        }
        print_device(output, range->first, results[0].mac);
        fputc('\n', output);
    } else {
        struct in_addr first = {.s_addr = htonl(range->first)};
        char text[INET_ADDRSTRLEN];
        if (!inet_ntop(AF_INET, &first, text, sizeof(text))) fail("inet_ntop");
        fprintf(output, "{\"network\":\"%s/%u\",\"devices\":[", text, range->prefix);

        bool comma = false;
        size_t count = (size_t)(range->last - range->first) + 1;
        for (size_t i = 0; i < count; i++) {
            if (!results[i].found) continue;
            if (comma) fputc(',', output);
            print_device(output, range->first + (uint32_t)i, results[i].mac);
            comma = true;
        }
        fputs("]}\n", output);
    }
    return ferror(output) ? EXIT_FAILURE : EXIT_SUCCESS;
}

/**
 * Cette fonction envoie les requêtes progressivement et écoute les réponses entre les envois
 *
 * @param fd La socket ARP ouverte sur l'interface choisie
 * @param range La plage à interroger
 * @param network Les informations de notre interface
 * @param results Les réponses trouvées pendant le scan
 * @return 0 si le scan est terminé, 1 en cas d'erreur système ou de délai dépassé
 */
static int scan_network(int fd, const struct scan_range *range,
                        const struct network_interface *network,
                        struct scan_result *results) {
    size_t count = (size_t)(range->last - range->first) + 1;
    size_t target_count = 0, found_count = 0, cursor = 0;
    for (size_t i = 0; i < count; i++) {
        if (is_target_address(range->first + (uint32_t)i, range, network)) target_count++;
    }
    if (target_count == 0) return EXIT_SUCCESS;

    struct sockaddr_ll link = {0};
    link.sll_family = AF_PACKET;
    link.sll_protocol = htons(ETH_P_ARP);
    link.sll_ifindex = (int)network->index;
    link.sll_halen = 6;
    memset(link.sll_addr, 0xff, 6);

    unsigned int pass = 0;
    int64_t now = milliseconds();
    int64_t next_send = now, finish_at = 0;
    const int64_t deadline = now + MAX_SCAN_DURATION_MS;
    struct pollfd ready = {.fd = fd, .events = POLLIN};

    while (found_count < target_count) {
        now = milliseconds();
        if (now >= deadline) {
            fprintf(stderr, "Le scan a dépassé son délai maximum de 60 secondes.\n");
            return EXIT_FAILURE;
        }
        if (finish_at != 0 && now >= finish_at) break;

        if (finish_at == 0 && now >= next_send) {
            // On ne renvoie pas de requête aux machines qui ont déjà répondu
            while (cursor < count &&
                   (!is_target_address(range->first + (uint32_t)cursor, range, network) ||
                    results[cursor].found)) cursor++;

            if (cursor == count) {
                pass++;
                if (pass < PROBE_PASSES) {
                    cursor = 0;
                    next_send = now + RETRY_DELAY_MS;
                } else {
                    finish_at = now + RESPONSE_WAIT_MS;
                }
            } else {
                unsigned char frame[ARP_FRAME_SIZE];
                build_request(frame, network, range->first + (uint32_t)cursor);
                ssize_t sent = sendto(fd, frame, sizeof(frame), 0,
                                      (struct sockaddr *)&link, sizeof(link));
                if (sent == (ssize_t)sizeof(frame)) {
                    results[cursor].probed = true;
                    cursor++;
                } else if (!(sent < 0 && (errno == EINTR || errno == EAGAIN ||
                                          errno == EWOULDBLOCK || errno == ENOBUFS))) {
                    if (sent >= 0) errno = EIO;
                    perror("sendto");
                    return EXIT_FAILURE;
                }
                next_send = now + PROBE_INTERVAL_MS;
            }
        }

        int64_t wake_at = finish_at != 0 ? finish_at : next_send;
        if (wake_at > deadline) wake_at = deadline;
        now = milliseconds();
        int timeout = wake_at > now ? (int)(wake_at - now) : 0;
        ready.revents = 0;
        int available = poll(&ready, 1, timeout);
        if (available < 0) {
            if (errno == EINTR) continue;
            perror("poll");
            return EXIT_FAILURE;
        }
        if (available == 0) continue;
        if (ready.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            fprintf(stderr, "La socket ARP n'est plus disponible.\n");
            return EXIT_FAILURE;
        }

        // On limite aussi la lecture par tour pour continuer à envoyer nos requêtes
        for (unsigned int i = 0; i < MAX_PACKETS_PER_TICK; i++) {
            unsigned char reply[2048];
            struct sockaddr_ll sender = {0};
            socklen_t sender_length = sizeof(sender);
            ssize_t size = recvfrom(fd, reply, sizeof(reply), MSG_DONTWAIT,
                                    (struct sockaddr *)&sender, &sender_length);
            if (size < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                if (errno == EINTR) continue;
                perror("recvfrom");
                return EXIT_FAILURE;
            }
            if (sender.sll_pkttype == PACKET_OUTGOING ||
                sender.sll_ifindex != (int)network->index) continue;
            if (collect_reply(reply, (size_t)size, range, network, results)) found_count++;
        }
    }
    return EXIT_SUCCESS;
}

int main(int argc, char **argv) {
    if (argc == 2 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        usage(stdout, argv[0]);
        return EXIT_SUCCESS;
    }
    if (argc != 3 || strlen(argv[1]) == 0 || strlen(argv[1]) >= IFNAMSIZ) {
        usage(stderr, argv[0]);
        return EXIT_FAILURE;
    }

    struct scan_range range;
    if (!parse_target(argv[2], &range)) {
        fprintf(stderr, "Il faut une IPv4 valide ou une plage CIDR entre /20 et /32.\n");
        return EXIT_FAILURE;
    }

    struct network_interface network = {0};
    read_interface(argv[1], &network);
    if (!range_is_local(&range, &network)) {
        fprintf(stderr, "La cible doit rester dans le sous-réseau IPv4 de l'interface choisie.\n");
        return EXIT_FAILURE;
    }
    if (!range.is_cidr && !is_target_address(range.first, &range, &network)) {
        fprintf(stderr, "Choisis une autre IP que celle du serveur, du réseau ou du broadcast.\n");
        return EXIT_FAILURE;
    }

    struct scan_result results[MAX_SCAN_ADDRESSES] = {0};
    // Il faut CAP_NET_RAW pour ouvrir la socket qui envoie et reçoit les trames ARP
    int fd = socket(AF_PACKET, SOCK_RAW | SOCK_NONBLOCK | SOCK_CLOEXEC, htons(ETH_P_ARP));
    if (fd < 0) fail("Socket ARP (CAP_NET_RAW nécessaire)");
    struct sockaddr_ll link = {0};
    link.sll_family = AF_PACKET;
    link.sll_protocol = htons(ETH_P_ARP);
    link.sll_ifindex = (int)network.index;
    if (bind(fd, (struct sockaddr *)&link, sizeof(link)) < 0) fail("bind");

    int status = scan_network(fd, &range, &network, results);
    close(fd);
    if (status != EXIT_SUCCESS) return status;
    return print_results(stdout, &range, results);
}
