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

class SimpleCharBuffer
{
public:

    SimpleCharBuffer()
        : memory_capacity{64} 
    {
        memory = new char[memory_capacity];
    }

    SimpleCharBuffer(std::size_t memory_capacity)
        : memory_capacity{memory_capacity}
    {
        memory = new char[memory_capacity];
    }

    SimpleCharBuffer(const SimpleCharBuffer& that) = delete;

    SimpleCharBuffer& operator=(const SimpleCharBuffer& that) = delete;

    SimpleCharBuffer(SimpleCharBuffer&& that) = delete;

    SimpleCharBuffer& operator=(SimpleCharBuffer&& that) = delete;
    
    std::size_t capacity() const noexcept
    {
        return memory_capacity;       
    }

    void reallocate(std::size_t memory_capacity)
    {
        if(memory)
        {
            delete[] memory;
        }
        this->memory_capacity = memory_capacity;
        memory = new char[memory_capacity];
    } 

    void clear()
    {
        delete[] memory;
        memory = nullptr;
        memory_capacity = 0;
    }

    char* charPointer()
    {
        return memory;
    }

    void* rawPointer()
    {
        return static_cast<void*>(memory);
    }

    ~SimpleCharBuffer()
    {
        if(memory)
        {
            delete[] memory;
        }
    }

private:
    
    std::size_t memory_capacity;
    char* memory;

};
 

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

    SimpleCharBuffer message_buffer{1024};
    
    ssize_t bytes_accepted{recv(client_fd, message_buffer.rawPointer(), message_buffer.capacity(), MSG_PEEK)};
    
    std::cout << "Message: (" << message_buffer.charPointer() << ")\n";

    if(bytes_accepted < 0)
    {
        std::cerr << "Failed to accept message";
        return 1;
    }
    
    std::string sequence_to_find{"GET / HTTP/1.1\r\n"};
    
    bool found{true};
    std::size_t i{0};
    for(const auto& element : sequence_to_find)
    {
        if(element != message_buffer.charPointer()[i])
        {
            found = false;
            break;
        }
        ++i;
    }

    std::string message{found? "HTTP/1.1 200 OK\r\n\r\n" : "HTTP/1.1 404 Not Found\r\n\r\n"};
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
