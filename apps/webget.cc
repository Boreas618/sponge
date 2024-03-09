#include "socket.hh"
#include "util.hh"

#include <cstdlib>
#include <iostream>

void get_URL(const std::string &host, const std::string &path) {
    TCPSocket tcp_socket;
    Address addr(host, "http");
    tcp_socket.connect(addr);
    tcp_socket.write("GET " + path + " HTTP/1.1\r\nHost:" + host + "\r\nConnection:close\r\n\r\n");

    while (!tcp_socket.eof()) {
        std::string bytes = tcp_socket.read();
        std::cout << bytes;
    }
    tcp_socket.close();
}

int main(int argc, char *argv[]) {
    try {
        if (argc <= 0) {
            abort();  // For sticklers: don't try to access argv[0] if argc <= 0.
        }

        // The program takes two command-line arguments: the hostname and "path" part of the URL.
        // Print the usage message unless there are these two arguments (plus the program name
        // itself, so arg count = 3 in total).
        if (argc != 3) {
            std::cerr << "Usage: " << argv[0] << " HOST PATH\n";
            std::cerr << "\tExample: " << argv[0] << " stanford.edu /class/cs144\n";
            return EXIT_FAILURE;
        }

        // Get the command-line arguments.
        const std::string host = argv[1];
        const std::string path = argv[2];

        // Call the student-written function.
        get_URL(host, path);
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}