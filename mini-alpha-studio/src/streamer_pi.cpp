#include <arpa/inet.h>
#include <sys/socket.h>
#include <chrono>
#include <thread>
#include <cstdio>
#include <string>
#include "csv.hpp"

// Minimal stub for now; you can replace with the full version later.
int main(int argc, char** argv){
  const char* host = (argc>2 ? argv[2] : "239.1.1.1");
  int port = (argc>3 ? std::stoi(argv[3]) : 5005);

  // Send a few dummy ticks so the binary links & runs.
  int sock = socket(AF_INET, SOCK_DGRAM, 0);
  sockaddr_in addr{}; addr.sin_family=AF_INET; addr.sin_port=htons(port);
  addr.sin_addr.s_addr=inet_addr(host);

  for (int i=0;i<10;++i){
    char buf[64];
    int n = std::snprintf(buf, sizeof(buf), "%lld,%.4f\n",
                          (long long)(1704067200000LL + i*60000LL), 100.0 + i*0.1);
    sendto(sock, buf, n, 0, (sockaddr*)&addr, sizeof(addr));
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  std::puts("streamer_pi: sent 10 dummy ticks");
  return 0;
}
