#include <arpa/inet.h>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <unistd.h>

#define DEFAULTFDVALUE -1
#define NUMBOFARGS 5
#define BUFFERSIZE 1024

using namespace std;

class Server {
public:
  int socketFD;
  const int port;
  const string listenIP;

  Server(const string& listenIP, const int port)
      : socketFD(DEFAULTFDVALUE), port(port), listenIP(listenIP) {}

  ~Server() { close_socket(socketFD); }

  int create_socket();
  void bind_socket();
  void print_message();
  void close_socket(int socketFD);
};

volatile sig_atomic_t exit_flag = 0;

void sigint_handler(int signum) { exit_flag = 1; }

void setup_signal_handler() {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = sigint_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  if (sigaction(SIGINT, &sa, NULL) == -1) {
    perror("sigaction");
    exit(EXIT_FAILURE);
  }
}

int main(int argc, char* argv[]) {
  setup_signal_handler();

  if (argc != NUMBOFARGS) {
    cerr << "Incorrect number of command line arguments. Usage: " << argv[0]
         << " --listen-ip <ip> --listen-port <port>" << endl;
    return EXIT_FAILURE;
  }

  if (string(argv[1]) != "--listen-ip") { cerr << "Expected --listen-ip as first argument." << endl;
    return EXIT_FAILURE;
  }
  string listenIP = argv[2];

  if (string(argv[3]) != "--listen-port") {
    cerr << "Expected --listen-port as third argument." << endl;
    return EXIT_FAILURE;
  }
  int port = stoi(argv[4]);

  cout << "Server has started" << endl;

  Server* server = new Server(listenIP, port);
  server->create_socket();
  server->bind_socket();
  while (!exit_flag) {
    server->print_message();
  }

  delete server;

  return 0;
}

int Server::create_socket() {
  socketFD = socket(AF_INET, SOCK_DGRAM, 0);
  if (socketFD == -1) {
    cerr << "Error in creating socket: " << strerror(errno) << endl;
    exit(EXIT_FAILURE);
  }
  cout << "Socket created successfully" << endl;
  return socketFD;
}

void Server::bind_socket() {
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = inet_addr(listenIP.c_str());
  addr.sin_port = htons(port);

  if (bind(socketFD, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
    cerr << "Error in binding socket: " << strerror(errno) << endl;
    exit(EXIT_FAILURE);
  }

  cout << "Socket successfully bound at " << listenIP << " port " << port << endl;
}

void Server::print_message() {
  struct sockaddr_in client_addr;
  socklen_t client_addr_size = sizeof(client_addr);
  char buffer[BUFFERSIZE];

  ssize_t bytesReceived =
      recvfrom(socketFD, buffer, BUFFERSIZE - 1, 0,
               (struct sockaddr*)&client_addr, &client_addr_size);
  if (bytesReceived == -1) {
    cerr << "Error receiving message: " << strerror(errno) << endl;
    return;
  }

  buffer[bytesReceived] = '\0';
  cout << "Received message: " << buffer << endl;

  const char* ackMessage = "ACK";
  ssize_t ackBytes = sendto(socketFD, ackMessage, strlen(ackMessage), 0,
                            (struct sockaddr*)&client_addr, client_addr_size);
  if (ackBytes == -1) {
    cerr << "Error sending acknowledgment: " << strerror(errno) << endl;
  } else {
    cout << "Acknowledgment sent to client" << endl;
  }
}

void Server::close_socket(int socketFD) {
  if (close(socketFD) == -1) {
    cerr << "Error closing socket: " << strerror(errno) << endl;
    exit(EXIT_FAILURE);
  }
  cout << "Socket closed successfully" << endl;
}
