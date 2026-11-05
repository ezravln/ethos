#include <net/ping.h>
#include <net/ethernet.h>
#include <console/vga.h>
#include <lib/memory.h>
#include <sched/scheduler.h>

#define FRAME_CAPACITY 1600U
#define DHCP_CLIENT_PORT 68U
#define DHCP_SERVER_PORT 67U
#define DNS_SERVER_PORT 53U
#define PING_COUNT 4U
#define DHCP_TIMEOUT_TICKS 400U
#define DNS_TIMEOUT_TICKS 300U
#define ARP_TIMEOUT_TICKS 200U
#define ICMP_TIMEOUT_TICKS 100U

#define ETHERTYPE_IPV4 0x0800U
#define ETHERTYPE_ARP 0x0806U
#define IP_PROTOCOL_ICMP 1U
#define IP_PROTOCOL_UDP 17U

static const uint8_t broadcast_ip[4] = {255, 255, 255, 255};
static uint8_t local_ip[4];
static uint8_t subnet_mask[4];
static uint8_t gateway_ip[4];
static uint8_t dns_ip[4];
static uint8_t local_mac[6];
static uint32_t dhcp_xid = 0x4554484FU;
static uint16_t ip_identifier = 1U;
static uint16_t dns_identifier = 0xE705U;
static bool network_configured;

static bool parse_udp_from_ip(const uint8_t *frame, size_t frame_length,
  const uint8_t **udp_out, const uint8_t **payload_out,
  size_t *payload_length_out);
static bool send_udp(const uint8_t destination_mac[6],
  const uint8_t destination_ip[4], uint16_t source_port,
  uint16_t destination_port, const uint8_t *payload, size_t payload_length);
static bool resolve_arp(const uint8_t target_ip[4], uint8_t target_mac[6]);

