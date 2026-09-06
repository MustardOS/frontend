#define _DEFAULT_SOURCE

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <mdns/mdns.h>

#define MUDNS_VERSION "0.1.0"
#define MUDNS_MAX_ENDPOINTS 32
#define MUDNS_MAX_SERVICES 16
#define MUDNS_PACKET_SIZE 2048
#define MUDNS_NAME_SIZE 256
#define MUDNS_RESCAN_MS 5000

struct address_desc {
    char interface[IF_NAMESIZE];
    struct sockaddr_storage address;
    socklen_t address_size;
};

struct endpoint {
    struct address_desc desc;
    int socket;
};

struct service {
    char type[MUDNS_NAME_SIZE];
    char instance[MUDNS_NAME_SIZE];
    uint16_t port;
};

struct server {
    char host_label[64];
    char host_name[MUDNS_NAME_SIZE];
    char interface[IF_NAMESIZE];
    struct endpoint endpoints[MUDNS_MAX_ENDPOINTS];
    size_t endpoint_count;
    struct service services[MUDNS_MAX_SERVICES];
    size_t service_count;
    int verbose;
    uint32_t recv_buffer[MUDNS_PACKET_SIZE / sizeof(uint32_t)];
    uint32_t send_buffer[MUDNS_PACKET_SIZE / sizeof(uint32_t)];
};

struct callback_context {
    struct server *server;
    struct endpoint *endpoint;
};

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t reload_interfaces = 0;

static mdns_string_t string_ref(const char *value) {
    const mdns_string_t result = {value, strlen(value)};
    return result;
}

static int dns_equal(const mdns_string_t left, const char *right) {
    const size_t right_length = strlen(right);
    if (left.length != right_length) return 0;
    for (size_t i = 0; i < left.length; ++i) {
        if (tolower((unsigned char) left.str[i]) != tolower((unsigned char) right[i])) return 0;
    }
    return 1;
}

static int string_ends_with(const char *value, const char *suffix) {
    const size_t value_length = strlen(value);
    const size_t suffix_length = strlen(suffix);
    return value_length >= suffix_length && strcasecmp(value + value_length - suffix_length, suffix) == 0;
}

static int valid_host_label(const char *label) {
    const size_t length = strlen(label);
    if (!length || length > 63 || label[0] == '-' || label[length - 1] == '-') return 0;
    for (const unsigned char *p = (const unsigned char *) label; *p; ++p) {
        if (!isalnum(*p) && *p != '-') return 0;
    }
    return 1;
}

static int set_host_name(struct server *server, const char *requested) {
    char label[64];
    size_t length = strlen(requested);

    while (length && requested[length - 1] == '.') --length;
    if (length > 6 && strncasecmp(requested + length - 6, ".local", 6) == 0) length -= 6;
    if (!length || length >= sizeof(label)) return -1;

    memcpy(label, requested, length);
    label[length] = '\0';
    for (size_t i = 0; label[i]; ++i) label[i] = (char) tolower((unsigned char) label[i]);
    if (!valid_host_label(label)) return -1;

    snprintf(server->host_label, sizeof(server->host_label), "%s", label);
    snprintf(server->host_name, sizeof(server->host_name), "%s.local.", label);
    return 0;
}

static int valid_service_type(const char *type) {
    const size_t length = strlen(type);
    const char *transport = NULL;
    if (length > 5 && string_ends_with(type, "._tcp")) transport = type + length - 5;
    if (length > 5 && string_ends_with(type, "._udp")) transport = type + length - 5;
    if (!transport || type[0] != '_' || transport == type) return 0;
    for (const unsigned char *p = (const unsigned char *) type; p < (const unsigned char *) transport; ++p) {
        if (!isalnum(*p) && *p != '_' && *p != '-') return 0;
    }
    return 1;
}

