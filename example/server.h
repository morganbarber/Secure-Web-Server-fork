
#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <queue>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <regex>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

class Server {
public:
    Server(int port, int max_connections_per_second, std::string log_file_path) :
        port(port),
        max_connections_per_second(max_connections_per_second),
        log_file_path(log_file_path),
        running(false),
        server_fd(0),
        limiter_thread(&Server::limiter_loop, this),
        logger_thread(&Server::logger_loop, this)
    {}

    void start() {
        running = true;

        // Create server socket
        server_fd = socket(AF_INET, SOCK_STREAM, 0);
        if (server_fd == 0) {
            std::cerr << "Failed to create socket" << std::endl;
            return;
        }

        // Bind socket to port
        sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(port);
        if (bind(server_fd, (sockaddr*)&address, sizeof(address)) < 0) {
            std::cerr << "Failed to bind socket to port " << port << std::endl;
            return;
        }

        // Listen for incoming connections
        if (listen(server_fd, 5) < 0) {
            std::cerr << "Failed to listen for incoming connections" << std::endl;
            return;
        }

        // Start limiter and logger threads
        limiter_thread.detach();
        logger_thread.detach();

        // Accept incoming connections and spawn new threads to handle them
        while (running) {
            int client_fd = accept(server_fd, nullptr, nullptr);
            if (client_fd < 0) {
                std::cerr << "Failed to accept incoming connection" << std::endl;
                continue;
            }

            std::thread connection_thread(&Server::connection_loop, this, client_fd);
            connection_thread.detach();
        }
    }

    void stop() {
        running = false;
        close(server_fd);
    }

    void open(std::string filepath, std::string url) {
        filepaths[url] = filepath;
    }

private:
    int port;
    int max_connections_per_second;
    std::string log_file_path;
    bool running;
    int server_fd;
    std::thread limiter_thread;
    std::thread logger_thread;
    std::mutex limiter_mutex;
    std::condition_variable limiter_cv;
    std::queue<std::chrono::time_point<std::chrono::steady_clock>> limiter_queue;
    std::mutex logger_mutex;
    std::queue<std::string> logger_queue;
    std::unordered_map<std::string, std::string> filepaths;

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
        std::string response = "HTTP/1.1 200 OK\nContent-Type: " + content_type + "\n\n" + file_contents.str();

        // Send response
        int bytes_sent = send(client_fd, response.c_str(), response.length(), 0);
        if (bytes_sent < 0) {
            std::cerr << "Failed to send response" << std::endl;
        }

        close(client_fd);
    }

    void limiter_loop() {
        while (running) {
            std::unique_lock<std::mutex> limiter_lock(limiter_mutex);
            if (!limiter_queue.empty()) {
                auto oldest_request_time = limiter_queue.front();
                auto now = std::chrono::steady_clock::now();
                auto elapsed_time = std::chrono::duration_cast<std::chrono::milliseconds>(now - oldest_request_time);
                if (elapsed_time.count() < 1000 / max_connections_per_second) {
                    limiter_cv.wait_for(limiter_lock, std::chrono::milliseconds(1000 / max_connections_per_second) - elapsed_time);
                }
                limiter_queue.pop();
            } else {
                limiter_cv.wait(limiter_lock);
            }
        }
    }

    void logger_loop() {
        while (running) {
            std::unique_lock<std::mutex> logger_lock(logger_mutex);
            if (!logger_queue.empty()) {
                std::ofstream log_file(log_file_path, std::ios_base::app);
                if (log_file.is_open()) {
                    log_file << logger_queue.front() << std::endl;
                    logger_queue.pop();
                }
            } else {
                logger_lock.unlock();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
    }
};