static uint16_t read_be16(const uint8_t *data)
{
  return (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
}

static uint32_t read_be32(const uint8_t *data)
{
  return ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
      ((uint32_t)data[2] << 8) | data[3];
}

static void write_be16(uint8_t *data, uint16_t value)
{
  data[0] = (uint8_t)(value >> 8);
  data[1] = (uint8_t)value;
}

static void write_be32(uint8_t *data, uint32_t value)
{
  data[0] = (uint8_t)(value >> 24);
  data[1] = (uint8_t)(value >> 16);
  data[2] = (uint8_t)(value >> 8);
  data[3] = (uint8_t)value;
}

static bool bytes_equal(const uint8_t *left, const uint8_t *right, size_t size)
{
  for (size_t index = 0; index < size; index++)
  {
    if (left[index] != right[index])
    {
      return false;
    }
  }
  return true;
}

static uint16_t checksum(const uint8_t *data, size_t length)
{
  uint32_t sum = 0;
  while (length >= 2U)
  {
    sum += read_be16(data);
    data += 2;
    length -= 2U;
  }
  if (length != 0)
  {
    sum += (uint16_t)data[0] << 8;
  }
  while ((sum >> 16) != 0)
  {
    sum = (sum & 0xFFFFU) + (sum >> 16);
  }
  return (uint16_t)~sum;
}

static bool poll_frame(uint8_t frame[FRAME_CAPACITY], size_t *length)
{
  return ethernet_receive_frame(frame, FRAME_CAPACITY, length);
}

static void wait_for_tick(void)
{
  __asm__ volatile ("pause");
}

static size_t build_ipv4_header(uint8_t *frame, const uint8_t destination_mac[6],
    const uint8_t source_ip[4], const uint8_t destination_ip[4],
    uint8_t protocol, uint16_t payload_length)
{
  memcpy(frame, destination_mac, 6U);
  memcpy(frame + 6U, local_mac, 6U);
  write_be16(frame + 12U, ETHERTYPE_IPV4);
  uint8_t *ip = frame + 14U;
  ip[0] = 0x45U;
  ip[1] = 0;
  write_be16(ip + 2U, (uint16_t)(20U + payload_length));
  write_be16(ip + 4U, ip_identifier++);
  write_be16(ip + 6U, 0x4000U);
  ip[8] = 64U;
  ip[9] = protocol;
  write_be16(ip + 10U, 0);
  memcpy(ip + 12U, source_ip, 4U);
  memcpy(ip + 16U, destination_ip, 4U);
  write_be16(ip + 10U, checksum(ip, 20U));
  return 34U;
}

static bool send_dhcp(uint8_t message_type, uint32_t xid,
    const uint8_t requested_ip[4], const uint8_t server_ip[4])
{
  uint8_t frame[600] = {0};
  static const uint8_t broadcast_mac[6] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
  };
  static const uint8_t zero_ip[4] = {0, 0, 0, 0};
  size_t udp_offset = build_ipv4_header(frame, broadcast_mac, zero_ip,
      broadcast_ip, IP_PROTOCOL_UDP, 0);
  uint8_t *udp = frame + udp_offset;
  uint8_t *bootp = udp + 8U;
  size_t option = 240U;

  bootp[0] = 1U;
  bootp[1] = 1U;
  bootp[2] = 6U;
  write_be32(bootp + 4U, xid);
  write_be16(bootp + 10U, 0x8000U);
  memcpy(bootp + 28U, local_mac, sizeof(local_mac));
  bootp[236] = 0x63U;
  bootp[237] = 0x82U;
  bootp[238] = 0x53U;
  bootp[239] = 0x63U;
  bootp[option++] = 53U;
  bootp[option++] = 1U;
  bootp[option++] = message_type;
  if (message_type == 3U)
  {
    bootp[option++] = 50U;
    bootp[option++] = 4U;
    memcpy(bootp + option, requested_ip, 4U);
    option += 4U;
    bootp[option++] = 54U;
    bootp[option++] = 4U;
    memcpy(bootp + option, server_ip, 4U);
    option += 4U;
  }
  bootp[option++] = 61U;
  bootp[option++] = 7U;
  bootp[option++] = 1U;
  memcpy(bootp + option, local_mac, sizeof(local_mac));
  option += sizeof(local_mac);
  bootp[option++] = 55U;
  bootp[option++] = 4U;
  bootp[option++] = 1U;
  bootp[option++] = 3U;
  bootp[option++] = 6U;
  bootp[option++] = 51U;
  bootp[option++] = 255U;

  size_t bootp_length = option;
  size_t udp_length = 8U + bootp_length;
  write_be16(udp, DHCP_CLIENT_PORT);
  write_be16(udp + 2U, DHCP_SERVER_PORT);
  write_be16(udp + 4U, (uint16_t)udp_length);
  write_be16(udp + 6U, 0);
  write_be16(frame + 16U, (uint16_t)(20U + udp_length));
  write_be16(frame + 24U, 0);
  write_be16(frame + 24U, checksum(frame + 14U, 20U));
  return ethernet_send_frame(frame, udp_offset + udp_length);
}

