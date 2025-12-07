import sys
from scapy.all import rdpcap, DHCP, BOOTP, ARP, DNS, DNSQR, Ether, IP, UDP, Raw

def usage_and_exit():
    print("Usage: python3 lab4.py [CASE] [pcap_file] [destination_website]")
    print("CASE: A, B, C, ALL (default ALL). destination_website required for C and ALL.")
    sys.exit(1)

def parse_args():
    if len(sys.argv) < 3:
        usage_and_exit()
    case = sys.argv[1].upper() if len(sys.argv) >= 4 else "ALL"
    pcap_file = sys.argv[2] if len(sys.argv) >= 3 else None
    dest = None
    # handle both calling styles: python3 lab4.py lab4-test.pcap (defaults to ALL)
    if len(sys.argv) == 3:
        
        # assume pcap only, default ALL
        case = "ALL"
        pcap_file = sys.argv[2]
    elif len(sys.argv) >= 4:
        if sys.argv[1].upper() in ("A","B","C","ALL"):
            case = sys.argv[1].upper()
            pcap_file = sys.argv[2]
            if case in ("C","ALL"):
                
                if len(sys.argv) < 4:
                    usage_and_exit()
                dest = sys.argv[3]
        else:
            case = "ALL"
            pcap_file = sys.argv[1]
            dest = sys.argv[2]
    return case, pcap_file, dest

def load_pcap(filename):
    pkts = rdpcap(filename)
    return pkts

def case_a(pkts):
    # Find DHCP packets related to our host's IP allocation
    frames = []
    client_mac = None
    for i, p in enumerate(pkts, start=1):

        if p.haslayer(DHCP):
            dhcp_options = p[DHCP].options
            msg_type = None
            for opt in dhcp_options:
                if isinstance(opt, tuple) and opt[0] == 'message-type':
                    msg_type = opt[1]
            if msg_type in (1, 3):
                if p.haslayer(Ether):
                    client_mac = p[Ether].src
                    break
                if p.haslayer(BOOTP):
                    ch = p[BOOTP].chaddr
                    try:
                        mac = ':'.join('%02x' % b for b in ch[:6])
                        client_mac = mac
                        break
                    except:
                        
                        pass
    if client_mac is None:
        for i, p in enumerate(pkts, start=1):
            if p.haslayer(DHCP) and p.haslayer(Ether):
                client_mac = p[Ether].src
                break

    # Now collect all DHCP frames that involve this client_mac
    ip_computer = None
    ip_gateway = None
    dhcp_frame_numbers = []
    for i, p in enumerate(pkts, start=1):
        if p.haslayer(DHCP):
            involved = False
            if p.haslayer(Ether) and p[Ether].src == client_mac:
                involved = True
            if p.haslayer(BOOTP):
                try:
                    ch = p[BOOTP].chaddr
                    mac = ':'.join('%02x' % b for b in ch[:6])
                    if mac == client_mac:
                        involved = True
                except:
                    pass
            if involved:
                dhcp_frame_numbers.append(i)
            # Try to parse assigned IP and router option from DHCP ACK/OFFER
            if p.haslayer(BOOTP):
                yi = p[BOOTP].yiaddr
                if yi and yi != '0.0.0.0':
                    ok = False
                    try:
                        ch = p[BOOTP].chaddr
                        mac = ':'.join('%02x' % b for b in ch[:6])
                        if mac == client_mac:
                            ok = True
                    except:
                        ok = True 
                    if ok:
                        ip_computer = yi
            if p.haslayer(DHCP):
                for opt in p[DHCP].options:
                    if isinstance(opt, tuple) and opt[0] == 'router':
                        val = opt[1]
                        if isinstance(val, list):
                            ip_gateway = val[0]
                        else:
                            ip_gateway = val
    # Output
    print("CASE A:\n")
    if dhcp_frame_numbers:
        for f in dhcp_frame_numbers:
            print("Frame {}".format(f))
    else:
        print("# No DHCP frames found for this client in pcap.")
    if ip_computer:
        print("\nIPAddr-computer: {}".format(ip_computer))
    else:
        print("\nIPAddr-computer: ")
    if ip_gateway:
        print("\nIPAddr-gateway: {}".format(ip_gateway))
    else:
        print("\nIPAddr-gateway: ")

    return client_mac, ip_computer, ip_gateway, dhcp_frame_numbers

