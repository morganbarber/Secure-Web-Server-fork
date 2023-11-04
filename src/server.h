
#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <ctime>
#include <fstream>
#include <sstream>
#include <vector>
#include <map>
#include <regex>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

class WebServer {
public:
    WebServer(int port) : port(port) {}

    void start() {
        int server_fd, new_socket;
        struct sockaddr_in address;
        int opt = 1;
        int addrlen = sizeof(address);

        // Create socket file descriptor
        if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
            perror("socket failed");
            exit(EXIT_FAILURE);
        }

        // Attach socket to the port
        if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
            perror("setsockopt");
            exit(EXIT_FAILURE);
        }
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);

        if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
            perror("bind failed");
            exit(EXIT_FAILURE);
        }

        if (listen(server_fd, 3) < 0) {
            perror("listen");
            exit(EXIT_FAILURE);
        }

        while (true) {
            if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
                perror("accept");
                exit(EXIT_FAILURE);
            }

            std::thread t(&WebServer::handleRequest, this, new_socket);
            t.detach();
        }
    }

private:
    int port;

    void handleRequest(int socket) {
        // Read the incoming request
        char buffer[1024] = {0};
        read(socket, buffer, 1024);

        // Parse the request using an HTML parser
        std::string request(buffer);
        std::regex regex_method("^([A-Z]+)\\s");
        std::smatch match_method;
        std::regex_search(request, match_method, regex_method);
        std::string method = match_method[1];
        std::regex regex_path("^([A-Z]+)\\s([^\\s]+)");
        std::smatch match_path;
        std::regex_search(request, match_path, regex_path);
        std::string path = match_path[2];

        // Implement a rate limiter to control traffic
        static std::map<std::string, int> request_count;
        static std::time_t last_time = std::time(nullptr);
        std::time_t current_time = std::time(nullptr);
        if (current_time - last_time > 1) {
            request_count.clear();
            last_time = current_time;
        }
        request_count[path]++;
        if (request_count[path] > 10) {
            std::string response = "HTTP/1.1 429 Too Many Requests\r\nContent-Length: 0\r\n\r\n";
            send(socket, response.c_str(), response.size(), 0);
            close(socket);
            return;
        }

        // Use a logging system to record any valuable information
        std::ofstream log_file("log.txt", std::ios_base::app);
        std::time_t now = std::time(nullptr);
        std::tm* local_time = std::localtime(&now);
        char time_str[20];
        std::strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", local_time);
        log_file << time_str << " " << method << " " << path << std::endl;

        // Serve the HTML file
        std::ifstream file(path);
        if (!file.is_open()) {
            std::string response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
            send(socket, response.c_str(), response.size(), 0);
            close(socket);
            return;
        }
        std::stringstream buffer_stream;
        buffer_stream << file.rdbuf();
        std::string file_content = buffer_stream.str();
        std::string response = "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(file_content.size()) + "\r\n\r\n" + file_content;
        send(socket, response.c_str(), response.size(), 0);
        close(socket);
    }
};
