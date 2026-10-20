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
