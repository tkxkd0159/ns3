set terminal png size 600,400
set output "cwnd2.png"
set title "cwnd size"
set xlabel "Time(sec)"
set ylabel "cwnd (B)"
plot "tcp-dynamic-pacing-cwnd2.dat" using 1:2 with lines title "cwnd"
