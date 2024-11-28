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

using namespace std;

void forward_packets(const char *listen_ip, int listen_port,
                     const char *forwardIP, int forwardPort, int buffer_size,
                     double client_drop_chance, double server_drop_chance,
                     double client_delay_chance, double server_delay_chance,
                     int client_delay_time, int server_delay_time) {
  int sock;
  struct sockaddr_in listen_addr, sender_addr;
  char *buffer = new char[buffer_size + 1];  

  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    cout << "Error creating socket" << endl;
    delete[] buffer;
    return;
  }

  memset(&listen_addr, 0, sizeof(listen_addr));
  listen_addr.sin_family = AF_INET;
  listen_addr.sin_port = htons(listen_port);
  listen_addr.sin_addr.s_addr = inet_addr(listen_ip);

  if (bind(sock, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0) {
    cout << "Binding socket failed" << endl;
    close(sock);
    delete[] buffer;
    return;
  }

  cout << "Success, waiting for UDP packets on " << listen_ip << ":"
            << listen_port << endl;

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(forwardPort);
  server_addr.sin_addr.s_addr = inet_addr(forwardIP);

  struct sockaddr_in client_addr;
  memset(&client_addr, 0, sizeof(client_addr));
  bool client_addr_set = false;

  // Random number generator setup
  random_device rd;
  mt19937 gen(rd());
  uniform_real_distribution<> dis(0.0, 1.0);

  while (true) {
    // Clear the buffer at the start of each iteration
    memset(buffer, 0, buffer_size + 1);

    socklen_t len = sizeof(sender_addr);

    ssize_t recv_len = recvfrom(sock, buffer, buffer_size, 0,
                                (struct sockaddr *)&sender_addr, &len);
    if (recv_len < 0) {
      cout << "recvfrom error" << endl;
      continue;
    }

    // Null-terminate the buffer
    buffer[recv_len] = '\0';

    string serverIPString = forwardIP;
    int serverIPPort = forwardPort;

    string senderIPString = inet_ntoa(sender_addr.sin_addr);
    int senderIPPort = ntohs(sender_addr.sin_port);

    cout << "Received packet from " << senderIPString << ":"
              << senderIPPort << endl;
    cout << "Packet contains data: " << buffer << endl;

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
      cout << "Packet dropped." << endl;
      continue;
    }

    if (delay_packet) {
      cout << "Delaying packet for " << delay_duration << " milliseconds."
                << endl;
      this_thread::sleep_for(chrono::milliseconds(delay_duration));
    }

    if (is_from_server) {
      cout << "Received a response from the server" << endl;
      if (!client_addr_set) {
        cout << "No client address known, cannot forward to client"
                  << endl;
      } else {
        if (sendto(sock, buffer, recv_len, 0, (struct sockaddr *)&client_addr,
                   sizeof(client_addr)) < 0) {
          cout << "sendto error when forwarding to client" << endl;
        } else {
          cout << "Forwarded response back to client "
                    << inet_ntoa(client_addr.sin_addr) << ":"
                    << ntohs(client_addr.sin_port) << endl;
        }
      }
    } else {
      cout << "Received a packet from client" << endl;
      client_addr = sender_addr;
      client_addr_set = true;

      if (sendto(sock, buffer, recv_len, 0, (struct sockaddr *)&server_addr,
                 sizeof(server_addr)) < 0) {
        cout << "sendto error when forwarding to server" << endl;
      } else {
        cout << "Forwarded packet to server " << forwardIP << ":"
                  << forwardPort << endl;
      }
    }

    // Clear the buffer after processing the packet
    memset(buffer, 0, buffer_size + 1);
  }

  close(sock);
  delete[] buffer;
}

