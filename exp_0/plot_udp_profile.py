#!/usr/bin/env python3
import matplotlib.pyplot as plt

offered = [1, 2, 3, 4, 5]
achieved = [0.85, 0.86, 0.90, 0.82, 0.93]
loss = [3.1, 47, 63, 75, 77]

fig, ax1 = plt.subplots(figsize=(6, 3.5))

# Left axis: throughput
color = 'tab:blue'
ax1.set_xlabel('Offered load (Mbit/s)')
ax1.set_ylabel('Achieved throughput (Mbit/s)', color=color)
ax1.plot(offered, achieved, color=color, marker='o', label='Throughput')
ax1.tick_params(axis='y', labelcolor=color)
ax1.set_ylim(0, 1.2)

# Right axis: loss
ax2 = ax1.twinx()
color = 'tab:red'
ax2.set_ylabel('Packet loss (%)', color=color)
ax2.plot(offered, loss, color=color, marker='s', linestyle='--', label='Loss')
ax2.tick_params(axis='y', labelcolor=color)
ax2.set_ylim(0, 100)

fig.tight_layout()
fig.savefig('fig2_udp_profile.png', dpi=300)
print("Saved fig2_udp_profile.png")
