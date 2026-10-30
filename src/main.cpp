// #define _WIN32
#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>

    using socket_t = SOCKET;
    void CloseSocket(socket_t fd) {
        closesocket(fd);
    }
#elif __linux__
    #include <sys/socket.h>
    #include <netdb.h>
    #include <arpa/inet.h>
    #include <string.h>
    #include <unistd.h>
    #include <ifaddrs.h>
    #include <net/if.h>

    using socket_t = int;
    void CloseSocket(socket_t fd) {
        close(fd);
    }
#endif

#include <iostream>
#include <string>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <array>

using std::cerr;
using std::cout;

constexpr const char* PORT  = "29898";
constexpr const char MAGIC[] = {'L', 'O', 'H'};

#pragma pack(push, 1)
struct FileHeader{
    char magic[3];
    uint64_t size;
    uint8_t name_size;
};
#pragma pack(pop)

enum ApplicationTypeT {
    Reciever, Sender
};

class FileSender {
private:
    bool PrintIPAddress() {
        addrinfo* address{};

        #ifdef _WIN32
        char host_name[256]{};
        if (gethostname(host_name, sizeof(host_name)) != 0) {
            std::cerr << "Error: can't get hostname\n";
            return 1;
        }

        addrinfo hints{};
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        int code = getaddrinfo(host_name, PORT, &hints, &address);
        if (code != 0) {
            cerr << "Error: can't get address info\n";
            return false;
        }

        char ipString[INET_ADDRSTRLEN];
        for (auto* ptr = address; ptr != nullptr; ptr = ptr->ai_next) {
            struct sockaddr_in* sockaddr_ipv4 = (struct sockaddr_in*)ptr->ai_addr;
            
            inet_ntop(AF_INET, &(sockaddr_ipv4->sin_addr), ipString, INET_ADDRSTRLEN);    
            break;
        }
        std::cout << "Sender IP: " << ipString << std::endl;

        #elif __linux__
        ifaddrs* sender_address{};
        if (getifaddrs(&sender_address) == -1) {
            cerr << "Error: Can't get ip address\n"
                 << "errno: " << strerror(errno) << '\n';
            return false;
        }

        bool found{};
        for (auto* p = sender_address; p != nullptr; p = p->ifa_next) {
            int flags = p->ifa_flags;

            if (flags & IFF_LOOPBACK) {
                continue;
            }
            if (!(flags & IFF_UP) || !(flags & IFF_RUNNING)) {
                continue;
            }
            if (p->ifa_addr == nullptr) {
                continue;
            }
            if (p->ifa_addr->sa_family != AF_INET) {
                continue;
            }

            char ip_str[INET_ADDRSTRLEN]{};
            void* sockaddr = &reinterpret_cast<sockaddr_in*>(p->ifa_addr)->sin_addr;
            
            if (inet_ntop(AF_INET, sockaddr, ip_str, INET_ADDRSTRLEN)) {
                cout << "Sender IP: " << ip_str << '\n';
            }
            else {
                cerr << "Error: can't print ip address\n";
                continue;
            }

            found = true;
            break;
        }

        if (!found) {
            cerr << "Error: check if you are online\n";
            return false;
        }

        freeifaddrs(sender_address);
        #endif

        return true;
    }
public:
    socket_t reciever_fd{};
    socket_t sender_fd{};

    bool Init() {
        addrinfo* address{};

        // Getting addrinfo
        #pragma region
        
        addrinfo hints{};
        hints.ai_family   = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        hints.ai_flags    = AI_PASSIVE;

        int gai_code = getaddrinfo(nullptr, PORT, &hints, &address);
        if (gai_code != 0) {
            cerr << "Error: can't get address info\n"
                 << "gai_error: " << gai_strerror(gai_code) << '\n';
            return false;
        }
        
        #pragma endregion

        // Getting socket
        #pragma region

        addrinfo* address_working{};
        for (auto* p = address; p != nullptr; p = p->ai_next) {
            reciever_fd = socket(
                p->ai_family,
                p->ai_socktype,
                p->ai_protocol
            );
            
            if (reciever_fd >= 0) {
                address_working = p;
                break;
            }
        }
        if (reciever_fd < 0) {
            cerr << "Error: can't get socket descriptor\n"
                 << "errno: " << strerror(errno) << '\n';
            return false;
        }

        #pragma endregion

        // Setting reusable socket
        #pragma region
        
        int yes = 1;
        #ifdef _WIN32
        if (setsockopt(reciever_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes)) == SOCKET_ERROR) {
            std::cerr << "Error: can't set setsockopt\n";
            return false;
        }
        #elif __linux__
        if (setsockopt(reciever_fd, SOL_SOCKET, SO_REUSEADDR, (const void*)&yes, sizeof(yes)) == -1) {
            std::cerr << "Error: can't set setsockopt\n"
                      << "errno: " << strerror(errno) << '\n';
            return false;
        }
        #endif

