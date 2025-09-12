import sys
from collections import defaultdict

sender_file = sys.argv[1]
receiver_file = sys.argv[2]

sender = []
with open(sender_file, 'r') as f:
    for line in f:
        parts = line.strip().split(', ')
        if len(parts) == 3:
            seq, next_seq, ts = int(parts[0]), int(parts[1]), float(parts[2])
            sender.append((seq, next_seq, ts))

receiver_times = defaultdict(list)
with open(receiver_file, 'r') as f:
    for line in f:
        parts = line.strip().split(', ')
        if len(parts) == 3:
            seq, next_seq, ts = int(parts[0]), int(parts[1]), float(parts[2])
            receiver_times[(seq, next_seq)].append(ts)

result = []
for seq, next_seq, ts in sorted(sender, key=lambda x: x[0]):
    key = (seq, next_seq)
    times = receiver_times[key]
    if not times:
        status, consumed = 0, 5000.0
    else:
        status = 2 if len(times) > 1 else 1
        consumed = max(times) - ts
    result.append(f"{seq}, {next_seq}, {status}, {consumed}")

with open('result.txt', 'w') as f:
    f.write('\n'.join(result))