static bool parse_dhcp_reply(const uint8_t *frame, size_t frame_length,
    uint32_t xid, uint8_t expected_type, uint8_t offered_ip[4],
    uint8_t server_ip[4], uint8_t mask[4], uint8_t router[4],
    uint8_t dns[4])
{
  if (frame_length < 14U + 20U + 8U + 240U ||
      read_be16(frame + 12U) != ETHERTYPE_IPV4)
  {
    return false;
  }
  const uint8_t *ip = frame + 14U;
  size_t ip_header_length = (ip[0] & 0x0FU) * 4U;
  if ((ip[0] >> 4) != 4U || ip_header_length < 20U ||
      frame_length < 14U + ip_header_length + 8U ||
      ip[9] != IP_PROTOCOL_UDP)
  {
    return false;
  }
  const uint8_t *udp = ip + ip_header_length;
  uint16_t udp_length = read_be16(udp + 4U);
  if (read_be16(udp) != DHCP_SERVER_PORT ||
      read_be16(udp + 2U) != DHCP_CLIENT_PORT || udp_length < 8U + 240U ||
      frame_length < 14U + ip_header_length + udp_length)
  {
    return false;
  }
  const uint8_t *bootp = udp + 8U;
  if (bootp[0] != 2U || bootp[1] != 1U || bootp[2] != 6U ||
      read_be32(bootp + 4U) != xid ||
      !bytes_equal(bootp + 28U, local_mac, sizeof(local_mac)) ||
      bootp[236] != 0x63U || bootp[237] != 0x82U ||
      bootp[238] != 0x53U || bootp[239] != 0x63U)
  {
    return false;
  }

  memcpy(offered_ip, bootp + 16U, 4U);
  bool found_type = false;
  size_t option = 240U;
  size_t bootp_length = udp_length - 8U;
  while (option < bootp_length)
  {
    uint8_t code = bootp[option++];
    if (code == 0U)
    {
      continue;
    }
    if (code == 255U)
    {
      break;
    }
    if (option >= bootp_length)
    {
      return false;
    }
    uint8_t length = bootp[option++];
    if (length > bootp_length - option)
    {
      return false;
    }
    switch (code)
    {
      case 53U:
        found_type = length == 1U && bootp[option] == expected_type;
        break;
      case 54U:
        if (length >= 4U) memcpy(server_ip, bootp + option, 4U);
        break;
      case 1U:
        if (length >= 4U) memcpy(mask, bootp + option, 4U);
        break;
      case 3U:
        if (length >= 4U) memcpy(router, bootp + option, 4U);
        break;
      case 6U:
        if (length >= 4U) memcpy(dns, bootp + option, 4U);
        break;
      default:
        break;
    }
    option += length;
  }
  return found_type;
}

static bool configure_dhcp(void)
{
  struct ethernet_device_info device;
  if (!ethernet_get_info(&device) || !device.link_up)
  {
    vga_write("ping: Ethernet link is down\n");
    return false;
  }
  memcpy(local_mac, device.mac_address, sizeof(local_mac));
  dhcp_xid ^= (uint32_t)scheduler_ticks() + 0x9E3779B9U;

  uint8_t frame[FRAME_CAPACITY];
  size_t frame_length;
  uint8_t offered[4] = {0};
  uint8_t server[4] = {0};
  uint8_t mask[4] = {0};
  uint8_t router[4] = {0};
  uint8_t dns[4] = {0};

  if (!send_dhcp(1U, dhcp_xid, offered, server))
  {
    vga_write("ping: DHCP discover send failed\n");
    return false;
  }
  uint64_t start = scheduler_ticks();
  bool offer_received = false;
  while (scheduler_ticks() - start < DHCP_TIMEOUT_TICKS)
  {
    if (poll_frame(frame, &frame_length) &&
        parse_dhcp_reply(frame, frame_length, dhcp_xid, 2U, offered,
            server, mask, router, dns))
    {
      offer_received = true;
      break;
    }
    wait_for_tick();
  }
  if (!offer_received || offered[0] == 0U || server[0] == 0U)
  {
    vga_write("ping: no DHCP offer from QEMU network\n");
    return false;
  }

  if (!send_dhcp(3U, dhcp_xid, offered, server))
  {
    vga_write("ping: DHCP request send failed\n");
    return false;
  }
  start = scheduler_ticks();
  bool ack_received = false;
  while (scheduler_ticks() - start < DHCP_TIMEOUT_TICKS)
  {
    if (poll_frame(frame, &frame_length) &&
        parse_dhcp_reply(frame, frame_length, dhcp_xid, 5U, offered,
            server, mask, router, dns))
    {
      ack_received = true;
      break;
    }
    wait_for_tick();
  }
  if (!ack_received || router[0] == 0U || dns[0] == 0U)
  {
    vga_write("ping: DHCP lease configuration incomplete\n");
    return false;
  }
  memcpy(local_ip, offered, 4U);
  memcpy(subnet_mask, mask, 4U);
  memcpy(gateway_ip, router, 4U);
  memcpy(dns_ip, dns, 4U);
  network_configured = true;
  return true;
}

