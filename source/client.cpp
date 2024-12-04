#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#define DEFAULTFDVALUE -1
#define NUMBOFARGS 7
#define BUFFERSIZE 1500
#define STARTING_SEQ 0

using namespace std;

class Client {
public:
  int socketFD, sequenceNumber;
  const int port, timeout;
  const string ipAddress;

  Client(const string ipAddress, const int port, const int timeout)
      : socketFD(DEFAULTFDVALUE), ipAddress(ipAddress), port(port),
        timeout(timeout), sequenceNumber(STARTING_SEQ) {}

  ~Client() { close_socket(socketFD); }
  int create_socket();
  void send_message(int socketFD, string message);
  void close_socket(int socketFD);
};

int main(int argc, char *argv[]) {
  if (argc != NUMBOFARGS) {
    cerr << "Incorrect amount of command line arguments, only provide IP "
            "Address, port and timeout value. Example: ./client --target-ip "
            "127.0.0.1 --target-port 80 --timeout 2"
         << endl;
    return EXIT_FAILURE;
  }

  string ipAddress = argv[2];
  int port = stoi(argv[4]);
  int timeout = stoi(argv[6]);

  Client *client = new Client(ipAddress, port, timeout);
  int socketFD = client->create_socket();

  string message;

  while (true) {
    cout << "Type your message (or type 'exit' to quit):\n";
    getline(cin, message);
    if (message == "exit") {
      break;
    }
    client->send_message(socketFD, message);
  }

  delete client;

  return 0;
}

int Client::create_socket() {

  socketFD = socket(AF_INET, SOCK_DGRAM, 0);
  if (socketFD == -1) {
    cerr << "Error in creating socket: " << strerror(errno) << endl;
    exit(EXIT_FAILURE);
  }

  struct timeval tv;
  tv.tv_sec = timeout;
  tv.tv_usec = 0;
  if (setsockopt(socketFD, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
    cerr << "Error setting socket's timeout: " << strerror(errno) << endl;
    exit(EXIT_FAILURE);
  }

  cout << "Socket created successfully" << endl;
  return socketFD;
}

void Client::send_message(int socketFD, string message) {
  struct sockaddr_in server_addr;
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(port);
  if (inet_pton(AF_INET, ipAddress.c_str(), &server_addr.sin_addr) <= 0) {
    cerr << "Invalid IP address format" << endl;
    exit(EXIT_FAILURE);
  }

  ssize_t request;
  char buffer[BUFFERSIZE];
  socklen_t server_len = sizeof(server_addr);

  string original_message = message;

  while (true) {

    string first_bit = to_string(sequenceNumber);
    string message_to_send = first_bit + original_message;

    request = sendto(socketFD, message_to_send.c_str(), message_to_send.size(),
                     0, (sockaddr *)&server_addr, server_len);
    if (request == -1) {
      cerr << "Error in sending request to server: " << strerror(errno) << endl;
      exit(EXIT_FAILURE);
    }
    cout << "Message sent to server with sequence number " << sequenceNumber
         << ", waiting " << timeout << " seconds for acknowledgment..." << endl;

    ssize_t ack = recvfrom(socketFD, buffer, BUFFERSIZE - 1, 0,
                           (sockaddr *)&server_addr, &server_len);
    if (ack > 0) {
      buffer[ack] = '\0';

      int ackSequenceNumber = buffer[0] - '0';

      cout << "Acknowledgment received with ack number: " << ackSequenceNumber
           << endl;

      if (ackSequenceNumber == sequenceNumber) {
        sequenceNumber = 1 - sequenceNumber;
        break;
      } else {
        cout << "Acknowledgment sequence number mismatch (expected "
             << sequenceNumber << ", got " << ackSequenceNumber
             << "), resending message..." << endl;
        continue;
      }
    }

    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      cout << "No acknowledgment received within timeout, resending message..."
           << endl;
      continue;
    } else {
      cerr << "Error in receiving acknowledgment: " << strerror(errno) << endl;
      exit(EXIT_FAILURE);
    }
  }
}

void Client::close_socket(int socketFD) {
  if (close(socketFD) == -1) {
    cerr << "Error closing socket: " << strerror(errno) << endl;
    exit(EXIT_FAILURE);
  }
  cout << "Socket closed successfully" << endl;
}
