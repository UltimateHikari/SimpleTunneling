** Port tunneling [lab34-35] **
 * Pair transmitter--reciever;
 * reciever connects to specified host,
 * transmitter tunnels all his connections (up to 254 because reasons)
to that host through reciever through single TCP socket between them.
 * simple "leading byte + byte with connection number + doubling all same bytes in payload + ending byte + zero byte" protocol was used for multiplexing 
 * except of its main purpose program is threoretically prone to errors
on TCP delivering only parts of packets or packets being bigger than buffer sizes,
which are used in encoding-decoding for passing mupliple connections and multiple frames between tsm-rcv
![Dev screenshot](./resources/scrot.png)
