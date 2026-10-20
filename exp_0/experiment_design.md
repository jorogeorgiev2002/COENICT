## Network Tunnel Performance Evaluation

### A. Setup and Constraints

COENICT connects to the local Wi-Fi (192.168.0.0/24) and establishes a
WireGuard tunnel to Machine A (192.168.1.0/24).  Machine B (192.168.0.0/24)
hosts the measurement servers.  The forwarding path is:

Machine A → WireGuard tunnel → COENICT → Wi-Fi → Machine B

COENICT is based on the ESP32-S3 MCU and is not intended as a general-purpose
router.  Prior ESP32 WireGuard measurements report additional memory, CPU, and
latency costs relative to non-VPN communication [1].  Therefore, all tests are
short (≤20 s), single-flow, and separated by a 10 s cool-down.  A 5-packet ICMP
probe to the COENICT is issued before each test; if the tunnel is down, the
operator re-establishes it via the command channel before continuing.

### B. Test Procedure

All commands are executed on **Machine A**.  The tests are run in the order
listed below to avoid aggressive loads contaminating baseline measurements.
Flent was used to make the time-series measurements repeatable and to generate
comparative plots, consistent with its design as a coordinator and analysis
wrapper for network benchmark tools [2].

| Step | Tool | Command | Purpose |
|---|---|---|---|
| 1 | `ping` | `ping -c 100 -i 0.2 <MACHINE_B_IP>` | Baseline RTT and loss |
| 2 | `iperf3` | `iperf3 -c <MACHINE_B_IP> -t 10 -i 1 -f m` | Single-stream TCP throughput |
| 3 | `iperf3` | `iperf3 -c <MACHINE_B_IP> -u -b <RATE> -t 10 -f m` | UDP throughput and loss; repeat for `RATE = 1M, 2M, 3M, 4M, 5M` |
| 4 | `irtt` | `irtt client -i 100ms -d 15s <MACHINE_B_IP>` | High-resolution RTT and jitter |
| 5 | `ping` | `ping -c 10 -s <SIZE> -M do <MACHINE_B_IP>` | Path-MTU discovery; `SIZE = 56, 512, 1024, 1380, 1392, 1400` |
| 6a | `flent` | `flent ping -H <MACHINE_B_IP> -l 60 -t ping_idle_<DEVICE>.json.gz` | Baseline RTT time-series |
| 6b | `flent` + `iperf3` | `iperf3 -c <MACHINE_B_IP> -u -b 1M -t 70` (background); `flent ping -H <MACHINE_B_IP> -l 60 -t ping_loaded_<DEVICE>.json.gz` | RTT time-series under 1 Mbit/s UDP load |

> **Note:** The commands above create raw text files.  The values in Section C were extracted from those files with the one-liners shown in Section G.

*Prerequisites on Machine B:* `iperf3 -s`, `irtt server`, and `netserver` running.

### C. Results

| Metric | Value | Raw File |
|---|---|---|
| Baseline RTT (mean / std. dev.) | 72.33 ms / 32.79 ms | `ping_Test_1.txt` |
| Baseline packet loss | 1 % | `ping_Test_1.txt` |
| TCP throughput (sender / receiver) | 1.26 / 1.04 Mbit/s | `tcp_Test_1.txt` |
| TCP retransmissions | 17 | `tcp_Test_1.txt` |
| UDP throughput (1 M offered) | 0.85 Mbit/s | `udp_1M_Test_1.txt` |
| UDP loss (1 M offered) | 3.1 % | `udp_1M_Test_1.txt` |
| UDP throughput (2 M offered) | 0.86 Mbit/s | `udp_2M_Test_1.txt` |
| UDP loss (2 M offered) | 47 % | `udp_2M_Test_1.txt` |
| UDP throughput (3 M offered) | 0.90 Mbit/s | `udp_3M_Test_1.txt` |
| UDP loss (3 M offered) | 63 % | `udp_3M_Test_1.txt` |
| UDP throughput (4 M offered) | 0.82 Mbit/s | `udp_4M_Test_1.txt` |
| UDP loss (4 M offered) | 75 % | `udp_4M_Test_1.txt` |
| UDP throughput (5 M offered) | 0.93 Mbit/s | `udp_5M_Test_1.txt` |
| UDP loss (5 M offered) | 77 % | `udp_5M_Test_1.txt` |
| Mean RTT (`irtt`) | 55.7 ms | `irtt_Test_1.txt` |
| RTT std. dev. (`irtt`) | 40.05 ms | `irtt_Test_1.txt` |
| Largest validated unfragmented inner IP packet | 1420 bytes | `mtu_1392_Test_1.txt` |
| First failed inner IP packet size | 1428 bytes | `mtu_1400_Test_1.txt` |
| Tunnel failure observed? | No, during the recorded sequence | — |

