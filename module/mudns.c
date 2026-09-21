#define _DEFAULT_SOURCE

#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <ifaddrs.h>
#include <limits.h>
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

#define MUDNS_VERSION       "0.2.0"
#define MUDNS_MAX_ENDPOINTS 32
#define MUDNS_MAX_SERVICES  16
#define MUDNS_MAX_DEVICES   32
#define MUDNS_PACKET_SIZE   2048
#define MUDNS_NAME_SIZE     256
#define MUDNS_RESCAN_MS     5000
#define MUDNS_DISCOVERY_MS  5000
#define MUDNS_DEVICE_TTL_MS 20000
#define MUDNS_PROBE_MS      200
#define MUDNS_PROBE_COUNT   3

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
    char label[64];
    uint16_t port;
    int dashboard;
};

struct discovered_device {
    char instance[MUDNS_NAME_SIZE];
    char host[MUDNS_NAME_SIZE];
    char address[INET_ADDRSTRLEN];
    uint16_t port;
    int64_t seen_at;
};

struct server {
    char requested_label[64];
    char host_label[64];
    char host_name[MUDNS_NAME_SIZE];
    char interface[IF_NAMESIZE];
    char name_file[PATH_MAX];
    char discovery_file[PATH_MAX];
    struct endpoint endpoints[MUDNS_MAX_ENDPOINTS];
    size_t endpoint_count;
    struct service services[MUDNS_MAX_SERVICES];
    size_t service_count;
    struct discovered_device devices[MUDNS_MAX_DEVICES];
    size_t device_count;
    unsigned name_suffix;
    int probing;
    int name_conflict;
    int discovery_dirty;
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

static int64_t monotonic_ms(void);

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

    while (length && requested[length - 1] == '.')
        --length;
    if (length > 6 && strncasecmp(requested + length - 6, ".local", 6) == 0) length -= 6;
    if (!length || length >= sizeof(label)) return -1;

    memcpy(label, requested, length);
    label[length] = '\0';
    for (size_t i = 0; label[i]; ++i)
        label[i] = (char) tolower((unsigned char) label[i]);
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
    while (type_length && spec[type_length - 1] == '.')
        spec[--type_length] = '\0';
    if (string_ends_with(spec, ".local")) spec[type_length -= 6] = '\0';
    for (size_t i = 0; spec[i]; ++i)
        spec[i] = (char) tolower((unsigned char) spec[i]);
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
    snprintf(service->label, sizeof(service->label), "%s", instance_label);
    service->port = (uint16_t) port;
    service->dashboard = !strcasecmp(instance_label, "MustardOS") && !strcasecmp(type_name, "_http._tcp.local.");
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

static int dns_copy(const mdns_string_t value, char *out, const size_t out_size) {
    if (!value.length || value.length >= out_size) return 0;
    memcpy(out, value.str, value.length);
    out[value.length] = '\0';
    return 1;
}

static int dashboard_instance(const char *instance) {
    static const char suffix[] = "._http._tcp.local.";
    const size_t length = strlen(instance);
    const size_t suffix_length = sizeof(suffix) - 1;
    if (length <= suffix_length || strcasecmp(instance + length - suffix_length, suffix)) return 0;

    const size_t label_length = length - suffix_length;
    static const char legacy[] = "MustardOS";
    static const char named[] = "MustardOS on ";
    return (label_length == sizeof(legacy) - 1 && !strncasecmp(instance, legacy, label_length))
           || (label_length > sizeof(named) - 1 && !strncasecmp(instance, named, sizeof(named) - 1));
}

static struct discovered_device *device_for_instance(struct server *server, const char *instance) {
    for (size_t i = 0; i < server->device_count; ++i) {
        if (!strcasecmp(server->devices[i].instance, instance)) return &server->devices[i];
    }
    if (server->device_count >= MUDNS_MAX_DEVICES) return NULL;

