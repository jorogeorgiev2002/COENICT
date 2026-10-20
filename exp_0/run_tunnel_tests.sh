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