### D. Figures

* **Fig. 1 — RTT Comparison (Baseline vs. Light Load):** Overlay of two `flent
  ping` time-series.  `flent -i ping_idle_<DEVICE>.json.gz -i
ping_loaded_<DEVICE>.json.gz -p ping --save figX_rtt_comparison_<DEVICE>.png`.
X-axis: elapsed time (s); Y-axis: RTT (ms).  The plot compares latency with no
background traffic and with a 1 Mbit/s UDP background flow.
* **Fig. 2 — UDP Load Profile:** Hand-crafted plot from the iperf3 UDP text
  logs (Step 3).  X-axis: offered load (Mbit/s); left Y-axis: achieved
throughput (Mbit/s); right Y-axis: loss (%).  The receiver throughput plateaus
near 0.9 Mbit/s while loss rises sharply beyond 1 Mbit/s, revealing the hard
forwarding ceiling of the MCU.  Generate with a script (see Section G).

### E. Interpretation

The baseline ICMP test measured a mean RTT of **72.33** ms (std. dev. **32.79**
ms) with **1** % packet loss through the COENICT tunnel in the absence of
background traffic.  The high RTT variance is consistent with variable
processing and queueing in the Wi-Fi and WireGuard forwarding path.

TCP throughput with a single stream reached **1.26** Mbit/s (sender) and
**1.04** Mbit/s (receiver), with **17** retransmissions occurring during the
10-second test.  The difference between sender and receiver rates, together
with the retransmissions, is consistent with queueing and packet loss along the
tested forwarding path.

The UDP loss profile reveals a practical forwarding ceiling.  Receiver-side
throughput remains within **0.82–0.93** Mbit/s across all offered rates, while
loss increases from **3.1** % at 1 Mbit/s to **47** % at 2 Mbit/s and **77** %
at 5 Mbit/s.  This behaviour is consistent with a constrained embedded
forwarding environment: once the offered rate exceeds its processing and queueing
capacity, excess packets are dropped abruptly.

Path-MTU tests show that a **1392-byte** ICMP payload succeeds without
fragmentation, whereas a **1400-byte** payload fails.  Including the 8-byte
ICMP and 20-byte IPv4 headers, this validates unfragmented delivery of a
**1420-byte** inner IP packet and bounds the effective inner path MTU to the
interval from **1420** to **1427** bytes.  The precise value depends on the
WireGuard interface configuration and outer IP encapsulation.

High-resolution irtt measurements report a mean RTT of **55.7** ms (std. dev.
**40.05** ms) with **1.33** % loss.  The delay is highly asymmetric: the send
delay (A → B) averages **53.85** ms (std. dev. **39.9** ms), whereas the
receive delay (B -> A) averages **1.85** ms (std. dev. **2.8** ms).  This result
identifies the A -> B direction, which includes tunnel decapsulation and Wi-Fi
forwarding by the COENICT, as the dominant contributor to latency variation.

