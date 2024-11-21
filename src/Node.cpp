#include <iomanip>
#include <iostream>
#include <chrono>
#include <cstring>
#include <random>
#include <string>
#include <sstream>
#include <thread>
#include <utility>
#include <unistd.h>
#include <dirent.h>
#include <arpa/inet.h>

#include "Node.h"
#include "Utility.h"

Node::Node(NodeType::Type nodeType, std::string name, const std::string &ip, int port,
           std::pair<double, double> coords, NetworkManager networkManager)
    : type(nodeType), id(generateUUID()), name(std::move(name)), ip(ip), port(port),
      coords(std::move(coords)), networkManager(std::move(networkManager)) {
  socket_fd = -1;
  std::memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);
  delay = 0;
}

std::string Node::getId() const { return id; }
std::string Node::getName() const { return name; }
std::string Node::getIP() const { return ip; }
int Node::getPort() const { return port; }

std::pair<double, double> Node::getCoords() const { return coords; }
void Node::setCoords(const std::pair<double, double> &newCoords) { coords = newCoords; }

NodeType::Type Node::getType() const { return type; }

bool Node::bind() {
  // Create a UDP socket
  socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (socket_fd < 0) {
    std::cerr << "Failed to create socket for Node " << name << ": " << strerror(errno)
              << std::endl;
    return false;
  }

  // Bind the socket to the IP and Port
  if (::bind(socket_fd, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr)) < 0) {
    std::cerr << "Failed to bind socket for Node " << name << ": " << strerror(errno) << std::endl;
    close(socket_fd);
    socket_fd = -1;
    return false;
  }

  std::cout << "[NEXUS] Node " << name << " successfully bound to " << ip << ":" << port
            << std::endl;
  return true;
}

void Node::updatePosition() {
  if (type == NodeType::Type::SATELLITE) {
    // Satellite nodes move based on below logic
    coords.first = roundToTwoDecimalPlaces(coords.first + 0.05);
    coords.second = roundToTwoDecimalPlaces(coords.second + 0.075);
    std::cout << "[UPDATE] Satellite " << name << " new position: (" << coords.first << ", "
              << coords.second << ")" << std::endl;

    // Update the registry with new coordinates
    networkManager.updateNodeInRegistry(shared_from_this());
  } else if (type == NodeType::Type::GROUND) {
    // Ground nodes are stationary
    std::cout << "[INFO] Ground node " << name << " remains stationary at (" << coords.first
              << ", " << coords.second << ")" << std::endl;
  }
}


void Node::receiveMessage(std::string &message) {
  if (socket_fd < 0) {
    std::cerr << "[ERROR] Socket is not initialized for receiving messages." << std::endl;
    return;
  }

  // Added extra 32 for non payload data
  constexpr size_t bufferSize = MAX_BUFFER_SIZE + 32;
  char buffer[bufferSize];

  struct sockaddr_in senderAddr {};
  socklen_t senderAddrLen = sizeof(senderAddr);

  ssize_t bytesReceived =
      recvfrom(socket_fd, buffer, bufferSize - 1, 0,
               reinterpret_cast<struct sockaddr *>(&senderAddr), &senderAddrLen);

  std::cout << "[NEXUS] Received " << bytesReceived << "bytes\n";

  if (bytesReceived < 0) {
    std::cerr << "[ERROR] Failed to receive message: " << strerror(errno) << "" << std::endl;
    return;
  }

  // Null-terminate the buffer to treat it as a string
  buffer[bytesReceived] = '\0';
  std::vector<uint8_t> receivedMessage(buffer, buffer + bufferSize);
  std::cout << "[NEXUS] Buffer sz " << receivedMessage.size() << "bytes\n";

  char senderIP[INET_ADDRSTRLEN];
  inet_ntop(AF_INET, &senderAddr.sin_addr, senderIP, sizeof(senderIP));
  int senderPort = ntohs(senderAddr.sin_port);

  std::string senderName, targetIP;
  int targetPort = 0;
  std::cout << "[NEXUS] Received message from " << senderIP << ":" << senderPort << "\n";

  Packet pkt = pkt.deserialize(receivedMessage);

  // std::ofstream debugFile("img3.webp", std::ios::binary | std::ios::app);
  // debugFile.write(reinterpret_cast<char *>(pkt.data.data()), pkt.data.size());
  // debugFile.close();

  if ((pkt.tAddress == addr.sin_addr.s_addr) && (pkt.tPort == htons(port))) {
    processMessage(pkt);
  } else {
    char nextIP[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(pkt.tAddress), nextIP, sizeof(nextIP));
    int nextPort = ntohs(pkt.tPort);

    std::cout << "[NEXUS] Forwarding to " << nextIP << ":" << nextPort << "\n";
    sendTo(nextIP, nextPort, pkt);

    // Send to specified node
    // Extract specified final node
    // sendMessage(sockaddr_in, pkt);
  }
}