static int add_service(struct server *server, const char *specification) {
    if (server->service_count >= MUDNS_MAX_SERVICES) return -1;

    char spec[MUDNS_NAME_SIZE * 2];
    if (strlen(specification) >= sizeof(spec)) return -1;
    snprintf(spec, sizeof(spec), "%s", specification);

    char *port_text = strchr(spec, ':');
    if (!port_text) return -1;
    *port_text++ = '\0';
    char *instance = strchr(port_text, ':');
    if (instance) *instance++ = '\0';

    size_t type_length = strlen(spec);
    while (type_length && spec[type_length - 1] == '.') spec[--type_length] = '\0';
    if (string_ends_with(spec, ".local")) spec[type_length -= 6] = '\0';
    for (size_t i = 0; spec[i]; ++i) spec[i] = (char) tolower((unsigned char) spec[i]);
    if (!valid_service_type(spec)) return -1;

    errno = 0;
    char *port_end = NULL;
    const long port = strtol(port_text, &port_end, 10);
    if (errno || !port_end || *port_end || port < 1 || port > 65535) return -1;

    const char *instance_source = (!instance || !*instance) ? server->host_label : instance;
    if (strlen(instance_source) > 63 || strchr(instance_source, '.') || strchr(instance_source, ':')) return -1;
    char instance_label[64];
    snprintf(instance_label, sizeof(instance_label), "%s", instance_source);
    for (const unsigned char *p = (const unsigned char *) instance_label; *p; ++p) {
        if (*p < 0x20 || *p == 0x7f) return -1;
    }

    char type_name[MUDNS_NAME_SIZE];
    char instance_name[MUDNS_NAME_SIZE];
    if (snprintf(type_name, sizeof(type_name), "%s.local.", spec) >= (int) sizeof(type_name)) return -1;
    if (snprintf(instance_name, sizeof(instance_name), "%s.%s", instance_label, type_name)
        >= (int) sizeof(instance_name))
        return -1;

    for (size_t i = 0; i < server->service_count; ++i) {
        if (!strcasecmp(server->services[i].instance, instance_name)) return -1;
    }

    struct service *service = &server->services[server->service_count];
    snprintf(service->type, sizeof(service->type), "%s", type_name);
    snprintf(service->instance, sizeof(service->instance), "%s", instance_name);
    service->port = (uint16_t) port;
    ++server->service_count;
    return 0;
}

static int address_compare(const void *left_ptr, const void *right_ptr) {
    const struct address_desc *left = left_ptr;
    const struct address_desc *right = right_ptr;
    int result = strcmp(left->interface, right->interface);
    if (result) return result;

    result = left->address.ss_family - right->address.ss_family;
    if (result) return result;
    if (left->address.ss_family == AF_INET) {
        const struct sockaddr_in *left4 = (const struct sockaddr_in *) &left->address;
        const struct sockaddr_in *right4 = (const struct sockaddr_in *) &right->address;
        return memcmp(&left4->sin_addr, &right4->sin_addr, sizeof(left4->sin_addr));
    }

    const struct sockaddr_in6 *left6 = (const struct sockaddr_in6 *) &left->address;
    const struct sockaddr_in6 *right6 = (const struct sockaddr_in6 *) &right->address;
    result = memcmp(&left6->sin6_addr, &right6->sin6_addr, sizeof(left6->sin6_addr));
    if (result) return result;
    if (left6->sin6_scope_id < right6->sin6_scope_id) return -1;
    return left6->sin6_scope_id > right6->sin6_scope_id;
}

static size_t collect_addresses(const struct server *server, struct address_desc *addresses) {
    struct ifaddrs *interfaces = NULL;
    if (getifaddrs(&interfaces) < 0) {
        fprintf(stderr, "mudns: cannot read network interfaces: %s\n", strerror(errno));
        return 0;
    }

    size_t count = 0;
    for (const struct ifaddrs *item = interfaces; item && count < MUDNS_MAX_ENDPOINTS; item = item->ifa_next) {
        if (!item->ifa_addr || !(item->ifa_flags & IFF_UP) || !(item->ifa_flags & IFF_MULTICAST)
            || (item->ifa_flags & IFF_LOOPBACK) || (item->ifa_flags & IFF_POINTOPOINT))
            continue;
        if (server->interface[0] && strcmp(server->interface, item->ifa_name)) continue;

        const int family = item->ifa_addr->sa_family;
        if (family != AF_INET && family != AF_INET6) continue;
        if (family == AF_INET) {
            const struct sockaddr_in *address = (const struct sockaddr_in *) item->ifa_addr;
            if (address->sin_addr.s_addr == htonl(INADDR_ANY) || address->sin_addr.s_addr == htonl(INADDR_LOOPBACK))
                continue;
        } else {
            const struct sockaddr_in6 *address = (const struct sockaddr_in6 *) item->ifa_addr;
            if (IN6_IS_ADDR_UNSPECIFIED(&address->sin6_addr) || IN6_IS_ADDR_LOOPBACK(&address->sin6_addr)) continue;
        }

        struct address_desc *description = &addresses[count];
        memset(description, 0, sizeof(*description));
        snprintf(description->interface, sizeof(description->interface), "%s", item->ifa_name);
        description->address_size = family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
        memcpy(&description->address, item->ifa_addr, description->address_size);
        if (family == AF_INET) {
            ((struct sockaddr_in *) &description->address)->sin_port = 0;
        } else {
            struct sockaddr_in6 *address = (struct sockaddr_in6 *) &description->address;
            address->sin6_port = 0;
            if (!address->sin6_scope_id) address->sin6_scope_id = if_nametoindex(item->ifa_name);
        }
        ++count;
    }
    freeifaddrs(interfaces);

    qsort(addresses, count, sizeof(*addresses), address_compare);
    size_t unique = 0;
    for (size_t i = 0; i < count; ++i) {
        if (unique && !address_compare(&addresses[unique - 1], &addresses[i])) continue;
        if (unique != i) addresses[unique] = addresses[i];
        ++unique;
    }
    return unique;
}

