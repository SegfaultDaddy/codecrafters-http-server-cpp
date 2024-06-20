#include <algorithm>
#include <array>
#include <complex>
#include <cstddef>
#include <fstream>
#include <functional>
#include <ios>
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
#include <thread>
#include <optional>
#include <variant>
#include <zlib.h>

constexpr std::size_t max_clients{8};

struct Client
{
    struct sockaddr_in address;
    int address_length{sizeof(address)};
    int file_descriptor;
};

int find_start_sequnce_index(const std::string& request_message);
std::string find_string_in_between(const std::string& first, const std::string& second, const std::string& line);
std::optional<std::string> check_for_compression_header(const std::string& request_message);
std::optional<std::string> read_file(const std::string& filename, const std::string& directory_path);
void write_file(const std::string& filename, const std::string& directory_path, const std::string& text);
std::string gzip_compression(const std::string& message_to_compress);
std::string get_response_message(const std::string& request_message, const std::string& directory_path);
int send_server_response(int client_file_descriptor, int server_file_descriptor, const std::string& directory_path);

int main(int argc, char **argv) 
{
    std::cout << std::unitbuf;
    std::cerr << std::unitbuf;

    std::string directory_path{};
    if(argc > 1)
    {
        if(std::strcmp(argv[1], "--directory") == 0)
        {
            directory_path = argv[2];
        }
    }

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

    std::array<Client, max_clients> clients;
    std::array<std::thread, max_clients> threads; 
    
    for(std::size_t index{0}; index < max_clients; ++index)
    {
        clients[index].file_descriptor = accept(server_fd, (struct sockaddr *)&clients[index].address, (socklen_t *)&clients[index].address_length); 
        
        if(clients[index].file_descriptor < 0)
        {
            std::cerr << "Failed to create client socket\n";
            return 1;
        }

        threads[index] = std::thread{send_server_response, clients[index].file_descriptor, server_fd, directory_path};
        threads[index].join();
    }

    for(std::size_t index{0}; index < max_clients; ++index)
    {
        close(clients[index].file_descriptor);
    } 

    close(server_fd);
    return 0;
}

int find_start_sequnce_index(const std::string& request_message)
{
    static const std::array<std::string_view, 5> start_sequence{"GET / ", "GET /echo/", "GET /user-agent ", "GET /files", "POST /files"};
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
    std::size_t string_length{line.substr(start_index).find(second)};
    std::string string_in_between{line.substr(start_index, string_length)};
    return string_in_between;
}

std::optional<std::string> check_for_compression_header(const std::string& request_message)
{
    static const std::string compression_header{"Accept-Encoding: "};
    static const std::array<std::string, 1> available_encoding{"gzip"};
    for(const auto& encoding : available_encoding)
    {
        if(request_message.find(compression_header) != std::string::npos && request_message.find(encoding) != std::string::npos)
        {
            return encoding;
        }
    }
    return std::nullopt;
}

std::optional<std::string> read_file(const std::string& filename, const std::string& directory_path)
{
    std::ifstream file{directory_path + filename};
    if(file.is_open())
    {
        file >> std::noskipws;
        std::string read_data{std::istream_iterator<char>{file}, std::istream_iterator<char>{}};
        file.close();
        return read_data;
    }
    file.close();
    return std::nullopt;
}

void write_file(const std::string& filename, const std::string& directory_path, const std::string& text)
{
    std::ofstream file{directory_path + filename};
    std::copy(text.begin(), text.end(), std::ostream_iterator<char>(file));
}

std::string gzip_compression(const std::string& message_to_compress)
{ 
    char a[50] = "Hello Hello Hello Hello Hello Hello!"; 

    // placeholder for the compressed (deflated) version of "a" 
    char b[50];

    // placeholder for the UNcompressed (inflated) version of "b"
    char c[50];
     

    printf("Uncompressed size is: %lu\n", strlen(a));
    printf("Uncompressed string is: %s\n", a);


    printf("\n----------\n\n");

    // STEP 1.
    // deflate a into b. (that is, compress a into b)
    
    // zlib struct
    z_stream defstream;
    defstream.zalloc = Z_NULL;
    defstream.zfree = Z_NULL;
    defstream.opaque = Z_NULL;
    // setup "a" as the input and "b" as the compressed output
    defstream.avail_in = (uInt)strlen(a)+1; // size of input, string + terminator
    defstream.next_in = (Bytef *)a; // input char array
    defstream.avail_out = (uInt)sizeof(b); // size of output
    defstream.next_out = (Bytef *)b; // output char array
    
    // the actual compression work.
    deflateInit(&defstream, Z_BEST_COMPRESSION);
    deflate(&defstream, Z_FINISH);
    deflateEnd(&defstream);
     
    // This is one way of getting the size of the output
    printf("Compressed size is: %lu\n", strlen(b));
    printf("Compressed string is: %s\n", b);
   return message_to_compress;
}

std::string get_response_message(const std::string& request_message, const std::string& directory_path)
{
    std::string message{"HTTP/1.1 404 Not Found\r\n\r\n"};
    std::optional<std::string> available_encoding{check_for_compression_header(request_message)};
    switch (find_start_sequnce_index(request_message))
    {
    case 0:
        message = "HTTP/1.1 200 OK\r\n\r\n";
        break;
    case 1:
        {
            std::string response{find_string_in_between("echo/", " HTTP", request_message)};
            message = "HTTP/1.1 200 OK\r\n";
            if(available_encoding.has_value())
            {
                message += "Content-Encoding: " + available_encoding.value() + "\r\n";
                std::cout << "Message" << gzip_compression(response) << '\n';
            }
            message += "Content-Type: text/plain\r\nContent-Length: " + std::to_string(response.length()) + "\r\n\r\n" + response;
        }
        break;
    case 2:
        {
            std::string response{find_string_in_between("User-Agent: ", "\r\n", request_message)};
            message = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: " + std::to_string(response.length()) + "\r\n\r\n" + response;
        }
        break;
    case 3:
        {
            auto read_data{read_file(find_string_in_between("files/", " HTTP", request_message), directory_path)};
            if(read_data.has_value())
            {
                message = "HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\nContent-Length: " + std::to_string(read_data.value().length()) + "\r\n\r\n" + read_data.value();
            }
        }
        break;
    case 4:
        {
            std::size_t start_index{request_message.find("\r\n\r\n") + 4};
            std::string text{request_message.substr(start_index, request_message.find('\0') - start_index)};
            write_file(find_string_in_between("files/", " HTTP", request_message), directory_path, text);
            message = "HTTP/1.1 201 Created\r\n\r\n";
        }
        break;
    default:
        break;
    }
    return message;
}

int send_server_response(int client_file_descriptor, int server_file_descriptor, const std::string& directory_path)
{
    std::string request_message_buffer(1024, '\0');
    ssize_t bytes_accepted{recv(client_file_descriptor, static_cast<void*>(&request_message_buffer[0]), request_message_buffer.capacity(), MSG_PEEK)};
    
    if(bytes_accepted < 0)
    {
        std::cerr << "Failed to accept message";
        return 1;
    }

    std::string response_message{get_response_message(request_message_buffer, directory_path)};
 
    ssize_t bytes_send{send(client_file_descriptor, response_message.c_str(), response_message.length(), MSG_EOR)};

    if(bytes_send < 0)
    {
        std::cerr << "Failed to send message\n";
        return 1;
    }

    return 0;
}