void Node::sendMessage(const std::string &targetName, const std::string &targetIP, int targetPort,
                       const std::string &message) {
  std::cout << "[NEXUS] Node " << name << " sending from " << ip << ":" << port << std::endl;
  std::cout << "Sending to " << targetName << ":" << targetPort << std::endl;

  if (socket_fd < 0) {
    std::cerr << "[ERROR] Invalid socket descriptor. Did you forget to bind the socket?" << std::endl;
    return;
  }

  struct sockaddr_in targetAddr = {};
  targetAddr.sin_family = AF_INET;
  targetAddr.sin_port = htons(targetPort);
  if (inet_pton(AF_INET, targetIP.c_str(), &targetAddr.sin_addr) <= 0) {
    std::cerr << "[ERROR] Invalid target IP address: " << targetIP << std::endl;
    return;
  }

  Packet pkt(addr.sin_addr.s_addr, addr.sin_port, targetAddr.sin_addr.s_addr, targetAddr.sin_port,
             packetType::TEXT);

  auto packet_data = pkt.serialize();
  std::cout << "[DEBUG1] Packet size: " << packet_data.size() << " bytes" << std::endl;
  if (packet_data.empty()) {
    std::cerr << "[ERROR] Failed to serialize packet. No data to send." << std::endl;
    return;
  }

  auto nextHop = networkManager.getNextHop(targetName);
  if (!nextHop) {
    std::cerr << "[ERROR] No next hop found for target: " << targetName << std::endl;
  }
  // for (auto n : networkManager.nextHop) {
  //   std::cout << n << " ";
  // }
  // std::cout << "Sending (intermediate) to " << nextHop->name << " " << nextHop->ip << ":"
  //           << nextHop->port << std::endl;

  struct sockaddr_in nextAddr = {};
  nextAddr.sin_family = AF_INET;
  nextAddr.sin_port = htons(nextHop->getPort());
  if (inet_pton(AF_INET, nextHop->getIP().c_str(), &nextAddr.sin_addr) <= 0) {
    std::cerr << "[ERROR] Invalid next hop IP address: " << nextHop->getIP() << std::endl;
    return;
  }

  const int bytesSent = sendto(socket_fd, packet_data.data(), packet_data.size(), 0,
                               reinterpret_cast<struct sockaddr *>(&nextAddr), sizeof(nextAddr));

  if (bytesSent < 0) {
    std::cerr << "Failed to send message: " << strerror(errno) << std::endl;
  } else {
    std::cout << "[NEXUS] Sent message to " << targetName << " at " << targetIP << ":" << targetPort
              << std::endl;
  }
}

void Node::sendTo(const std::string &targetIP, int targetPort, Packet &pkt) {
  struct sockaddr_in targetAddr = {};
  targetAddr.sin_family = AF_INET;
  targetAddr.sin_port = htons(targetPort);
  inet_pton(AF_INET, targetIP.c_str(), &targetAddr.sin_addr);

  auto pkt_data = pkt.serialize();

  const int bytesSent =
      sendto(socket_fd, pkt_data.data(), pkt_data.size(), 0,
             reinterpret_cast<struct sockaddr *>(&targetAddr), sizeof(targetAddr));

  if (bytesSent < 0) {
    std::cerr << "Failed to send data: " << strerror(errno) << std::endl;
  } else {
    std::cout << "[NEXUS] Sent " << bytesSent << "to (sendTo)" << targetIP << ":" << targetPort
              << std::endl;
  }
}


