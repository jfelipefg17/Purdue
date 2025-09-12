import sys
import matplotlib.pyplot as plt

result_file = sys.argv[1]
lines = [line.strip() for line in open(result_file, 'r') if line.strip()]

indices = []
times = []
for idx, line in enumerate(lines, 1):
    parts = line.split(', ')
    if len(parts) == 4 and int(parts[2]) == 1:
        indices.append(idx)
        times.append(float(parts[3]))

plt.figure()
plt.plot(indices, times, marker='o')
plt.xlabel('packet number (from 1)')
plt.ylabel('Time (ms)')
plt.title('Time per packet')
plt.savefig('plot.png')
plt.show()
