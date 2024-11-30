#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <random>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

void forward_packets(const char *listen_ip, int listen_port,
                     const char *forwardIP, int forwardPort, int buffer_size,
                     double client_drop_chance, double server_drop_chance,
                     double client_delay_chance, double server_delay_chance,
                     int client_delay_time, int server_delay_time) {
  int sock;
  struct sockaddr_in listen_addr, sender_addr;
  char *buffer = new char[buffer_size];

  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    std::cout << "Error creating socket" << std::endl;
    delete[] buffer;
    return;
  }

  memset(&listen_addr, 0, sizeof(listen_addr));
  listen_addr.sin_family = AF_INET;
  listen_addr.sin_port = htons(listen_port);
  listen_addr.sin_addr.s_addr = inet_addr(listen_ip);

  if (bind(sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0) {
    std::cout << "Binding socket failed" << std::endl;
    close(sock);
    delete[] buffer;
    return;
  }

  std::cout << "Success, waiting for UDP packets on " << listen_ip << ":"
            << listen_port << std::endl;

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(forwardPort);
  server_addr.sin_addr.s_addr = inet_addr(forwardIP);

  struct sockaddr_in client_addr;
  memset(&client_addr, 0, sizeof(client_addr));
  bool client_addr_set = false;

  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> dis(0.0, 1.0);

  while (true) {
    socklen_t len = sizeof(sender_addr);

    ssize_t recv_len = recvfrom(sock, buffer, buffer_size, 0,
                                (struct sockaddr *)&sender_addr, &len);
    if (recv_len < 0) {
      std::cout << "recvfrom error" << std::endl;
      continue;
    }

    std::string serverIPString = forwardIP;
    int serverIPPort = forwardPort;

    std::string senderIPString = inet_ntoa(sender_addr.sin_addr);
    int senderIPPort = ntohs(sender_addr.sin_port);

    std::cout << "Received packet from " << senderIPString << ":"
              << senderIPPort << std::endl;
    std::cout << "Packet data: " << buffer << std::endl;

    bool is_from_server =
        (senderIPString == serverIPString && senderIPPort == serverIPPort);

    bool drop_packet_chance = false;
    bool delay_packet_chance = false;
    int delay_duration = 0;

    double random_value = dis(gen);

    if (is_from_server) {
      drop_packet_chance = random_value < server_drop_chance;
      random_value = dis(gen);
      delay_packet_chance = random_value < server_delay_chance;
      delay_duration = server_delay_time;
    } else {
      drop_packet_chance = random_value < client_drop_chance;
      random_value = dis(gen);
      delay_packet_chance = random_value < client_delay_chance;
      delay_duration = client_delay_time;
    }

    if (drop_packet_chance) {
      std::cout << "Dropped packet, chance was: " << drop_packet_chance
                << std::endl;
      continue;
    }

    if (delay_packet_chance) {
      std::cout << "Delaying packet for " << delay_duration << " ms"
                << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(delay_duration));
    }

    if (is_from_server) {
      std::cout << "Received response from the server" << std::endl;
      if (!client_addr_set) {
        std::cout << "No client address to forward to " << std::endl;
      } else {
        if (sendto(sock, buffer, recv_len, 0, (struct sockaddr *)&client_addr,
                   sizeof(client_addr)) < 0) {
          std::cout << "sendto error when forwarding the packet to the client"
                    << std::endl;
        } else {
          std::cout << "Forwarded response to client at ip "
                    << inet_ntoa(client_addr.sin_addr) << ":"
                    << ntohs(client_addr.sin_port) << std::endl;
        }
      }
    } else {
      std::cout << "Received packet from client" << std::endl;
      client_addr = sender_addr;
      client_addr_set = true;

      if (sendto(sock, buffer, recv_len, 0, (struct sockaddr *)&server_addr,
                 sizeof(server_addr)) < 0) {
        std::cout << "sendto error when forwarding packet to the server"
                  << std::endl;
      } else {
        std::cout << "forwarding the packet to the server" << forwardIP << ":"
                  << forwardPort << std::endl;
      }
    }
  }

  close(sock);
  delete[] buffer;
}