static bool parse_ipv4(const char *text, uint8_t address[4])
{
  for (size_t part = 0; part < 4U; part++)
  {
    if (*text < '0' || *text > '9')
    {
      return false;
    }
    uint32_t value = 0;
    size_t digits = 0;
    while (*text >= '0' && *text <= '9')
    {
      value = value * 10U + (uint32_t)(*text++ - '0');
      if (value > 255U || ++digits > 3U)
      {
        return false;
      }
    }
    address[part] = (uint8_t)value;
    if (part != 3U)
    {
      if (*text++ != '.') return false;
    }
  }
  return *text == '\0';
}

static bool append_dns_name(uint8_t *buffer, size_t capacity,
    size_t *position, const char *hostname)
{
  size_t index = 0;
  while (hostname[index] != '\0')
  {
    size_t label_start = index;
    while (hostname[index] != '\0' && hostname[index] != '.')
    {
      unsigned char character = (unsigned char)hostname[index];
      if (character <= 0x20U || character >= 0x7FU ||
          *position >= capacity)
      {
        return false;
      }
      index++;
    }
    size_t label_length = index - label_start;
    if (label_length == 0U || label_length > 63U ||
        *position + label_length + 1U > capacity)
    {
      return false;
    }
    buffer[(*position)++] = (uint8_t)label_length;
    for (size_t letter = label_start; letter < index; letter++)
    {
      char character = hostname[letter];
      buffer[(*position)++] = character >= 'A' && character <= 'Z'
          ? (uint8_t)(character - 'A' + 'a') : (uint8_t)character;
    }
    if (hostname[index] == '.')
    {
      index++;
      if (hostname[index] == '\0') break;
    }
  }
  if (*position >= capacity)
  {
    return false;
  }
  buffer[(*position)++] = 0;
  return true;
}

static size_t skip_dns_name(const uint8_t *data, size_t length, size_t offset)
{
  size_t position = offset;
  size_t steps = 0;
  while (position < length && steps++ < 128U)
  {
    uint8_t label = data[position++];
    if (label == 0U)
    {
      return position;
    }
    if ((label & 0xC0U) == 0xC0U)
    {
      return position < length ? position + 1U : length + 1U;
    }
    if ((label & 0xC0U) != 0 || label > length - position)
    {
      return length + 1U;
    }
    position += label;
  }
  return length + 1U;
}

static bool parse_dns_answer(const uint8_t *message, size_t length,
    uint16_t identifier, uint8_t address[4])
{
  if (length < 12U || read_be16(message) != identifier ||
      (read_be16(message + 2U) & 0x8000U) == 0 ||
      (read_be16(message + 2U) & 0x000FU) != 0)
  {
    return false;
  }
  uint16_t questions = read_be16(message + 4U);
  uint16_t answers = read_be16(message + 6U);
  size_t offset = 12U;
  for (uint16_t index = 0; index < questions; index++)
  {
    offset = skip_dns_name(message, length, offset);
    if (offset > length || length - offset < 4U)
    {
      return false;
    }
    offset += 4U;
  }
  for (uint16_t index = 0; index < answers; index++)
  {
    offset = skip_dns_name(message, length, offset);
    if (offset > length || length - offset < 10U)
    {
      return false;
    }
    uint16_t type = read_be16(message + offset);
    uint16_t class_code = read_be16(message + offset + 2U);
    uint16_t record_length = read_be16(message + offset + 8U);
    offset += 10U;
    if (record_length > length - offset)
    {
      return false;
    }
    if (type == 1U && class_code == 1U && record_length == 4U)
    {
      memcpy(address, message + offset, 4U);
      return true;
    }
    offset += record_length;
  }
  return false;
}

