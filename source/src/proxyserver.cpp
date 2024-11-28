#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <map>
#include <netinet/in.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <chrono>
#include <random>  

#define DEFAULT_ARGS_VALUE 0
#define DEFAULT_ARGS_CHANCE_VALUE 0.0


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

  // Random number generator setup
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
    std::cout << "Packet contains data: " << buffer << std::endl;

    bool is_from_server =
        (senderIPString == serverIPString && senderIPPort == serverIPPort);

    bool drop_packet = false;
    bool delay_packet = false;
    int delay_duration = 0;

    double random_value = dis(gen);  // Generate a random number between 0 and 1

    if (is_from_server) {
      drop_packet = random_value < server_drop_chance;
      random_value = dis(gen);  // Generate a new random number for delay
      delay_packet = random_value < server_delay_chance;
      delay_duration = server_delay_time;
    } else {
      drop_packet = random_value < client_drop_chance;
      random_value = dis(gen);  // Generate a new random number for delay
      delay_packet = random_value < client_delay_chance;
      delay_duration = client_delay_time;
    }

    if (drop_packet) {
      std::cout << "Packet dropped." << std::endl;
      continue;  
    }

    if (delay_packet) {
      std::cout << "Delaying packet for " << delay_duration << " milliseconds." << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(delay_duration));
    }

    if (is_from_server) {
      std::cout << "Received a response from the server" << std::endl;
      if (!client_addr_set) {
        std::cout << "No client address known, cannot forward to client"
                  << std::endl;
      } else {
        if (sendto(sock, buffer, recv_len, 0, (struct sockaddr *)&client_addr,
                   sizeof(client_addr)) < 0) {
          std::cout << "sendto error when forwarding to client" << std::endl;
        } else {
          std::cout << "Forwarded response back to client "
                    << inet_ntoa(client_addr.sin_addr) << ":"
                    << ntohs(client_addr.sin_port) << std::endl;
        }
      }
    } else {
      std::cout << "Received a packet from client" << std::endl;
      client_addr = sender_addr;
      client_addr_set = true;

      if (sendto(sock, buffer, recv_len, 0, (struct sockaddr *)&server_addr,
                 sizeof(server_addr)) < 0) {
        std::cout << "sendto error when forwarding to server" << std::endl;
      } else {
        std::cout << "Forwarded packet to server " << forwardIP << ":"
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
  int listenPort = DEFAULT_ARGS_VALUE;
  const char *forwardIP = nullptr;
  int forwardPort = DEFAULT_ARGS_VALUE;

  double client_drop_chance = 0.0;
  double server_drop_chance = 0.0;
  double client_delay_chance = 0.0;
  double server_delay_chance = 0.0;
  int client_delay_time = DEFAULT_ARGS_VALUE;
  int server_delay_time = DEFAULT_ARGS_VALUE;

  try {
    if (args.find("--listen-ip") != args.end()) {
      listenIP = args["--listen-ip"].c_str();
    } else {
      throw std::invalid_argument("Missing --listen-ip");
    }

    if (args.find("--listen-port") != args.end()) {
      listenPort = std::stoi(args["--listen-port"]);
    } else {
      throw std::invalid_argument("Missing --listen-port");
    }

    if (args.find("--target-ip") != args.end()) {
      forwardIP = args["--target-ip"].c_str();
    } else {
      throw std::invalid_argument("Missing --target-ip");
    }

    if (args.find("--target-port") != args.end()) {
      forwardPort = std::stoi(args["--target-port"]);
    } else {
      throw std::invalid_argument("Missing --target-port");
    }

    if (args.find("--client-drop") != args.end()) {
      client_drop_chance = std::stod(args["--client-drop"]);
    } else {
      throw std::invalid_argument("Missing --client-drop");
    }

    if (args.find("--server-drop") != args.end()) {
      server_drop_chance = std::stod(args["--server-drop"]);
    } else {
      throw std::invalid_argument("Missing --server-drop");
    }

    if (args.find("--client-delay") != args.end()) {
      client_delay_chance = std::stod(args["--client-delay"]);
    } else {
      throw std::invalid_argument("Missing --client-delay");
    }

    if (args.find("--server-delay") != args.end()) {
      server_delay_chance = std::stod(args["--server-delay"]);
    } else {
      throw std::invalid_argument("Missing --server-delay");
    }

    if (args.find("--client-delay-time") != args.end()) {
      client_delay_time = std::stoi(args["--client-delay-time"]);
    } else {
      throw std::invalid_argument("Missing --client-delay-time");
    }

    if (args.find("--server-delay-time") != args.end()) {
      server_delay_time = std::stoi(args["--server-delay-time"]);
    } else {
      throw std::invalid_argument("Missing --server-delay-time");
    }

  } catch (const std::exception &e) {
    std::cerr << "Error parsing arguments: " << e.what() << std::endl;
    std::cerr << "Usage: " << argv[0]
              << " --listen-ip <ip> --listen-port <port> --target-ip <ip> "
                 "--target-port <port> --client-drop <chance> --server-drop <chance> "
                 "--client-delay <chance> --server-delay <chance> "
                 "--client-delay-time <ms> --server-delay-time <ms>"
              << std::endl;
    std::cerr << "Example: " << argv[0]
              << " --listen-ip 127.0.0.1 --listen-port 8080 --target-ip 127.0.0.1 "
                 "--target-port 9090 --client-drop 0.1 --server-drop 0.1 "
                 "--client-delay 0.2 --server-delay 0.2 --client-delay-time 100 "
                 "--server-delay-time 100"
              << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "Proxy Server Configuration:" << std::endl;
  std::cout << "Listening on IP: " << listenIP << std::endl;
  std::cout << "Listening on Port: " << listenPort << std::endl;
  std::cout << "Forwarding to IP: " << forwardIP << std::endl;
  std::cout << "Forwarding to Port: " << forwardPort << std::endl;
  std::cout << "Client Drop Chance: " << client_drop_chance << std::endl;
  std::cout << "Server Drop Chance: " << server_drop_chance << std::endl;
  std::cout << "Client Delay Chance: " << client_delay_chance << std::endl;
  std::cout << "Server Delay Chance: " << server_delay_chance << std::endl;
  std::cout << "Client Delay Time: " << client_delay_time << " ms" << std::endl;
  std::cout << "Server Delay Time: " << server_delay_time << " ms" << std::endl;

  int voiceBufferSize = 5700;

  std::thread forwardingThread(
      forward_packets, listenIP, listenPort, forwardIP, forwardPort,
      voiceBufferSize, client_drop_chance, server_drop_chance,
      client_delay_chance, server_delay_chance,
      client_delay_time, server_delay_time);

  forwardingThread.join();

  return 0;
}