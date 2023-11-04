#include <iostream>
#include <string>
#include "server.h"

int main() {
    // Create server instance
    Server server(8080, 10, "log.txt");

    // Open index.html file
    server.open("index.html", "/");

    // Start server
    server.start();

    return 0;
}

void connection_loop(int client_fd) {
    // Read incoming request
    std::string request;
    char buffer[1024] = {0};
    int bytes_read = read(client_fd, buffer, 1024);
    if (bytes_read < 0) {
        std::cerr << "Failed to read incoming request" << std::endl;
        close(client_fd);
        return;
    }
    request = buffer;

    // Parse request
    std::regex request_regex("GET /(.*) HTTP/1\\.1");
    std::smatch request_match;
    if (!std::regex_search(request, request_match, request_regex)) {
        std::cerr << "Failed to parse incoming request" << std::endl;
        close(client_fd);
        return;
    }
    std::string url = request_match[1].str();

    // Check if URL is in filepaths
    if (filepaths.find(url) == filepaths.end()) {
        std::cerr << "Failed to find file for URL " << url << std::endl;
        close(client_fd);
        return;
    }
    std::string filepath = filepaths[url];

    // Check rate limiter
    std::unique_lock<std::mutex> limiter_lock(limiter_mutex);
    limiter_cv.wait(limiter_lock, [this]() {
        if (limiter_queue.empty()) {
            return true;
        }
        auto oldest_request_time = limiter_queue.front();
        auto now = std::chrono::steady_clock::now();
        auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(now - oldest_request_time);
        return elapsed_time.count() >= 1000 / max_connections_per_second;
    });
    limiter_queue.push(std::chrono::steady_clock::now());
    limiter_lock.unlock();

    // Generate response
    std::ifstream file(filepath);
    if (!file.is_open()) {
        std::cerr << "Failed to open file " << filepath << std::endl;
        close(client_fd);
        return;
    }
    std::stringstream file_contents;
    file_contents << file.rdbuf();
    std::string file_extension = filepath.substr(filepath.find_last_of(".") + 1);
    std::string content_type;
    if (file_extension == "html") {
        content_type = "text/html";
    } else if (file_extension == "css") {
        content_type = "text/css";
    } else if (file_extension == "js") {
        content_type = "application/javascript";
    } else {
        content_type = "application/octet-stream";
    }

    // Perform addition operation
    std::regex addition_regex("GET /addition\\?a=(\\d+)&b=(\\d+) HTTP/1\\.1");
    std::smatch addition_match;
    if (std::regex_search(request, addition_match, addition_regex)) {
        int a = std::stoi(addition_match[1].str());
        int b = std::stoi(addition_match[2].str());
        int result = a + b;
        file_contents << "<p>" << a << " + " << b << " = " << result << "</p>";
    }

    std::string response = "HTTP/1.1 200 OK\nContent-Type: " + content_type + "\n\n" + file_contents.str();

    // Send response
    int bytes_sent = send(client_fd, response.c_str(), response.length(), 0);
    if (bytes_sent < 0) {
        std::cerr << "Failed to send response" << std::endl;
    }

    close(client_fd);
}