static int endpoint_matches(const struct endpoint *endpoint, const struct address_desc *address) {
    return !address_compare(&endpoint->desc, address);
}

static mdns_record_t address_record(const struct server *server, const struct endpoint *endpoint) {
    mdns_record_t record;
    memset(&record, 0, sizeof(record));
    record.name = string_ref(server->host_name);
    if (endpoint->desc.address.ss_family == AF_INET) {
        record.type = MDNS_RECORDTYPE_A;
        record.data.a.addr = *(const struct sockaddr_in *) &endpoint->desc.address;
    } else {
        record.type = MDNS_RECORDTYPE_AAAA;
        record.data.aaaa.addr = *(const struct sockaddr_in6 *) &endpoint->desc.address;
    }
    return record;
}

static mdns_record_t ptr_record(const struct service *service) {
    mdns_record_t record;
    memset(&record, 0, sizeof(record));
    record.name = string_ref(service->type);
    record.type = MDNS_RECORDTYPE_PTR;
    record.data.ptr.name = string_ref(service->instance);
    return record;
}

static mdns_record_t srv_record(const struct server *server, const struct service *service) {
    mdns_record_t record;
    memset(&record, 0, sizeof(record));
    record.name = string_ref(service->instance);
    record.type = MDNS_RECORDTYPE_SRV;
    record.data.srv.port = service->port;
    record.data.srv.name = string_ref(server->host_name);
    return record;
}

static mdns_record_t txt_record(const struct service *service) {
    mdns_record_t record;
    memset(&record, 0, sizeof(record));
    record.name = string_ref(service->instance);
    record.type = MDNS_RECORDTYPE_TXT;
    record.data.txt.key = string_ref("txtvers");
    record.data.txt.value = string_ref("1");
    return record;
}

static void send_response(const struct callback_context *context, const struct sockaddr *from, const size_t addrlen,
                          const uint16_t query_id, const uint16_t query_type, const uint16_t query_class,
                          const mdns_string_t query_name, const mdns_record_t answer,
                          const mdns_record_t *additional, const size_t additional_count) {
    struct server *server = context->server;
    int result;
    if (query_class & MDNS_UNICAST_RESPONSE) {
        result = mdns_query_answer_unicast(
            context->endpoint->socket, from, addrlen, server->send_buffer, sizeof(server->send_buffer), query_id,
            (mdns_record_type_t) query_type, query_name.str, query_name.length, answer, NULL, 0, additional,
            additional_count
        );
    } else {
        result = mdns_query_answer_multicast(
            context->endpoint->socket, server->send_buffer, sizeof(server->send_buffer), answer, NULL, 0, additional,
            additional_count
        );
    }
    if (result < 0)
        fprintf(stderr, "mudns: could not answer %.*s: %s\n", (int) query_name.length, query_name.str,
                strerror(errno));
    else if (server->verbose)
        fprintf(stderr, "mudns: answered %.*s via %s\n", (int) query_name.length, query_name.str,
                (query_class & MDNS_UNICAST_RESPONSE) ? "unicast" : "multicast");
}

