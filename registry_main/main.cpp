#include "../src/NexusRegistryServer.h"

int main(int argc, char **argv) {
    if (argc != 2) {
        std::cout << "Usage: registry_service PORT" << std::endl;
        return 1;
    }

    NexusRegistryServer server(std::stoi(argv[1])); // Port for the registry server
    server.start();
    return 0;
}