int main(int argc, char *argv[]) {
  std::map<std::string, std::string> args;

  for (int i = 1; i < argc - 1; i += 2) {
    std::string arg = argv[i];
    std::string value = argv[i + 1];
    args[arg] = value;
  }

  const char *listenIP = nullptr;
  int listenPort = 0;
  const char *forwardIP = nullptr;
  int forwardPort = 0;

  double client_drop_chance = 0.0;
  double server_drop_chance = 0.0;
  double client_delay_chance = 0.0;
  double server_delay_chance = 0.0;
  int client_delay_time = 0;
  int server_delay_time = 0;

  try {
    if (args.find("--listen-ip") != args.end()) {
      listenIP = args["--listen-ip"].c_str();
    } else {
      std::cout << "Missing --listen-ip" << std::endl;
      exit(1);
    }

    if (args.find("--listen-port") != args.end()) {
      listenPort = std::stoi(args["--listen-port"]);
    } else {
      std::cout << "Missing --listen-port" << std::endl;
      exit(1);
    }

    if (args.find("--target-ip") != args.end()) {
      forwardIP = args["--target-ip"].c_str();
    } else {
      std::cout << "Missing --target-ip" << std::endl;
      exit(1);
    }

    if (args.find("--target-port") != args.end()) {
      forwardPort = std::stoi(args["--target-port"]);
    } else {
      std::cout << "Missing --target-port" << std::endl;
      exit(1);
    }

    if (args.find("--client-drop") != args.end()) {
      client_drop_chance = std::stod(args["--client-drop"]);
    } else {
      std::cout << "Missing --client-drop" << std::endl;
      exit(1);
    }

    if (args.find("--server-drop") != args.end()) {
      server_drop_chance = std::stod(args["--server-drop"]);
    } else {
      std::cout << "Missing --server-drop" << std::endl;
      exit(1);
    }

    if (args.find("--client-delay") != args.end()) {
      client_delay_chance = std::stod(args["--client-delay"]);
    } else {
      std::cout << "Missing --client-delay" << std::endl;
      exit(1);
    }

    if (args.find("--server-delay") != args.end()) {
      server_delay_chance = std::stod(args["--server-delay"]);
    } else {
      std::cout << "Missing --server-delay" << std::endl;
      exit(1);
    }

    if (args.find("--client-delay-time") != args.end()) {
      client_delay_time = std::stoi(args["--client-delay-time"]);
    } else {
      std::cout << "Missing --client-delay-time" << std::endl;
      exit(1);
    }

    if (args.find("--server-delay-time") != args.end()) {
      server_delay_time = std::stoi(args["--server-delay-time"]);
    } else {
      std::cout << "Missing --server-delay-time" << std::endl;
      exit(1);
    }

  } catch (const std::exception &e) {
    std::cerr << "Error parsing arguments: " << e.what() << std::endl;
    std::cerr << "eg: " << argv[0]
              << " --listen-ip ip --listen-port port --target-ip ip "
                 "--target-port port --client-drop chance --server-drop chance"
                 "--client-delay chance --server-delay chance "
                 "--client-delay-time ms --server-delay-time ms"
              << std::endl;
    std::cerr
        << "eg: " << argv[0]
        << " --listen-ip 127.0.0.1 --listen-port 8080 --target-ip 127.0.0.1 "
           "--target-port 9090 --client-drop 0.1 --server-drop 0.1 "
           "--client-delay 0.2 --server-delay 0.2 --client-delay-time 100 "
           "--server-delay-time 100"
        << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "Listening IP: " << listenIP << std::endl;
  std::cout << "Listening Port: " << listenPort << std::endl;
  std::cout << "Forwarding IP: " << forwardIP << std::endl;
  std::cout << "Forwarding Port: " << forwardPort << std::endl;
  std::cout << "Client Drop %: " << client_drop_chance << std::endl;
  std::cout << "Server Drop %: " << server_drop_chance << std::endl;
  std::cout << "Client Delay %: " << client_delay_chance << std::endl;
  std::cout << "Server Delay %: " << server_delay_chance << std::endl;
  std::cout << "Client Delay time: " << client_delay_time << " ms" << std::endl;
  std::cout << "Server Delay time: " << server_delay_time << " ms" << std::endl;

  int bufferSize = 5700;

  std::thread forwardingThread(
      forward_packets, listenIP, listenPort, forwardIP, forwardPort, bufferSize,
      client_drop_chance, server_drop_chance, client_delay_chance,
      server_delay_chance, client_delay_time, server_delay_time);

  forwardingThread.join();

  return 0;
}
