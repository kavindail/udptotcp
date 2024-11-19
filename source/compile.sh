g++ client.cpp -o client
echo "Compiled client"
g++ server.cpp -o server
echo "Compiled server"
g++ -std=c++11 -pthread -o proxyserver proxyserver.cpp
echo "Compiled proxy server"
