#include <iostream>
#include "server.h"
#include <string>
#include <functional>


int main() {
    WebServer server(8080);

    server.registerFunction("/add", []() {
        std::string request;

        size_t start = request.find("num1=") + 5;
        size_t end = request.find("&num2=");

        if (start != std::string::npos && end != std::string::npos) {
            std::string num1 = request.substr(start, end - start);

            start = end + 6;
            end = request.find(" HTTP/1.1");

            if (start != std::string::npos && end != std::string::npos) {
                std::string num2 = request.substr(start, end - start);

                try {
                    int result = std::stoi(num1) + std::stoi(num2);
                    response = "HTTP/1.1 200 OK\nContent-Type: text/html\n\n";
                    response += "<h1>Result: " + std::to_string(result) + "</h1>";
                } catch (const std::exception& e) {
                    response = "HTTP/1.1 400 Bad Request\nContent-Type: text/html\n\n400 Bad Request";
                }
            }
        }

        return response;
    });

    server.registerFile("/", "index.html");

    server.start();

    return 0;
}