static int query_callback(int socket, const struct sockaddr *from, const size_t addrlen, const mdns_entry_type_t entry,
                          const uint16_t query_id, const uint16_t rtype, const uint16_t rclass, const uint32_t ttl,
                          const void *data, const size_t size, const size_t name_offset, const size_t name_length,
                          const size_t record_offset, const size_t record_length, void *user_data) {
    (void) socket;
    (void) ttl;
    (void) name_length;
    (void) record_offset;
    (void) record_length;
    if (entry != MDNS_ENTRYTYPE_QUESTION) return 0;

    struct callback_context *context = user_data;
    struct server *server = context->server;
    char name_buffer[MUDNS_NAME_SIZE];
    size_t offset = name_offset;
    const mdns_string_t name = mdns_string_extract(data, size, &offset, name_buffer, sizeof(name_buffer));
    if (!name.length) return 0;

    if (server->verbose) fprintf(stderr, "mudns: query %.*s type %u on %s\n", (int) name.length, name.str, rtype,
                                 context->endpoint->desc.interface);

    if (dns_equal(name, "_services._dns-sd._udp.local.")
        && (rtype == MDNS_RECORDTYPE_PTR || rtype == MDNS_RECORDTYPE_ANY)) {
        for (size_t i = 0; i < server->service_count; ++i) {
            int already_sent = 0;
            for (size_t j = 0; j < i; ++j) already_sent |= !strcasecmp(server->services[j].type, server->services[i].type);
            if (already_sent) continue;
            mdns_record_t answer;
            memset(&answer, 0, sizeof(answer));
            answer.name = name;
            answer.type = MDNS_RECORDTYPE_PTR;
            answer.data.ptr.name = string_ref(server->services[i].type);
            send_response(context, from, addrlen, query_id, rtype, rclass, name, answer, NULL, 0);
        }
        return 0;
    }

    for (size_t i = 0; i < server->service_count; ++i) {
        const struct service *service = &server->services[i];
        if (dns_equal(name, service->type) && (rtype == MDNS_RECORDTYPE_PTR || rtype == MDNS_RECORDTYPE_ANY)) {
            const mdns_record_t answer = ptr_record(service);
            const mdns_record_t additional[] = {
                srv_record(server, service), address_record(server, context->endpoint), txt_record(service)
            };
            send_response(context, from, addrlen, query_id, rtype, rclass, name, answer, additional,
                          sizeof(additional) / sizeof(additional[0]));
            continue;
        }
        if (!dns_equal(name, service->instance)) continue;

        mdns_record_t additional[3];
        size_t additional_count = 0;
        mdns_record_t answer;
        if (rtype == MDNS_RECORDTYPE_TXT) {
            answer = txt_record(service);
            additional[additional_count++] = srv_record(server, service);
            additional[additional_count++] = address_record(server, context->endpoint);
        } else if (rtype == MDNS_RECORDTYPE_SRV || rtype == MDNS_RECORDTYPE_ANY) {
            answer = srv_record(server, service);
            additional[additional_count++] = address_record(server, context->endpoint);
            additional[additional_count++] = txt_record(service);
        } else {
            continue;
        }
        send_response(context, from, addrlen, query_id, rtype, rclass, name, answer, additional, additional_count);
    }

    if (dns_equal(name, server->host_name)) {
        const mdns_record_t answer = address_record(server, context->endpoint);
        if (rtype == answer.type || rtype == MDNS_RECORDTYPE_ANY)
            send_response(context, from, addrlen, query_id, rtype, rclass, name, answer, NULL, 0);
    }
    return 0;
}

static void publish_endpoint(struct server *server, struct endpoint *endpoint, const int goodbye) {
    mdns_record_t address = address_record(server, endpoint);
    if (goodbye)
        (void) mdns_goodbye_multicast(endpoint->socket, server->send_buffer, sizeof(server->send_buffer), address,
                                      NULL, 0, NULL, 0);
    else
        (void) mdns_announce_multicast(endpoint->socket, server->send_buffer, sizeof(server->send_buffer), address,
                                       NULL, 0, NULL, 0);

    for (size_t i = 0; i < server->service_count; ++i) {
        const mdns_record_t answer = ptr_record(&server->services[i]);
        const mdns_record_t additional[] = {
            srv_record(server, &server->services[i]), address, txt_record(&server->services[i])
        };
        if (goodbye)
            (void) mdns_goodbye_multicast(endpoint->socket, server->send_buffer, sizeof(server->send_buffer), answer,
                                          NULL, 0, additional, sizeof(additional) / sizeof(additional[0]));
        else
            (void) mdns_announce_multicast(endpoint->socket, server->send_buffer, sizeof(server->send_buffer), answer,
                                           NULL, 0, additional, sizeof(additional) / sizeof(additional[0]));
    }
}