        #pragma endregion

        // Binding socket
        #pragma region
        
        if (bind(
                reciever_fd, 
                address_working->ai_addr, 
                address_working->ai_addrlen
            ) == -1) {
            cerr << "Error: can't bind socket\n"
                 << "errno: " << strerror(errno) << '\n';
            return false;
        }

        #pragma endregion
        
        freeaddrinfo(address);

        if (!PrintIPAddress()) {
            return false;
        }

        // Listening for client
        #pragma region
        
        if (listen(reciever_fd, 1) == -1) {
            cerr << "Error: can't listen for client\n"
                 << "errno: " << strerror(errno) << '\n';
            return false;
        }
        
        #pragma endregion

        // Accepting client
        #pragma region

        sockaddr_storage client_sockaddr{};
        socklen_t client_sockaddr_len = sizeof(client_sockaddr);

        cout << "Waiting for client...\n";
        sender_fd = accept(reciever_fd, reinterpret_cast<sockaddr*>(&client_sockaddr), &client_sockaddr_len);
        if (sender_fd == -1) {
            cerr << "Error: can't accept client\n"
                 << "errno: " << strerror(errno) << '\n';
            return false;
        }
        cout << "Client connected!\n";
        
        #pragma endregion

        return true;
    }
    
    ~FileSender() {
        CloseSocket(reciever_fd);
        CloseSocket(sender_fd);
    }
};

class FileReciever {
public:
    socket_t fd{};

    bool Init(const char* address_str) {
        addrinfo* address{};
        
        // Getting addrinfo
        #pragma region 

        addrinfo hints{};
        hints.ai_family   = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        int gai_code = getaddrinfo(address_str, PORT, &hints, &address);
        if (gai_code != 0) {
            cerr << "Error: can't get address info\n"
                 << "gai_error: " << gai_strerror(gai_code) << '\n';
            return false;
        }
        
        #pragma endregion

        // Getting socket
        #pragma region

        for (auto* p = address; p != nullptr; p = p->ai_next) {
            fd = socket(
                p->ai_family,
                p->ai_socktype,
                p->ai_protocol
            );
            
            if (fd >= 0) {
                break;
            }
        }
        if (fd < 0) {
            cerr << "Error: can't get socket descriptor\n"
                 << "errno: " << strerror(errno) << '\n';
            return false;
        }

        #pragma endregion
        
        // Connecting to sender
        #pragma region

        cout << "Connecting to sender...\n";
        if (connect(fd, address->ai_addr, address->ai_addrlen) == -1) {
            cerr << "Error: can't connect to sender\n"
                 << "errno: " << strerror(errno) << '\n';
            return false;
        }
        cout << "Connected to sender!\n";

        #pragma endregion

        freeaddrinfo(address);

        return true;
    }

    ~FileReciever() {
        CloseSocket(fd);
    }
};


ApplicationTypeT GetApplicationTypeT(const char* argument) {
    sockaddr_in client_sockaddr{};
    int code = inet_pton(AF_INET, argument, &client_sockaddr.sin_addr);

    return code == 1 ? Reciever : Sender;
}

bool GetFileStream(std::fstream& file_stream, const ApplicationTypeT application_type, const char** argv, int argc) {
    if (application_type == Sender) {
        if (!std::filesystem::exists(argv[1])) {
            cerr << "Error: file \"" << argv[1] << "\" doesn't exist\n";
            return false;
        }
    
        if (!std::filesystem::is_regular_file(argv[1])) {
            cerr << "Error: \"" << argv[1] << "\" is not a regular file\n";
            return false;
        }
    
        file_stream.open(argv[1], std::ios::in | std::ios::binary);

        if (!file_stream.is_open()) {
            cerr << "Error: can't open a file \"" << argv[1] << "\"\n";
        }
    }
    else {
        if (argc > 3) {
            cerr << "Error: too many argv\n";
            return false;
        }

        bool is_output_file_specified = (argc == 3);
        if (is_output_file_specified) {
            file_stream.open(argv[2], std::ios::out | std::ios::binary);
            
            if (!file_stream.is_open()) {
                cerr << "Error: can't create a file \"" << argv[2] << "\"\n";
                return false;
            }
        }
    }

    return true;
}