static bool resolve_dns(const char *hostname, uint8_t address[4])
{
  uint8_t frame[FRAME_CAPACITY] = {0};
  uint8_t dns_payload[256] = {0};
  uint8_t destination_mac[6] = {0};
  size_t name_position = 12U;
  dns_identifier++;
  write_be16(dns_payload, dns_identifier);
  write_be16(dns_payload + 2U, 0x0100U);
  write_be16(dns_payload + 4U, 1U);
  if (!append_dns_name(dns_payload, sizeof(dns_payload), &name_position,
      hostname) || name_position + 4U > sizeof(dns_payload))
  {
    vga_write("ping: invalid hostname\n");
    return false;
  }
  write_be16(dns_payload + name_position, 1U);
  write_be16(dns_payload + name_position + 2U, 1U);
  name_position += 4U;

  if (!resolve_arp(dns_ip, destination_mac))
  {
    vga_write("ping: couldn't resolve DNS server MAC\n");
    return false;
  }
  if (!send_udp(destination_mac, dns_ip, 53000U, DNS_SERVER_PORT,
      dns_payload, name_position))
  {
    vga_write("ping: DNS query send failed\n");
    return false;
  }

  uint64_t start = scheduler_ticks();
  size_t frame_length;
  while (scheduler_ticks() - start < DNS_TIMEOUT_TICKS)
  {
    if (poll_frame(frame, &frame_length))
    {
      const uint8_t *udp;
      const uint8_t *payload;
      size_t payload_length;
      if (parse_udp_from_ip(frame, frame_length, &udp, &payload,
          &payload_length) && udp[9] == IP_PROTOCOL_UDP &&
          read_be16(udp + 20U) == DNS_SERVER_PORT &&
          read_be16(udp + 22U) == 53000U &&
          parse_dns_answer(payload, payload_length, dns_identifier, address))
      {
        return true;
      }
    }
    wait_for_tick();
  }
  vga_write("ping: DNS lookup timed out or returned no IPv4 address\n");
  return false;
}

static bool parse_ipv4_packet(const uint8_t *frame, size_t frame_length,
    const uint8_t **ip_out, size_t *ip_length_out)
{
  if (frame_length < 34U || read_be16(frame + 12U) != ETHERTYPE_IPV4)
  {
    return false;
  }
  const uint8_t *ip = frame + 14U;
  size_t header_length = (ip[0] & 0x0FU) * 4U;
  uint16_t total_length = read_be16(ip + 2U);
  if ((ip[0] >> 4) != 4U || header_length < 20U ||
      total_length < header_length || frame_length < 14U + total_length)
  {
    return false;
  }
  *ip_out = ip;
  *ip_length_out = total_length;
  return true;
}

static bool parse_udp_from_ip(const uint8_t *frame, size_t frame_length,
    const uint8_t **ip_out, const uint8_t **payload_out,
    size_t *payload_length_out)
{
  const uint8_t *ip;
  size_t total_length;
  if (!parse_ipv4_packet(frame, frame_length, &ip, &total_length) ||
      ip[9] != IP_PROTOCOL_UDP)
  {
    return false;
  }
  size_t header_length = (ip[0] & 0x0FU) * 4U;
  if (total_length < header_length + 8U)
  {
    return false;
  }
  const uint8_t *udp = ip + header_length;
  uint16_t udp_length = read_be16(udp + 4U);
  if (udp_length < 8U || udp_length > total_length - header_length)
  {
    return false;
  }
  *ip_out = ip;
  *payload_out = udp + 8U;
  *payload_length_out = udp_length - 8U;
  return true;
}