static void close_endpoints(struct server *server, const int goodbye) {
    for (size_t i = 0; i < server->endpoint_count; ++i) {
        if (goodbye) publish_endpoint(server, &server->endpoints[i], 1);
        mdns_socket_close(server->endpoints[i].socket);
    }
    server->endpoint_count = 0;
}

static int addresses_unchanged(const struct server *server, const struct address_desc *addresses, const size_t count) {
    if (count != server->endpoint_count) return 0;
    for (size_t i = 0; i < count; ++i) {
        if (!endpoint_matches(&server->endpoints[i], &addresses[i])) return 0;
    }
    return 1;
}

static void refresh_endpoints(struct server *server, const int force) {
    struct address_desc addresses[MUDNS_MAX_ENDPOINTS];
    const size_t count = collect_addresses(server, addresses);
    if (!force && addresses_unchanged(server, addresses, count)) return;

    close_endpoints(server, 1);
    for (size_t i = 0; i < count; ++i) {
        struct address_desc bind_address = addresses[i];
        int socket = -1;
        if (bind_address.address.ss_family == AF_INET) {
            struct sockaddr_in *address = (struct sockaddr_in *) &bind_address.address;
            address->sin_port = htons(MDNS_PORT);
            socket = mdns_socket_open_ipv4(address);
        } else {
            struct sockaddr_in6 *address = (struct sockaddr_in6 *) &bind_address.address;
            address->sin6_port = htons(MDNS_PORT);
            socket = mdns_socket_open_ipv6(address);
        }
        if (socket < 0) {
            fprintf(stderr, "mudns: cannot open %s mDNS socket: %s\n", addresses[i].interface, strerror(errno));
            continue;
        }
#ifdef SO_BINDTODEVICE
        if (setsockopt(socket, SOL_SOCKET, SO_BINDTODEVICE, addresses[i].interface,
                       (socklen_t) (strlen(addresses[i].interface) + 1)) < 0
            && server->verbose)
            fprintf(stderr, "mudns: cannot pin socket to %s: %s\n", addresses[i].interface, strerror(errno));
#endif
        struct endpoint *endpoint = &server->endpoints[server->endpoint_count++];
        endpoint->desc = addresses[i];
        endpoint->socket = socket;
        publish_endpoint(server, endpoint, 0);

        if (server->verbose) {
            char address_text[INET6_ADDRSTRLEN];
            const void *raw_address = addresses[i].address.ss_family == AF_INET
                                          ? (const void *) &((struct sockaddr_in *) &addresses[i].address)->sin_addr
                                          : (const void *) &((struct sockaddr_in6 *) &addresses[i].address)->sin6_addr;
            inet_ntop(addresses[i].address.ss_family, raw_address, address_text, sizeof(address_text));
            fprintf(stderr, "mudns: serving %s on %s (%s)\n", server->host_name, addresses[i].interface,
                    address_text);
        }
    }
}