In summary, the tunnel provides functional low-rate connectivity suitable for
command traffic and typical penetration testing tasks.  The measured UDP
receiver throughput remains below **1 Mbit/s**, and the irtt RTT standard
deviation is **40.05** ms.  Standard multi-flow bufferbloat tests were excluded
because preliminary runs destabilised the tunnel.  This decision is
methodologically justified because Flent’s RRUL test combines four concurrent
TCP flows in each direction with UDP and ICMP latency flows [2].

### F. References

[1] C. M. Purba and P. D. Ibnugraha, "Securing Data Transmission in IoT Devices Using Wireguard VPN," in *Proc. 2025 IEEE Int. Conf. on Artificial Intelligence and Mechatronics Systems (AIMS)*, 2025, doi: 10.1109/AIMS66189.2025.11229500.

[2] T. Høiland-Jørgensen et al., "Flent: The FLExible Network Tester," in *Proc. 11th EAI Int. Conf. Performance Evaluation Methodologies and Tools (VALUETOOLS)*, 2017, pp. 120–125, doi: 10.1145/3150928.3150957.

### G. Quick Automation

The following Bash script reproduces the exact sequence used for the results above.  Execute on Machine A with `./run_tunnel_tests.sh <DEVICE_IP> <MACHINE_B_IP> <LABEL>`.

```bash
#!/usr/bin/env bash
set -euo pipefail
DEV_IP="$1"; B_IP="$2"; LABEL="$3"; CD=10
health() { ping -c 5 -i 1 "$DEV_IP" >/dev/null || { echo "FAIL: tunnel down"; exit 1; }; }
mkdir -p "res_${LABEL}" && cd "res_${LABEL}"
health; ping -c 100 -i 0.2 "$B_IP" | tee "ping_${LABEL}.txt"; sleep $CD
health; iperf3 -c "$B_IP" -t 10 -i 1 -f m | tee "tcp_${LABEL}.txt"; sleep $CD
for r in 1M 2M 3M 4M 5M; do
  health; iperf3 -c "$B_IP" -u -b "$r" -t 10 -f m | tee "udp_${r}_${LABEL}.txt"; sleep $CD
done
health; irtt client -i 100ms -d 15s "$B_IP" | tee "irtt_${LABEL}.txt"; sleep $CD
for s in 56 512 1024 1380 1392 1400; do
  health; ping -c 10 -s "$s" -M do "$B_IP" | tee "mtu_${s}_${LABEL}.txt" || true
done
health; flent ping -H "$B_IP" -l 60 -t "ping_idle_${LABEL}.json.gz"; sleep $CD
health; iperf3 -c "$B_IP" -u -b 1M -t 70 -i 1 > "bg_udp_${LABEL}.txt" &
BGPID=$!
sleep 2
flent ping -H "$B_IP" -l 60 -t "ping_loaded_${LABEL}.json.gz"
kill $BGPID 2>/dev/null; wait $BGPID 2>/dev/null
```

**Extracting results.**

```bash
# From inside res_${LABEL}/
grep -E 'rtt|packet loss' ping_${LABEL}.txt
grep -E 'Mbits/sec|receiver' tcp_${LABEL}.txt
grep 'Retr' tcp_${LABEL}.txt
for r in 1M 2M 3M 4M 5M; do
  echo -n "$r: "; grep 'Mbits/sec' udp_${r}_${LABEL}.txt | tail -1
done
grep -E 'Mean|Stddev' irtt_${LABEL}.txt
for s in 56 512 1024 1380 1392 1400; do
  echo -n "$s: "; grep 'packet loss' mtu_${s}_${LABEL}.txt
done

# Plot generation
flent -i ping_idle_${LABEL}.json.gz -i ping_loaded_${LABEL}.json.gz -p ping --save figX_rtt_${LABEL}.png

# Figure Y (manual, from iperf3 UDP logs)
python3 plot_udp_profile.py
```