    struct discovered_device *device = &server->devices[server->device_count++];
    memset(device, 0, sizeof(*device));
    snprintf(device->instance, sizeof(device->instance), "%s", instance);
    server->discovery_dirty = 1;
    return device;
}

static int is_own_ipv4(const struct server *server, const struct in_addr address) {
    for (size_t i = 0; i < server->endpoint_count; ++i) {
        if (server->endpoints[i].desc.address.ss_family != AF_INET) continue;
        const struct sockaddr_in *local = (const struct sockaddr_in *) &server->endpoints[i].desc.address;
        if (local->sin_addr.s_addr == address.s_addr) return 1;
    }
    return 0;
}

static int is_own_ipv6(const struct server *server, const struct in6_addr *address) {
    for (size_t i = 0; i < server->endpoint_count; ++i) {
        if (server->endpoints[i].desc.address.ss_family != AF_INET6) continue;
        const struct sockaddr_in6 *local = (const struct sockaddr_in6 *) &server->endpoints[i].desc.address;
        if (!memcmp(&local->sin6_addr, address, sizeof(*address))) return 1;
    }
    return 0;
}

static int foreign_ipv4_wins(const struct server *server, const struct in_addr address) {
    uint32_t smallest = UINT32_MAX;
    int found = 0;
    for (size_t i = 0; i < server->endpoint_count; ++i) {
        if (server->endpoints[i].desc.address.ss_family != AF_INET) continue;
        const struct sockaddr_in *local = (const struct sockaddr_in *) &server->endpoints[i].desc.address;
        const uint32_t value = ntohl(local->sin_addr.s_addr);
        if (!found || value < smallest) smallest = value;
        found = 1;
    }
    return !found || ntohl(address.s_addr) < smallest;
}

static int has_own_ipv4(const struct server *server) {
    for (size_t i = 0; i < server->endpoint_count; ++i) {
        if (server->endpoints[i].desc.address.ss_family == AF_INET) return 1;
    }
    return 0;
}

static int foreign_ipv6_wins(const struct server *server, const struct in6_addr *address) {
    const struct in6_addr *smallest = NULL;

    for (size_t i = 0; i < server->endpoint_count; ++i) {
        if (server->endpoints[i].desc.address.ss_family != AF_INET6) continue;
        const struct sockaddr_in6 *local = (const struct sockaddr_in6 *) &server->endpoints[i].desc.address;
        if (!smallest || memcmp(&local->sin6_addr, smallest, sizeof(*smallest)) < 0) smallest = &local->sin6_addr;
    }

    return !smallest || memcmp(address, smallest, sizeof(*address)) < 0;
}

static void observe_record(
    struct server *server, const mdns_entry_type_t entry, const uint16_t rtype, const uint32_t ttl, const void *data,
    const size_t size, const size_t name_offset, const size_t record_offset, const size_t record_length
) {
    char name_buffer[MUDNS_NAME_SIZE];
    size_t name_cursor = name_offset;
    const mdns_string_t parsed_name = mdns_string_extract(data, size, &name_cursor, name_buffer, sizeof(name_buffer));
    char name[MUDNS_NAME_SIZE];
    if (!dns_copy(parsed_name, name, sizeof(name))) return;

    if ((entry == MDNS_ENTRYTYPE_ANSWER || entry == MDNS_ENTRYTYPE_AUTHORITY || entry == MDNS_ENTRYTYPE_ADDITIONAL)
        && !strcasecmp(name, server->host_name)) {
        int foreign = 0;
        int foreign_wins = 0;
        if (rtype == MDNS_RECORDTYPE_A) {
            struct sockaddr_in address;
            memset(&address, 0, sizeof(address));
            mdns_record_parse_a(data, size, record_offset, record_length, &address);
            foreign = !is_own_ipv4(server, address.sin_addr);
            foreign_wins = foreign_ipv4_wins(server, address.sin_addr);
        } else if (rtype == MDNS_RECORDTYPE_AAAA) {
            struct sockaddr_in6 address;
            memset(&address, 0, sizeof(address));
            mdns_record_parse_aaaa(data, size, record_offset, record_length, &address);
            foreign = !is_own_ipv6(server, &address.sin6_addr);
            foreign_wins = !has_own_ipv4(server) && foreign_ipv6_wins(server, &address.sin6_addr);
        }
        if (foreign && (server->probing || foreign_wins)) server->name_conflict = 1;
    }

    if (entry == MDNS_ENTRYTYPE_QUESTION) return;

    const int64_t now = monotonic_ms();
    if (rtype == MDNS_RECORDTYPE_PTR && !strcasecmp(name, "_http._tcp.local.")) {
        char target_buffer[MUDNS_NAME_SIZE];
        const mdns_string_t parsed =
            mdns_record_parse_ptr(data, size, record_offset, record_length, target_buffer, sizeof(target_buffer));
        char target[MUDNS_NAME_SIZE];
        if (!dns_copy(parsed, target, sizeof(target)) || !dashboard_instance(target)) return;
        struct discovered_device *device = device_for_instance(server, target);
        if (!device) return;
        device->seen_at = ttl ? now : 0;
        server->discovery_dirty = 1;
        return;
    }

    if (rtype == MDNS_RECORDTYPE_SRV && dashboard_instance(name)) {
        char target_buffer[MUDNS_NAME_SIZE];
        const mdns_record_srv_t parsed =
            mdns_record_parse_srv(data, size, record_offset, record_length, target_buffer, sizeof(target_buffer));
        char target[MUDNS_NAME_SIZE];
        if (!dns_copy(parsed.name, target, sizeof(target))) return;
        struct discovered_device *device = device_for_instance(server, name);
        if (!device) return;
        snprintf(device->host, sizeof(device->host), "%s", target);
        device->port = parsed.port;
        device->seen_at = ttl ? now : 0;
        server->discovery_dirty = 1;
        return;
    }

    if (rtype == MDNS_RECORDTYPE_A) {
        struct sockaddr_in parsed;
        memset(&parsed, 0, sizeof(parsed));
        mdns_record_parse_a(data, size, record_offset, record_length, &parsed);
        char address[INET_ADDRSTRLEN] = "";
        inet_ntop(AF_INET, &parsed.sin_addr, address, sizeof(address));
        for (size_t i = 0; i < server->device_count; ++i) {
            if (strcasecmp(server->devices[i].host, name)) continue;
            snprintf(server->devices[i].address, sizeof(server->devices[i].address), "%s", address);
            server->devices[i].seen_at = ttl ? now : 0;
            server->discovery_dirty = 1;
        }
    }
}

static void send_response(
    const struct callback_context *context, const struct sockaddr *from, const size_t addrlen, const uint16_t query_id,
    const uint16_t query_type, const uint16_t query_class, const mdns_string_t query_name, const mdns_record_t answer,
    const mdns_record_t *additional, const size_t additional_count
) {
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
        fprintf(stderr, "mudns: could not answer %.*s: %s\n", (int) query_name.length, query_name.str, strerror(errno));
    else if (server->verbose)
        fprintf(
            stderr, "mudns: answered %.*s via %s\n", (int) query_name.length, query_name.str,
            (query_class & MDNS_UNICAST_RESPONSE) ? "unicast" : "multicast"
        );
}

static int query_callback(
    int socket, const struct sockaddr *from, const size_t addrlen, const mdns_entry_type_t entry,
    const uint16_t query_id, const uint16_t rtype, const uint16_t rclass, const uint32_t ttl, const void *data,
    const size_t size, const size_t name_offset, const size_t name_length, const size_t record_offset,
    const size_t record_length, void *user_data
) {
    (void) socket;
    (void) name_length;

    struct callback_context *context = user_data;
    struct server *server = context->server;
    observe_record(server, entry, rtype, ttl, data, size, name_offset, record_offset, record_length);
    if (entry != MDNS_ENTRYTYPE_QUESTION || server->probing) return 0;

    char name_buffer[MUDNS_NAME_SIZE];
    size_t offset = name_offset;
    const mdns_string_t name = mdns_string_extract(data, size, &offset, name_buffer, sizeof(name_buffer));
    if (!name.length) return 0;

    if (server->verbose)
        fprintf(
            stderr, "mudns: query %.*s type %u on %s\n", (int) name.length, name.str, rtype,
            context->endpoint->desc.interface
        );

    if (dns_equal(name, "_services._dns-sd._udp.local.")
        && (rtype == MDNS_RECORDTYPE_PTR || rtype == MDNS_RECORDTYPE_ANY)) {
        for (size_t i = 0; i < server->service_count; ++i) {
            int already_sent = 0;
            for (size_t j = 0; j < i; ++j)
                already_sent |= !strcasecmp(server->services[j].type, server->services[i].type);
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
            send_response(
                context, from, addrlen, query_id, rtype, rclass, name, answer, additional,
                sizeof(additional) / sizeof(additional[0])
            );
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
        (void) mdns_goodbye_multicast(
            endpoint->socket, server->send_buffer, sizeof(server->send_buffer), address, NULL, 0, NULL, 0
        );
    else
        (void) mdns_announce_multicast(
            endpoint->socket, server->send_buffer, sizeof(server->send_buffer), address, NULL, 0, NULL, 0
        );

    for (size_t i = 0; i < server->service_count; ++i) {
        const mdns_record_t answer = ptr_record(&server->services[i]);
        const mdns_record_t additional[] = {
            srv_record(server, &server->services[i]), address, txt_record(&server->services[i])
        };
        if (goodbye)
            (void) mdns_goodbye_multicast(
                endpoint->socket, server->send_buffer, sizeof(server->send_buffer), answer, NULL, 0, additional,
                sizeof(additional) / sizeof(additional[0])
            );
        else
            (void) mdns_announce_multicast(
                endpoint->socket, server->send_buffer, sizeof(server->send_buffer), answer, NULL, 0, additional,
                sizeof(additional) / sizeof(additional[0])
            );
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

static int refresh_endpoints(struct server *server, const int force, const int announce) {
    struct address_desc addresses[MUDNS_MAX_ENDPOINTS];
    const size_t count = collect_addresses(server, addresses);
    if (!force && addresses_unchanged(server, addresses, count)) return 0;

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
        if (setsockopt(
                socket, SOL_SOCKET, SO_BINDTODEVICE, addresses[i].interface,
                (socklen_t) (strlen(addresses[i].interface) + 1)
            ) < 0
            && server->verbose)
            fprintf(stderr, "mudns: cannot pin socket to %s: %s\n", addresses[i].interface, strerror(errno));
#endif
        struct endpoint *endpoint = &server->endpoints[server->endpoint_count++];
        endpoint->desc = addresses[i];
        endpoint->socket = socket;
        if (announce) publish_endpoint(server, endpoint, 0);

        if (server->verbose) {
            char address_text[INET6_ADDRSTRLEN];
            const void *raw_address = addresses[i].address.ss_family == AF_INET
                                          ? (const void *) &((struct sockaddr_in *) &addresses[i].address)->sin_addr
                                          : (const void *) &((struct sockaddr_in6 *) &addresses[i].address)->sin6_addr;
            inet_ntop(addresses[i].address.ss_family, raw_address, address_text, sizeof(address_text));
            fprintf(stderr, "mudns: serving %s on %s (%s)\n", server->host_name, addresses[i].interface, address_text);
        }
    }
    return 1;
}

static int64_t monotonic_ms(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return 0;
    return (int64_t) now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static int write_name_file(const struct server *server) {
    if (!server->name_file[0]) return 1;
    char temporary[PATH_MAX];
    if (snprintf(temporary, sizeof(temporary), "%s.tmp", server->name_file) >= (int) sizeof(temporary)) return 0;

    FILE *file = fopen(temporary, "w");
    if (!file) return 0;
    int ok = fprintf(file, "%s\n", server->host_label) > 0;
    if (fclose(file) != 0) ok = 0;
    if (!ok) {
        unlink(temporary);
        return 0;
    }
    if (rename(temporary, server->name_file) < 0) {
        unlink(temporary);
        return 0;
    }
    return 1;
}

static void refresh_service_instances(struct server *server) {
    for (size_t i = 0; i < server->service_count; ++i) {
        struct service *service = &server->services[i];
        if (service->dashboard)
            snprintf(
                service->instance, sizeof(service->instance), "MustardOS on %.49s.%s", server->host_label, service->type
            );
        else
            snprintf(service->instance, sizeof(service->instance), "%s.%s", service->label, service->type);
    }
}

static void publish_all(struct server *server) {
    for (size_t i = 0; i < server->endpoint_count; ++i)
        publish_endpoint(server, &server->endpoints[i], 0);
}

static int probe_name(struct server *server) {
    server->probing = 1;
    server->name_conflict = 0;

    for (int attempt = 0; attempt < MUDNS_PROBE_COUNT && !server->name_conflict; ++attempt) {
        for (size_t i = 0; i < server->endpoint_count; ++i)
            (void) mdns_query_send(
                server->endpoints[i].socket, MDNS_RECORDTYPE_ANY, server->host_name, strlen(server->host_name),
                server->send_buffer, sizeof(server->send_buffer), 0
            );

        const int64_t deadline = monotonic_ms() + MUDNS_PROBE_MS;
        while (!server->name_conflict) {
            const int64_t remaining = deadline - monotonic_ms();
            if (remaining <= 0) break;

            struct pollfd descriptors[MUDNS_MAX_ENDPOINTS];
            for (size_t i = 0; i < server->endpoint_count; ++i) {
                descriptors[i].fd = server->endpoints[i].socket;
                descriptors[i].events = POLLIN;
                descriptors[i].revents = 0;
            }

            const int ready = poll(descriptors, server->endpoint_count, (int) remaining);
            if (ready <= 0) continue;
            for (size_t i = 0; i < server->endpoint_count; ++i) {
                if (!(descriptors[i].revents & POLLIN)) continue;
                struct callback_context context = {server, &server->endpoints[i]};
                (void) mdns_socket_listen(
                    server->endpoints[i].socket, server->recv_buffer, sizeof(server->recv_buffer), query_callback,
                    &context
                );
            }
        }
    }

    server->probing = 0;
    return !server->name_conflict;
}

static int choose_unique_name(struct server *server) {
    for (unsigned suffix = server->name_suffix; suffix < 10000; ++suffix) {
        char suffix_text[16] = "";
        if (suffix) snprintf(suffix_text, sizeof(suffix_text), "%u", suffix);
        const size_t suffix_length = strlen(suffix_text);
        const int base_length = (int) (63 - suffix_length);

        char candidate[64];
        snprintf(candidate, sizeof(candidate), "%.*s%s", base_length, server->requested_label, suffix_text);
        if (set_host_name(server, candidate) < 0) continue;
        if (server->endpoint_count && !probe_name(server)) continue;

        server->name_suffix = suffix;
        server->name_conflict = 0;
        refresh_service_instances(server);
        if (!write_name_file(server))
            fprintf(stderr, "mudns: cannot write effective local name to %s: %s\n", server->name_file, strerror(errno));
        if (server->verbose) fprintf(stderr, "mudns: selected %s\n", server->host_name);
        return 1;
    }
    return 0;
}

static void send_discovery_query(struct server *server) {
    static const char service[] = "_http._tcp.local.";
    for (size_t i = 0; i < server->endpoint_count; ++i)
        (void) mdns_query_send(
            server->endpoints[i].socket, MDNS_RECORDTYPE_PTR, service, sizeof(service) - 1, server->send_buffer,
            sizeof(server->send_buffer), 0
        );
}

static void prune_devices(struct server *server, const int64_t now) {
    size_t kept = 0;
    for (size_t i = 0; i < server->device_count; ++i) {
        const struct discovered_device *device = &server->devices[i];
        if (!device->seen_at || now - device->seen_at > MUDNS_DEVICE_TTL_MS) {
            server->discovery_dirty = 1;
            continue;
        }
        if (kept != i) server->devices[kept] = server->devices[i];
        ++kept;
    }
    server->device_count = kept;
}

static int write_discovery_file(struct server *server) {
    if (!server->discovery_file[0]) return 1;
    char temporary[PATH_MAX];
    if (snprintf(temporary, sizeof(temporary), "%s.tmp", server->discovery_file) >= (int) sizeof(temporary)) return 0;

    FILE *file = fopen(temporary, "w");
    if (!file) return 0;
    int ok = fputs("{\"devices\":[", file) >= 0;
    int emitted = 0;
    for (size_t i = 0; ok && i < server->device_count; ++i) {
        const struct discovered_device *device = &server->devices[i];
        if (!device->host[0] || !device->port || !strcasecmp(device->host, server->host_name)) continue;

        char host[MUDNS_NAME_SIZE];
        snprintf(host, sizeof(host), "%s", device->host);
        size_t length = strlen(host);
        if (length && host[length - 1] == '.') host[--length] = '\0';

        char name[MUDNS_NAME_SIZE];
        snprintf(name, sizeof(name), "%s", host);
        if (length > 6 && !strcasecmp(name + length - 6, ".local")) name[length - 6] = '\0';

        if (emitted) ok = fputc(',', file) != EOF;
        if (ok)
            ok = fprintf(
                     file, "{\"name\":\"%s\",\"host\":\"%s\",\"address\":\"%s\",\"port\":%u}", name, host,
                     device->address, device->port
                 )
                 > 0;
        emitted = 1;
    }
    if (ok) ok = fputs("]}\n", file) >= 0;
    if (fclose(file) != 0) ok = 0;
    if (!ok || rename(temporary, server->discovery_file) < 0) {
        unlink(temporary);
        return 0;
    }
    server->discovery_dirty = 0;
    return 1;
}

static void signal_handler(const int signal_number) {
    if (signal_number == SIGHUP)
        reload_interfaces = 1;
    else
        running = 0;
}

static void print_usage(FILE *stream) {
    fprintf(
        stream, "Usage: mudns [options]\n"
                "  -n, --hostname LABEL          publish LABEL.local (default: system hostname)\n"
                "  -i, --interface NAME         restrict service to one interface\n"
                "  -s, --service TYPE:PORT[:INSTANCE]\n"
                "                               advertise a DNS-SD service (repeatable)\n"
                "  -N, --name-file PATH        write the selected local name\n"
                "  -D, --discovery-file PATH   write discovered MustardOS dashboards as JSON\n"
                "  -c, --check                  validate and list addresses without listening\n"
                "  -v, --verbose                log queries and interface changes\n"
                "  -V, --version                print version\n"
                "  -h, --help                   show this help\n"
    );
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
    if (gethostname(system_hostname, sizeof(system_hostname) - 1) < 0)
        snprintf(system_hostname, sizeof(system_hostname), "muos");
    system_hostname[sizeof(system_hostname) - 1] = '\0';
    if (set_host_name(&server, system_hostname) < 0) (void) set_host_name(&server, "muos");

    int check_only = 0;
    const char *service_specifications[MUDNS_MAX_SERVICES];
    size_t service_specification_count = 0;
    const struct option options[] = {
        {"hostname", required_argument, NULL, 'n'},
        {"interface", required_argument, NULL, 'i'},
        {"service", required_argument, NULL, 's'},
        {"name-file", required_argument, NULL, 'N'},
        {"discovery-file", required_argument, NULL, 'D'},
        {"check", no_argument, NULL, 'c'},
        {"verbose", no_argument, NULL, 'v'},
        {"version", no_argument, NULL, 'V'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    int option;
    while ((option = getopt_long(argc, argv, "n:i:s:N:D:cvVh", options, NULL)) != -1) {
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
            case 'N':
                if (!*optarg || strlen(optarg) >= sizeof(server.name_file)) return 2;
                snprintf(server.name_file, sizeof(server.name_file), "%s", optarg);
                break;
            case 'D':
                if (!*optarg || strlen(optarg) >= sizeof(server.discovery_file)) return 2;
                snprintf(server.discovery_file, sizeof(server.discovery_file), "%s", optarg);
                break;
            case 'c':
                check_only = 1;
                break;
            case 'v':
                server.verbose = 1;
                break;
            case 'V':
                printf("mudns %s (mjansson/mdns a569c475)\n", MUDNS_VERSION);
                return 0;
            case 'h':
                print_usage(stdout);
                return 0;
            default:
                print_usage(stderr);
                return 2;
        }
    }
    if (optind != argc) {
        print_usage(stderr);
        return 2;
    }
    snprintf(server.requested_label, sizeof(server.requested_label), "%s", server.host_label);
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

    refresh_endpoints(&server, 1, 0);
    if (!choose_unique_name(&server)) {
        fprintf(stderr, "mudns: could not select an unused local name\n");
        close_endpoints(&server, 0);
        return 1;
    }
    publish_all(&server);
    send_discovery_query(&server);
    server.discovery_dirty = 1;
    write_discovery_file(&server);

    int64_t next_rescan = monotonic_ms() + MUDNS_RESCAN_MS;
    int64_t next_discovery = monotonic_ms() + MUDNS_DISCOVERY_MS;
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
                    (void) mdns_socket_listen(
                        server.endpoints[i].socket, server.recv_buffer, sizeof(server.recv_buffer), query_callback,
                        &context
                    );
                }
                if (poll_descriptors[i].revents & (POLLERR | POLLHUP | POLLNVAL)) reload_interfaces = 1;
            }
        }

        const int64_t now = monotonic_ms();
        if (reload_interfaces || now >= next_rescan) {
            if (refresh_endpoints(&server, reload_interfaces != 0, 0)) {
                if (!choose_unique_name(&server)) break;
                publish_all(&server);
                server.discovery_dirty = 1;
            }
            reload_interfaces = 0;
            next_rescan = now + MUDNS_RESCAN_MS;
        }
        if (server.name_conflict && !server.probing) {
            for (size_t i = 0; i < server.endpoint_count; ++i)
                publish_endpoint(&server, &server.endpoints[i], 1);
            server.name_suffix += 1;
            if (!choose_unique_name(&server)) break;
            publish_all(&server);
            server.discovery_dirty = 1;
        }
        if (now >= next_discovery) {
            send_discovery_query(&server);
            prune_devices(&server, now);
            next_discovery = now + MUDNS_DISCOVERY_MS;
        }
        if (server.discovery_dirty) write_discovery_file(&server);
    }

    close_endpoints(&server, 1);
    if (server.name_file[0]) unlink(server.name_file);
    if (server.discovery_file[0]) unlink(server.discovery_file);
    return 0;
}