static int64_t monotonic_ms(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return 0;
    return (int64_t) now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static void signal_handler(const int signal_number) {
    if (signal_number == SIGHUP)
        reload_interfaces = 1;
    else
        running = 0;
}

static void print_usage(FILE *stream) {
    fprintf(stream,
            "Usage: mudns [options]\n"
            "  -n, --hostname LABEL          publish LABEL.local (default: system hostname)\n"
            "  -i, --interface NAME         restrict service to one interface\n"
            "  -s, --service TYPE:PORT[:INSTANCE]\n"
            "                               advertise a DNS-SD service (repeatable)\n"
            "  -c, --check                  validate and list addresses without listening\n"
            "  -v, --verbose                log queries and interface changes\n"
            "  -V, --version                print version\n"
            "  -h, --help                   show this help\n");
}

static int check_configuration(const struct server *server) {
    struct address_desc addresses[MUDNS_MAX_ENDPOINTS];
    const size_t count = collect_addresses(server, addresses);
    printf("address=%s\n", server->host_name);
    for (size_t i = 0; i < server->service_count; ++i)
        printf("service=%s port=%u\n", server->services[i].instance, server->services[i].port);
    for (size_t i = 0; i < count; ++i) {
        char address_text[INET6_ADDRSTRLEN];
        const void *raw_address = addresses[i].address.ss_family == AF_INET
                                      ? (const void *) &((struct sockaddr_in *) &addresses[i].address)->sin_addr
                                      : (const void *) &((struct sockaddr_in6 *) &addresses[i].address)->sin6_addr;
        if (inet_ntop(addresses[i].address.ss_family, raw_address, address_text, sizeof(address_text)))
            printf("interface=%s address=%s\n", addresses[i].interface, address_text);
    }
    return 0;
}

int main(const int argc, char **argv) {
    struct server server;
    memset(&server, 0, sizeof(server));

    char system_hostname[64] = "muos";
    if (gethostname(system_hostname, sizeof(system_hostname) - 1) < 0) snprintf(system_hostname, sizeof(system_hostname), "muos");
    system_hostname[sizeof(system_hostname) - 1] = '\0';
    if (set_host_name(&server, system_hostname) < 0) (void) set_host_name(&server, "muos");

    int check_only = 0;
    const char *service_specifications[MUDNS_MAX_SERVICES];
    size_t service_specification_count = 0;
    const struct option options[] = {
        {"hostname", required_argument, NULL, 'n'}, {"interface", required_argument, NULL, 'i'},
        {"service", required_argument, NULL, 's'}, {"check", no_argument, NULL, 'c'},
        {"verbose", no_argument, NULL, 'v'},       {"version", no_argument, NULL, 'V'},
        {"help", no_argument, NULL, 'h'},          {NULL, 0, NULL, 0}
    };

    int option;
    while ((option = getopt_long(argc, argv, "n:i:s:cvVh", options, NULL)) != -1) {
        switch (option) {
            case 'n':
                if (set_host_name(&server, optarg) < 0) {
                    fprintf(stderr, "mudns: invalid local name '%s'\n", optarg);
                    return 2;
                }
                break;
            case 'i':
                if (!*optarg || strlen(optarg) >= sizeof(server.interface)) {
                    fprintf(stderr, "mudns: invalid interface name\n");
                    return 2;
                }
                snprintf(server.interface, sizeof(server.interface), "%s", optarg);
                break;
            case 's':
                if (service_specification_count >= MUDNS_MAX_SERVICES) {
                    fprintf(stderr, "mudns: too many services\n");
                    return 2;
                }
                service_specifications[service_specification_count++] = optarg;
                break;
            case 'c': check_only = 1; break;
            case 'v': server.verbose = 1; break;
            case 'V': printf("mudns %s (mjansson/mdns a569c475)\n", MUDNS_VERSION); return 0;
            case 'h': print_usage(stdout); return 0;
            default: print_usage(stderr); return 2;
        }
    }
    if (optind != argc) {
        print_usage(stderr);
        return 2;
    }
    for (size_t i = 0; i < service_specification_count; ++i) {
        if (add_service(&server, service_specifications[i]) < 0) {
            fprintf(stderr, "mudns: invalid or duplicate service '%s'\n", service_specifications[i]);
            return 2;
        }
    }
    if (check_only) return check_configuration(&server);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGHUP, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    refresh_endpoints(&server, 1);
    int64_t next_rescan = monotonic_ms() + MUDNS_RESCAN_MS;
    while (running) {
        struct pollfd poll_descriptors[MUDNS_MAX_ENDPOINTS];
        for (size_t i = 0; i < server.endpoint_count; ++i) {
            poll_descriptors[i].fd = server.endpoints[i].socket;
            poll_descriptors[i].events = POLLIN;
            poll_descriptors[i].revents = 0;
        }

        const int poll_result = poll(poll_descriptors, server.endpoint_count, 1000);
        if (poll_result < 0 && errno != EINTR) {
            fprintf(stderr, "mudns: poll failed: %s\n", strerror(errno));
            break;
        }
        if (poll_result > 0) {
            for (size_t i = 0; i < server.endpoint_count; ++i) {
                if (poll_descriptors[i].revents & POLLIN) {
                    struct callback_context context = {&server, &server.endpoints[i]};
                    (void) mdns_socket_listen(server.endpoints[i].socket, server.recv_buffer,
                                              sizeof(server.recv_buffer), query_callback, &context);
                }
                if (poll_descriptors[i].revents & (POLLERR | POLLHUP | POLLNVAL)) reload_interfaces = 1;
            }
        }

        const int64_t now = monotonic_ms();
        if (reload_interfaces || now >= next_rescan) {
            refresh_endpoints(&server, reload_interfaces != 0);
            reload_interfaces = 0;
            next_rescan = now + MUDNS_RESCAN_MS;
        }
    }

    close_endpoints(&server, 1);
    return 0;
}