bool SendBuffer(char* buffer, int bytes_count, int fd) {
    int bytes_send_total{};

    while (bytes_send_total < bytes_count) {
        int bytes_send = send(fd, buffer + bytes_send_total, bytes_count - bytes_send_total, 0);
        bytes_send_total += bytes_send;

        if (bytes_send == -1) {
            cerr << "Error: can't send to client\n"
                 << "errno: " << strerror(errno) << '\n';
            return false;
        }
    }

    return true;
}

bool RecieveBuffer(char* buffer, int bytes_count, int fd, bool forceFull = true) {
    int bytes_recieved_total{};

    while (bytes_recieved_total < bytes_count) {
        int bytes_recieved = recv(fd, buffer + bytes_recieved_total, bytes_count - bytes_recieved_total, 0);
        bytes_recieved_total += bytes_recieved;

        if (bytes_recieved == -1) {
            cerr << "Error: can't recieve file\n"
                 << "errno: " << strerror(errno) << '\n';
            return false;
        }
        if (bytes_recieved == 0) {
            if (bytes_recieved_total < bytes_count) {
                cerr << "Error: file wasn't fully transfered because sender closed connection\n";
                return false;
            }
            break;
        }
    }

    return true;
}


int main(int argc, char const *argv[]) {
    #ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed!\n";
            return false;
        }
    #endif

    if (argc == 1) {
        cerr<<"Error: not enough arguments\n";
        return 1;
    }

    const ApplicationTypeT application_type = GetApplicationTypeT(argv[1]);
    bool is_output_file_specified = (argc == 3);

    std::fstream file_stream;
    if (!GetFileStream(file_stream, application_type, argv, argc)) {
        return 1;
    }

    std::array<char, 64*1024> buffer;
    FileHeader file_header{};

    if (application_type == Sender) {
        FileSender sender;
        if (!sender.Init()) {
            return 1;
        }

        std::string_view file_name(argv[1]);

        uint32_t slash_char_pos = file_name.find_first_of("/\\");
        if (slash_char_pos != file_name.npos) {
            file_name.remove_prefix(slash_char_pos+1);
        }

        std::memcpy(file_header.magic, MAGIC, sizeof(MAGIC));
        file_header.size = std::filesystem::file_size(argv[1]);
        file_header.name_size = file_name.size();
        
        file_header.size = htonl(file_header.size);

        std::memcpy(buffer.data(), &file_header, sizeof(file_header));
        std::memcpy(buffer.data()+sizeof(file_header), file_name.data(), file_name.size());

        cout << "Sending file...\n";

        if (!SendBuffer(buffer.data(), sizeof(file_header)+file_name.size(), sender.sender_fd)) {
            return 1;
        }

        while (true) {
            file_stream.read(buffer.data(), buffer.size());
            int bytes_read = file_stream.gcount();

            if (bytes_read == 0) {
                break;
            }

            if (!SendBuffer(buffer.data(), bytes_read, sender.sender_fd)) {
                return 1;
            }
        }

        cout << "File sent!\n";
    }
    
    else if (application_type == Reciever) {
        FileReciever reciever;
        if (!reciever.Init(argv[1])) {
            return 1;
        }
        
        cout << "Recieving file...\n";

        if (!RecieveBuffer(reinterpret_cast<char*>(&file_header), sizeof(file_header), reciever.fd)) {
            return 1;
        }

        if (std::strncmp(MAGIC, file_header.magic, sizeof(MAGIC)) != 0) {
            cerr << "Error: File came with error\n";
            return 1;
        }

        file_header.size = ntohl(file_header.size);
        // file_header.name_size = ntohs(file_header.name_size);

        char file_name[file_header.name_size];
        if (!RecieveBuffer(file_name, file_header.name_size, reciever.fd)) {
            return 1;
        }

        if (!is_output_file_specified) {
            file_stream.open(file_name, std::ios::out | std::ios::binary);
        }
        
        int bytes_left = file_header.size;

        while (true) {
            int target_recieve = std::min<int>(buffer.size(), bytes_left);

            if (!RecieveBuffer(buffer.data(), target_recieve, reciever.fd)) {
                return 1;
            }

            bytes_left -= target_recieve;
            file_stream.write(buffer.data(), target_recieve);

            if (bytes_left <= 0) {
                break;
            }
        }

        cout << "File recievied!\n";
    }

    #ifdef _WIN32
    WSACleanup();
    #endif

    file_stream.close();
    return 0;
}
