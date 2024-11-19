#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#define NUMBOFARGS 5

void forward_packets(const char *listen_ip, int listen_port,
                     const char *forwardIP, int forwardPort, int buffer_size) {
  int sock;
  struct sockaddr_in listen_addr, sender_addr;
  char buffer[buffer_size];

  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    std::cout << "Error creating socket" << std::endl;
    return;
  }

  memset(&listen_addr, 0, sizeof(listen_addr));
  listen_addr.sin_family = AF_INET;
  listen_addr.sin_port = htons(listen_port);
  listen_addr.sin_addr.s_addr = inet_addr(listen_ip);

  if (bind(sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0) {
    std::cout << "Binding socket failed" << std::endl;
    close(sock);
    return;
  }

  std::cout << "Success, waiting for udp packets on " << listen_ip << ":"
            << listen_port << std::endl;

  while (true) {
    socklen_t len = sizeof(sender_addr);

    ssize_t recv_len = recvfrom(sock, buffer, buffer_size, 0,
                                (struct sockaddr *)&sender_addr, &len);
    if (recv_len < 0) {
      std::cout << "recvfrom error" << std::endl;
      break;
    }

    std::cout << "Received packet from " << inet_ntoa(sender_addr.sin_addr)
              << ":" << ntohs(sender_addr.sin_port) << std::endl;

    std::cout << "Packet contains data:  " << buffer << std::endl;

    if (sendto(sock, buffer, recv_len, 0, (struct sockaddr *)&sender_addr,
               len) < 0) {
      std::cout << "sendto error" << std::endl;
      break;
    }

    // TODO: Change this to use the port and ip taken in from the command line
    // arguments rather than it just being a mirror
    std::cout << "Forwarded packet back to " << inet_ntoa(sender_addr.sin_addr)
              << ":" << ntohs(sender_addr.sin_port) << std::endl;
  }

  close(sock);
}

int main(int argc, char *argv[]) {

  if (argc != NUMBOFARGS) {
    std::cerr << "Incorrect amount of command line arguments,  "
                 "Provide IP Address to listen on, Port to listen on, IP "
                 "Address to forward to, Port to forward to"
                 ""
              << std::endl;
    return EXIT_FAILURE;
  }

  const char *listenIP = argv[1];
  int listenPort = std::stoi(argv[2]);
  // TODO: Need to change this so it actualy forwards to the port and IP listed
  const char *forwardIP = argv[3];
  int forwardPort = std::stoi(argv[4]);
  std::cout << "Listening on ip: " << listenIP << std::endl;
  std::cout << "Listening on port: " << listenPort << std::endl;
  std::cout << "Forwarding to ip: " << forwardIP << std::endl;
  std::cout << "Forwarding to port: " << forwardPort << std::endl;

  int voiceBufferSize = 5700;

  std::thread forwardingThread(forward_packets, listenIP, listenPort, forwardIP,
                               forwardPort, voiceBufferSize);

  forwardingThread.join();

  return 0;
}
