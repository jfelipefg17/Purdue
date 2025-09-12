from scapy.all import rdpcap, IP, ICMP
import sys

def parse_hop_txt(hop_file):
    hops = {}
    with open(hop_file, 'r') as f:
        for line in f:
            if line.startswith('hop '):
                parts = line.strip().split(': ')
                hop_num = int(parts[0].split()[1])
                ip = parts[1].strip()
                if ip == '*':
                    ip = None
                hops[hop_num] = ip
    return hops

def analyze_pcap(pcap_file, hops):
    packets = rdpcap(pcap_file)
    icmp_packets = [p for p in packets if IP in p and ICMP in p and p[ICMP].type in [8, 0, 11]]
    
    sent_probes = {}  
    probe_groups = {}  
    
    src_ip = None
    for pkt in icmp_packets:
        if pkt[ICMP].type == 8:  
            if src_ip is None:
                src_ip = pkt[IP].src
            dest_ip = pkt[IP].dst
            ttl = pkt[IP].ttl
            seq = pkt[ICMP].seq  
            send_time = pkt.time
            key = (src_ip, dest_ip, ttl, seq)
            sent_probes[key] = send_time
        elif pkt[ICMP].type in [0, 11]:  
            if src_ip is None:
                continue
            resp_src = pkt[IP].src
            resp_time = pkt.time
            
            for (s_src, s_dest, s_ttl, seq), send_time in sent_probes.items():
                if s_src == pkt[IP].dst and s_dest == resp_src and s_ttl == pkt[IP].ttl + 1:
                    rtt = (resp_time - send_time) * 1000  
                    hop = s_ttl
                    if hop not in probe_groups:
                        probe_groups[hop] = {}
                    if resp_src not in probe_groups[hop]:
                        probe_groups[hop][resp_src] = []
                    probe_groups[hop][resp_src].append(round(rtt, 2))
                    del sent_probes[(s_src, s_dest, s_ttl, seq)]
                    break

    
    max_hop = max(hops.keys())
    for hop in range(1, max_hop + 1):
        if hop in probe_groups:
            for ip, rtts in sorted(probe_groups[hop].items()):
                while len(rtts) < 3:
                    rtts.append('*')
                print(f"hop {hop}: {ip}, {', '.join(map(str, rtts))}")
        elif hop in hops and hops[hop]:
            print(f"hop {hop}: {hops[hop]}, * * *")
        else:
            print(f"hop {hop}: unknown, * * *")

if __name__ == "__main__":
    if len(sys.argv) != 3:
        print("Usage: python pcapTraceroute.py <pcap_file> <hop.txt>")
        sys.exit(1)
    pcap_file = sys.argv[1]
    hop_file = sys.argv[2]
    hops = parse_hop_txt(hop_file)
    analyze_pcap(pcap_file, hops)