def case_b(pkts, ip_gateway, client_ip=None):
    frames = []
    gateway_mac = None
    gateway_frame_nums = []
    for i, p in enumerate(pkts, start=1):
        if p.haslayer(ARP):
            arp = p[ARP]
            try:
                if arp.op == 1 and arp.pdst == ip_gateway:
                    gateway_frame_nums.append(i)
                if arp.op == 2 and arp.psrc == ip_gateway:
                    gateway_frame_nums.append(i)
                    gateway_mac = arp.hwsrc
            except:
                pass
    print("\nCASE B:\n")
    
    if gateway_frame_nums:
        # sort & unique while preserving order
        seen = set()
        ordered = []
        for f in gateway_frame_nums:
            if f not in seen:
                seen.add(f)
                ordered.append(f)
        for f in ordered:
            print("Frame {}".format(f))
    else:
        print("# No ARP frames for gateway found in pcap.")
    if ip_gateway:
        print("\nIPAddr-gateway: {}".format(ip_gateway))
    else:
        print("\nIPAddr-gateway: ")
    if gateway_mac:
        print("\nMACAddr-gateway: {}".format(gateway_mac))
    else:
        print("\nMACAddr-gateway: ")

    if gateway_mac:
        for i, p in enumerate(pkts, start=1):
            if p.haslayer(ARP):
                arp = p[ARP]
                if arp.op == 2 and arp.psrc == ip_gateway and arp.hwsrc == gateway_mac:
                    raw_bytes = bytes(p)
                    print("\n# Packet hex (ARP reply frame {}):".format(i))
                    print(raw_bytes.hex())
                    break

    return gateway_mac

def case_c(pkts, dest_name):
    query_frames = []
    answer_frames = []
    found_ip = None
    for i, p in enumerate(pkts, start=1):
        if p.haslayer(DNS) and p.haslayer(UDP):
            dns = p[DNS]
            if dns.qr == 0 and dns.qdcount > 0:
                try:
                    qname = dns.qd.qname.decode().rstrip('.')
                except:
                    try:
                        qname = str(dns.qd.qname)
                    except:
                        qname = None
                if qname == dest_name:
                    query_frames.append(i)
            # Answer
            if dns.qr == 1 and dns.ancount > 0:
                for j in range(dns.ancount):
                    ans = dns.an[j]
                    an_name = ans.rrname.decode().rstrip('.') if isinstance(ans.rrname, bytes) else str(ans.rrname).rstrip('.')
                    if an_name == dest_name:
                        if ans.type == 1:
                            if found_ip is None:
                                found_ip = ans.rdata
                            answer_frames.append(i)
                        else:
                            answer_frames.append(i)
                    else:
                        if ans.type == 1 and found_ip is None:
                            found_ip = ans.rdata
                            answer_frames.append(i)
    print("\nCASE C:\n")
    if query_frames:
        for f in query_frames:
            print("Frame {}".format(f))
    if answer_frames:
        for f in answer_frames:
            print("Frame {}".format(f))
    print("\nname: {}".format(dest_name))
    if found_ip:
        print("\nIPAddr: {}".format(found_ip))
    else:
        print("\nIPAddr: ")

    return query_frames, answer_frames, found_ip

def main():
    case, pcap_file, dest = parse_args()
    try:
        pkts = load_pcap(pcap_file)
    except FileNotFoundError:
        print("pcap file not found:", pcap_file)
        sys.exit(1)
    client_mac = None
    ip_computer = None
    ip_gateway = None
    if case in ("A","ALL"):
        client_mac, ip_computer, ip_gateway, dhcp_frames = case_a(pkts)
    if case in ("B","ALL"):
        if ip_gateway is None:
            for p in pkts:
                if p.haslayer(DHCP):
                    for opt in p[DHCP].options:
                        if isinstance(opt, tuple) and opt[0] == 'router':
                            val = opt[1]
                            if isinstance(val, list):
                                ip_gateway = val[0]
                            else:
                                ip_gateway = val
                            break
                if ip_gateway:
                    break
        gateway_mac = case_b(pkts, ip_gateway, ip_computer)
    
    if case in ("C","ALL"):
        if dest is None:
            print("Destination website required for CASE C / ALL")
            sys.exit(1)
        case_c(pkts, dest)

if __name__ == "__main__":
    main()