static bool send_udp(const uint8_t destination_mac[6],
    const uint8_t destination_ip[4], uint16_t source_port,
    uint16_t destination_port, const uint8_t *payload, size_t payload_length)
{
  uint8_t frame[FRAME_CAPACITY] = {0};
  if (payload_length > FRAME_CAPACITY - 42U)
  {
    return false;
  }
  size_t udp_length = 8U + payload_length;
  size_t offset = build_ipv4_header(frame, destination_mac, local_ip,
      destination_ip, IP_PROTOCOL_UDP, (uint16_t)udp_length);
  uint8_t *udp = frame + offset;
  write_be16(udp, source_port);
  write_be16(udp + 2U, destination_port);
  write_be16(udp + 4U, (uint16_t)udp_length);
  write_be16(udp + 6U, 0U);
  memcpy(udp + 8U, payload, payload_length);
  return ethernet_send_frame(frame, offset + udp_length);
}

static bool resolve_arp(const uint8_t target_ip[4], uint8_t target_mac[6])
{
  uint8_t frame[FRAME_CAPACITY] = {0};
  static const uint8_t broadcast_mac[6] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
  };
  memcpy(frame, broadcast_mac, 6U);
  memcpy(frame + 6U, local_mac, 6U);
  write_be16(frame + 12U, ETHERTYPE_ARP);
  uint8_t *arp = frame + 14U;
  write_be16(arp, 1U);
  write_be16(arp + 2U, ETHERTYPE_IPV4);
  arp[4] = 6U;
  arp[5] = 4U;
  write_be16(arp + 6U, 1U);
  memcpy(arp + 8U, local_mac, 6U);
  memcpy(arp + 14U, local_ip, 4U);
  memcpy(arp + 24U, target_ip, 4U);
  if (!ethernet_send_frame(frame, sizeof(frame)))
  {
    return false;
  }

  uint64_t start = scheduler_ticks();
  size_t frame_length;
  while (scheduler_ticks() - start < ARP_TIMEOUT_TICKS)
  {
    if (poll_frame(frame, &frame_length) && frame_length >= 42U &&
        read_be16(frame + 12U) == ETHERTYPE_ARP)
    {
      const uint8_t *reply = frame + 14U;
      if (read_be16(reply + 6U) == 2U &&
          bytes_equal(reply + 14U, target_ip, 4U) &&
          bytes_equal(reply + 24U, local_ip, 4U))
      {
        memcpy(target_mac, reply + 8U, 6U);
        return true;
      }
    }
    wait_for_tick();
  }
  return false;
}

static void print_ipv4(const uint8_t address[4])
{
  for (size_t part = 0; part < 4U; part++)
  {
    uint8_t value = address[part];
    char digits[3];
    size_t length = 0;
    do
    {
      digits[length++] = (char)('0' + value % 10U);
      value /= 10U;
    } while (value != 0U);
    while (length != 0)
    {
      vga_write_char(digits[--length]);
    }
    if (part != 3U)
    {
      vga_write_char('.');
    }
  }
}

static void print_unsigned(uint64_t value)
{
  char digits[20];
  size_t length = 0;
  do
  {
    digits[length++] = (char)('0' + value % 10U);
    value /= 10U;
  } while (value != 0U);
  while (length != 0)
  {
    vga_write_char(digits[--length]);
  }
}

static bool send_echo_request(const uint8_t destination_ip[4],
    const uint8_t destination_mac[6], uint16_t sequence)
{
  uint8_t frame[14U + 20U + 8U + 32U] = {0};
  size_t offset = build_ipv4_header(frame, destination_mac, local_ip,
      destination_ip, IP_PROTOCOL_ICMP, 40U);
  uint8_t *icmp = frame + offset;
  icmp[0] = 8U;
  icmp[1] = 0U;
  write_be16(icmp + 2U, 0U);
  write_be16(icmp + 4U, 0x4554U);
  write_be16(icmp + 6U, sequence);
  for (size_t index = 8U; index < 40U; index++)
  {
    icmp[index] = (uint8_t)(index + sequence);
  }
  write_be16(icmp + 2U, checksum(icmp, 40U));
  return ethernet_send_frame(frame, sizeof(frame));
}

