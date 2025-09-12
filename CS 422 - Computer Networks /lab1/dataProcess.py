import sys
from scapy.all import rdpcap, IP, TCP

def extract_packets(pcap_file, output_file, is_receiver=False):
    packets = rdpcap(pcap_file)
    packets = sorted(packets, key=lambda p: p.time)
    seqs = [pkt[TCP].seq for pkt in packets if IP in pkt and TCP in pkt and len(pkt[TCP].payload) > 0]
    if not seqs:
        print(f"No data packets in {pcap_file}")
        return
    min_seq = min(seqs)
    ips_src = set()
    ips_dst = set()
    with open(output_file, 'w') as f:
        for pkt in packets:
            if IP in pkt and TCP in pkt and len(pkt[TCP].payload) > 0:
                if is_receiver and pkt[IP].dst != "128.10.126.34":
                    continue
                if not is_receiver and pkt[IP].src != "128.10.106.26":
                    continue
                seq = pkt[TCP].seq - min_seq
                data_len = len(pkt[TCP].payload)
                next_seq = seq + data_len
                ts = pkt.time * 1000
                f.write(f"{seq}, {next_seq}, {ts}\n")
                ips_src.add(pkt[IP].src)
                ips_dst.add(pkt[IP].dst)
    print(f"IPs en {pcap_file}: Sender {list(ips_src)[0] if ips_src else 'None'}, Receiver {list(ips_dst)[0] if ips_dst else 'None'}")

if __name__ == "__main__":
    sender_pcap = sys.argv[1]
    receiver_pcap = sys.argv[2]
    extract_packets(sender_pcap, "senderPackets.txt", is_receiver=False)
    extract_packets(receiver_pcap, "receiverPackets.txt", is_receiver=True)