int main(int argc, char *argv[]) {
  map<string, string> args;

  for (int i = 1; i < argc - 1; i += 2) {
    string arg = argv[i];
    string value = argv[i + 1];
    args[arg] = value;
  }

  const char *listenIP = nullptr;
  int listenPort = DEFAULT_ARGS_VALUE;
  const char *forwardIP = nullptr;
  int forwardPort = DEFAULT_ARGS_VALUE;

  double client_drop_chance = DEFAULT_ARGS_CHANCE_VALUE;
  double server_drop_chance = DEFAULT_ARGS_CHANCE_VALUE;
  double client_delay_chance = DEFAULT_ARGS_CHANCE_VALUE;
  double server_delay_chance = DEFAULT_ARGS_CHANCE_VALUE;
  int client_delay_time = DEFAULT_ARGS_VALUE;
  int server_delay_time = DEFAULT_ARGS_VALUE;

  try {
    if (args.find("--listen-ip") != args.end()) {
      listenIP = args["--listen-ip"].c_str();
    } else {
      throw invalid_argument("Missing --listen-ip");
    }

    if (args.find("--listen-port") != args.end()) {
      listenPort = stoi(args["--listen-port"]);
    } else {
      throw invalid_argument("Missing --listen-port");
    }

    if (args.find("--target-ip") != args.end()) {
      forwardIP = args["--target-ip"].c_str();
    } else {
      throw invalid_argument("Missing --target-ip");
    }

    if (args.find("--target-port") != args.end()) {
      forwardPort = stoi(args["--target-port"]);
    } else {
      throw invalid_argument("Missing --target-port");
    }

    if (args.find("--client-drop") != args.end()) {
      client_drop_chance = stod(args["--client-drop"]);
    } else {
      throw invalid_argument("Missing --client-drop");
    }

    if (args.find("--server-drop") != args.end()) {
      server_drop_chance = stod(args["--server-drop"]);
    } else {
      throw invalid_argument("Missing --server-drop");
    }

    if (args.find("--client-delay") != args.end()) {
      client_delay_chance = stod(args["--client-delay"]);
    } else {
      throw invalid_argument("Missing --client-delay");
    }

    if (args.find("--server-delay") != args.end()) {
      server_delay_chance = stod(args["--server-delay"]);
    } else {
      throw invalid_argument("Missing --server-delay");
    }

    if (args.find("--client-delay-time") != args.end()) {
      client_delay_time = stoi(args["--client-delay-time"]);
    } else {
      throw invalid_argument("Missing --client-delay-time");
    }

    if (args.find("--server-delay-time") != args.end()) {
      server_delay_time = stoi(args["--server-delay-time"]);
    } else {
      throw invalid_argument("Missing --server-delay-time");
    }

  } catch (const exception &e) {
    cerr << "Error parsing arguments: " << e.what() << endl;
    cerr << "Usage: " << argv[0]
              << " --listen-ip <ip> --listen-port <port> --target-ip <ip> "
                 "--target-port <port> --client-drop <chance> --server-drop "
                 "<chance> "
                 "--client-delay <chance> --server-delay <chance> "
                 "--client-delay-time <ms> --server-delay-time <ms>"
              << endl;
    cerr << "Example: " << argv[0]
              << " --listen-ip 127.0.0.1 --listen-port 8080 --target-ip "
                 "127.0.0.1 "
                 "--target-port 9090 --client-drop 0.1 --server-drop 0.1 "
                 "--client-delay 0.2 --server-delay 0.2 --client-delay-time "
                 "100 "
                 "--server-delay-time 100"
              << endl;
    return EXIT_FAILURE;
  }

  cout << "Proxy Server Configuration:" << endl;
  cout << "Listening on IP: " << listenIP << endl;
  cout << "Listening on Port: " << listenPort << endl;
  cout << "Forwarding to IP: " << forwardIP << endl;
  cout << "Forwarding to Port: " << forwardPort << endl;
  cout << "Client Drop Chance: " << client_drop_chance << endl;
  cout << "Server Drop Chance: " << server_drop_chance << endl;
  cout << "Client Delay Chance: " << client_delay_chance << endl;
  cout << "Server Delay Chance: " << server_delay_chance << endl;
  cout << "Client Delay Time: " << client_delay_time << " ms" << endl;
  cout << "Server Delay Time: " << server_delay_time << " ms" << endl;

  int voiceBufferSize = 5700;

  thread forwardingThread(
      forward_packets, listenIP, listenPort, forwardIP, forwardPort,
      voiceBufferSize, client_drop_chance, server_drop_chance,
      client_delay_chance, server_delay_chance, client_delay_time,
      server_delay_time);

  forwardingThread.join();

  return 0;
}