static bool wait_echo_reply(const uint8_t destination_ip[4],
  uint16_t sequence, uint64_t start_tick, uint64_t *elapsed_ticks,
  uint8_t *reply_ttl)
{
  uint8_t frame[FRAME_CAPACITY];
  size_t frame_length;
  uint64_t start = scheduler_ticks();
  while (scheduler_ticks() - start < ICMP_TIMEOUT_TICKS)
  {
    if (poll_frame(frame, &frame_length))
    {
      const uint8_t *ip;
      size_t total_length;
      if (parse_ipv4_packet(frame, frame_length, &ip, &total_length) &&
          ip[9] == IP_PROTOCOL_ICMP && total_length >= 28U &&
          bytes_equal(ip + 12U, destination_ip, 4U))
      {
        size_t ip_header_length = (ip[0] & 0x0FU) * 4U;
        const uint8_t *icmp = ip + ip_header_length;
        if (icmp[0] == 0U && read_be16(icmp + 4U) == 0x4554U &&
            read_be16(icmp + 6U) == sequence)
        {
          *elapsed_ticks = scheduler_ticks() - start_tick;
          *reply_ttl = ip[8];
          return true;
        }
      }
    }
    wait_for_tick();
  }
  return false;
}

bool network_ping(const char *hostname)
{
  if (hostname == 0 || hostname[0] == '\0')
  {
    return false;
  }
  if (!network_configured && !configure_dhcp())
  {
    return false;
  }

  uint8_t destination_ip[4];
  if (!parse_ipv4(hostname, destination_ip) &&
      !resolve_dns(hostname, destination_ip))
  {
    return false;
  }

  vga_write("PING ");
  vga_write(hostname);
  vga_write(" (");
  print_ipv4(destination_ip);
  vga_write(") 32 bytes\n");

  uint8_t next_hop[4];
  bool destination_is_local = true;
  for (size_t index = 0; index < 4U; index++)
  {
    if ((destination_ip[index] & subnet_mask[index]) !=
        (local_ip[index] & subnet_mask[index]))
    {
      destination_is_local = false;
      break;
    }
  }
  memcpy(next_hop, destination_is_local ? destination_ip : gateway_ip, 4U);
  uint8_t destination_mac[6];
  if (!resolve_arp(next_hop, destination_mac))
  {
    vga_write("ping: ARP resolution timed out\n");
    return false;
  }

  size_t received = 0;
  for (uint16_t sequence = 1U; sequence <= PING_COUNT; sequence++)
  {
    uint64_t start_tick = scheduler_ticks();
    if (!send_echo_request(destination_ip, destination_mac, sequence))
    {
      vga_write("ping: failed to send ICMP request\n");
      continue;
    }
    uint64_t elapsed = 0;
    uint8_t reply_ttl = 0;
    if (wait_echo_reply(destination_ip, sequence, start_tick, &elapsed,
      &reply_ttl))
    {
      vga_write("32 bytes from ");
      print_ipv4(destination_ip);
      vga_write(": icmp_seq=");
      print_unsigned(sequence);
      vga_write(" ttl=");
      print_unsigned(reply_ttl);
      vga_write(" time=");
      if (elapsed == 0)
      {
        vga_write("<10");
      }
      else
      {
        print_unsigned(elapsed * 10U);
      }
      vga_write(" ms\n");
      received++;
    }
    else
    {
      vga_write("Request timeout for icmp_seq ");
      print_unsigned(sequence);
      vga_write_char('\n');
    }
  }
  vga_write("--- ping statistics: ");
  print_unsigned(PING_COUNT);
  vga_write(" sent, ");
  print_unsigned(received);
  vga_write(" received ---\n");
  return received != 0;
}
