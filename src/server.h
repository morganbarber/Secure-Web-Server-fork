#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <unordered_map>
#include <functional>
#include <fstream>

class WebServer {
public:
    WebServer(int port) : port(port) {}

    void start() {
        int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket == -1) {
            std::cerr << "Failed to create socket" << std::endl;
            return;
        }

        sockaddr_in serverAddress;
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_addr.s_addr = INADDR_ANY;
        serverAddress.sin_port = htons(port);

        if (bind(serverSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
            std::cerr << "Failed to bind socket" << std::endl;
            return;
        }

        listen(serverSocket, 3);

        std::cout << "Server started on port " << port << std::endl;

        int clientSocket;
        sockaddr_in clientAddress;
        int clientAddressLength = sizeof(clientAddress);

        while ((clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddress, (socklen_t *)&clientAddressLength))) {
            std::cout << "New connection accepted" << std::endl;

            handleRequest(clientSocket);

            close(clientSocket);
        }

        close(serverSocket);
    }

    void registerFunction(const std::string& filepath, std::function<std::string()> function) {
        functions[filepath] = function;
    }

    void registerFile(const std::string& filepath, const std::string& filename) {
        functions[filepath] = [filename]() {
            std::string response = "HTTP/1.1 200 OK\nContent-Type: text/html\n\n";
            std::string line;
            std::ifstream file(filename);

            try {
                while (getline(file, line)) {
                    response += line + "\n";
                }
                file.close();
            }
            catch (const std::exception& e) {
                std::cout << "Failed to read file " << filename << std::endl;
                response = "HTTP/1.1 404 Not Found\nContent-Type: text/html\n\n404 Not Found";
            }

            return response;
        };
    }

private:
    int port;
    std::unordered_map<std::string, std::function<std::string()>> functions;

    void handleRequest(int clientSocket) {
        char buffer[1024] = {0};
        std::string response;

        read(clientSocket, buffer, 1024);
        std::cout << "Received request:\n" << buffer << std::endl;

        std::string request(buffer);
        std::string filepath = extractFilePath(request);

        if (functions.find(filepath) != functions.end()) {
            response = functions[filepath]();
        } else {
            response = "HTTP/1.1 404 Not Found\nContent-Type: text/html\n\n404 Not Found";
        }

        send(clientSocket, response.c_str(), response.length(), 0);
        std::cout << "Sent response:\n" << response << std::endl;
    }

    std::string extractFilePath(const std::string& request) {
        std::string filepath;

        size_t start = request.find("GET ") + 4;
        size_t end = request.find(" HTTP/1.1");

        if (start != std::string::npos && end != std::string::npos) {
            filepath = request.substr(start, end - start);
        }

        return filepath;
    }
};