void Node::sendFile(const std::string &targetName, const std::string &targetIP, int targetPort,
                    const std::string &fileName) {
  struct sockaddr_in targetAddr = {};
  targetAddr.sin_family = AF_INET;
  targetAddr.sin_port = htons(targetPort);
  inet_pton(AF_INET, targetIP.c_str(), &targetAddr.sin_addr);

  std::vector<Packet> fragments;

  std::ifstream fileHandle(fileName, std::ios::binary | std::ios::ate);
  size_t file_size = fileHandle.tellg();
  int fragCount = std::ceil(static_cast<double>(file_size) / MAX_BUFFER_SIZE);
  Packet pkt(addr.sin_addr.s_addr, addr.sin_port, targetAddr.sin_addr.s_addr, targetAddr.sin_port,
             packetType::FILE);
  pkt.fragmentCount = fragCount;
  std::cout << "[NEXUS]: Fragment Count: " << pkt.fragmentCount << std::endl;

  fileHandle.seekg(0, std::ios::beg);
  int fragNumber = 1;
  std::array<uint8_t, MAX_BUFFER_SIZE> buffer;

  for (int i = 0; i < fragCount; i++) {
    fileHandle.read(reinterpret_cast<char *>(buffer.data()), MAX_BUFFER_SIZE);
    auto byteCount = fileHandle.gcount();
    std::cout << "[NEXUS]: Read : " << byteCount << " bytes" << std::endl;
    std::cout << "[NEXUS]: Sending fragment: " << fragNumber << std::endl;

    pkt.fragmentNumber = fragNumber++;
    pkt.data = buffer;

    auto nextHop = networkManager.getNextHop(targetName);

    sendTo(nextHop->getIP(), nextHop->getPort(), pkt);

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  fileHandle.close();
  std::cout << "File sent." << std::endl;
}

std::string Node::extractMessage(const std::string &payload, std::string &senderName,
                                 std::string &targetIP, int &targetPort) {
  std::istringstream iss(payload);
  std::string portStr;
  std::string actualMessage;

  // Parse the payload (format: senderName targetIP targetPort message)
  if (iss >> senderName >> targetIP >> portStr) {
    try {
      targetPort = std::stoi(portStr); // Convert port to integer
    } catch (const std::exception &e) {
      std::cerr << "[ERROR] Invalid port in message payload: " << portStr << "\n";
      return "";
    }

    // Extract the remaining part of the payload as the actual message
    std::getline(iss, actualMessage);
    actualMessage.erase(0, actualMessage.find_first_not_of(" ")); // Trim leading spaces
  } else {
    std::cerr << "[ERROR] Malformed message payload: " << payload << "\n";
  }

  return actualMessage;
}

void Node::simulateSignalDelay() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_real_distribution<> dis(0.1, 2.0); // Random delay between 0.1s and 2.0s
  delay = dis(gen);
  std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(delay * 1000)));
}

std::string Node::generateUUID() {
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 15);
  std::uniform_int_distribution<> dis2(8, 11);

  std::stringstream ss;
  ss << std::hex << std::setfill('0');
  for (int i = 0; i < 8; i++)
    ss << dis(gen);
  ss << "-";
  for (int i = 0; i < 4; i++)
    ss << dis(gen);
  ss << "-4"; // UUID version 4
  for (int i = 0; i < 3; i++)
    ss << dis(gen);
  ss << "-";
  ss << dis2(gen);
  for (int i = 0; i < 3; i++)
    ss << dis(gen);
  ss << "-";
  for (int i = 0; i < 12; i++)
    ss << dis(gen);
  return ss.str();
}

void Node::processMessage(Packet &pkt) {
  std::cout << "[NEXUS] Processing message from " << pkt.sAddress << ":" << pkt.sPort << "\n";

  if (pkt.type == packetType::FILE) {
    std::cout << "[NEXUS] Writing to file " << pkt.sAddress << ":" << pkt.sPort << "\n";
    std::cout << pkt.fragmentNumber << "/" << pkt.fragmentCount << std::endl;

    writeToFile(pkt);
    reassembleFile(pkt);
  } else {
    // TODO
  }
}

void Node::writeToFile(Packet &pkt) {
  std::ostringstream oss;
  oss << "/tmp/" << pkt.sAddress << "_" << pkt.sPort << "_" << pkt.fragmentNumber;
  std::string filename = oss.str();

  std::ofstream ofs(filename, std::ios::binary);
  ofs.write(reinterpret_cast<char *>(pkt.data.data()), pkt.data.size());
  ofs.close();
}

void Node::reassembleFile(Packet &pkt) {
  std::ostringstream oss;
  oss << pkt.sAddress << "_" << pkt.sPort;
  std::string prefix = oss.str();

  std::string path = "/tmp/";
  DIR *dir = opendir(path.c_str());
  if (dir == nullptr) {
    std::cerr << "[REASSEMBLY]: Failed to open directory" << std::endl;
    return;
  }

  int count = 0;
  struct dirent *entry;
  while ((entry = readdir(dir)) != nullptr) {
    std::string filename(entry->d_name);
    if (filename.rfind(prefix, 0) == 0) {
      count++;
    }
  }
  closedir(dir);

  if (count != pkt.fragmentCount) {
    std::cout << "[REASSEMBLY]: Missing fragments: " << pkt.fragmentCount - count << std::endl;
    return;
  }

  std::ofstream output(path + "final_" + prefix, std::ios::binary);
  for (int i = 1; i <= count; i++) {
    std::string filename = path + prefix + "_" + std::to_string(i);
    std::cout << "[REASSEMBLY]: Reading fragment " << filename << std::endl;
    std::ifstream fragment(filename, std::ios::binary);
    output << fragment.rdbuf();
    fragment.close();
    std::cout << "[REASSEMBLY]: Deleting fragment " << filename << std::endl;
    remove(filename.c_str());
  }
  output.close();
}
