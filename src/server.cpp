#include <algorithm>
#include <array>
#include <cstddef>
#include <iostream>
#include <cstdlib>
#include <iterator>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ranges>
#include <string_view>

int find_start_sequnce_index(const std::string& request_message);
std::string find_string_in_between(const std::string& first, const std::string& second, const std::string& line);
std::string get_response_message(const std::string& request_message);

int main(int argc, char **argv) 
{
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    std::cout << "Logs from your program will appear here!\n";

    int server_fd{socket(AF_INET, SOCK_STREAM, 0)};

    if (server_fd < 0) 
    {
        std::cerr << "Failed to create server socket\n";
        return 1;
    }

    int reuse{1};

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) 
    {
        std::cerr << "setsockopt failed\n";
        return 1;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(4221); 
    
    if (bind(server_fd, (struct sockaddr *) &server_addr, sizeof(server_addr)) != 0) 
    {
        std::cerr << "Failed to bind to port 4221\n";
        return 1;
    }

    int connection_backlog{5};

    if (listen(server_fd, connection_backlog) != 0) 
    {
        std::cerr << "listen failed\n";
        return 1;
    }

    struct sockaddr_in client_addr;
    int client_addr_len{sizeof(client_addr)};

    std::cout << "Waiting for a client to connect...\n";

    int client_fd{accept(server_fd, (struct sockaddr *) &client_addr, (socklen_t *) &client_addr_len)};

    if(client_fd < 0)
    {
        std::cerr << "Failed to create client socket\n";
        return 1;
    }
    
    std::cout << "Client connected\n";
 
    std::string request_message_buffer(1024, '\0');
    
    ssize_t bytes_accepted{recv(client_fd, static_cast<void*>(&request_message_buffer[0]), request_message_buffer.capacity(), MSG_PEEK)};

    if(bytes_accepted < 0)
    {
        std::cerr << "Failed to accept message";
        return 1;
    }

    std::string message{get_response_message(request_message_buffer)};

    ssize_t bytes_send{send(client_fd, message.c_str(), message.length(), MSG_EOR)};

    if(bytes_send < 0)
    {
        std::cerr << "Failed to send message\n";
        return 1;
    }

    close(server_fd);
    close(client_fd);
    return 0;
}

int find_start_sequnce_index(const std::string& request_message)
{
    static const std::array<std::string_view, 3> start_sequence{"GET / ", "GET /echo/", "GET /user-agent "};
    for (const auto& [index, line] : start_sequence | std::views::enumerate) 
    {
        if(request_message.find(line) != std::string::npos)
        {
            return index;
        }
    }
    return -1;
}

std::string find_string_in_between(const std::string& first, const std::string& second, const std::string& line)
{
    std::size_t start_index{line.find(first) + first.length()};
    std::size_t string_length{line.substr(start_index).find(second) - start_index - 1};
    std::string string_in_between{line.substr(start_index, string_length)};
    return string_in_between;
}

std::string get_response_message(const std::string& request_message)
{
    std::string message{};    
    switch (find_start_sequnce_index(request_message))
    {
    case 0:
        message = "HTTP/1.1 200 OK\r\n\r\n";
        break;
    case 1:
        {
            std::string response{find_string_in_between("echo/", " HTTP", request_message)};
            message = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(response.length()) + "\r\n\r\n" + response;
        }
        break;
    case 2:
        {
            std::string response{find_string_in_between("User-Agent: ", "\r\n", request_message)};
            message = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(response.length()) + "\r\n\r\n" + response;
        }
        break;
    default:
        message = "HTTP/1.1 404 Not Found\r\n\r\n";
        break;
    }
    return message;
}
