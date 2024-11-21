#include <json/json.h>
#include <mutex>
#include <netinet/in.h>
#include <unistd.h>
#include <unordered_map>

#include "NexusRegistryServer.h"
#include "Utility.h"

NexusRegistryServer::NexusRegistryServer(int port) : port(port), isRunning(false) {}

NexusRegistryServer::~NexusRegistryServer() { stop(); }

void NexusRegistryServer::start() {
  serverSocket = socket(AF_INET, SOCK_STREAM, 0);
  if (serverSocket < 0) {
    std::cerr << "[ERROR] Failed to create socket." << std::endl;
    return;
  }

  sockaddr_in serverAddr{};
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  serverAddr.sin_port = htons(port);

  if (bind(serverSocket, reinterpret_cast<struct sockaddr *>(&serverAddr), sizeof(serverAddr)) <
      0) {
    std::cerr << "[ERROR] Failed to bind socket." << std::endl;
    close(serverSocket);
    return;
  }

  if (listen(serverSocket, 10) < 0) {
    std::cerr << "[ERROR] Failed to listen on socket." << std::endl;
    close(serverSocket);
    return;
  }

  isRunning = true;
  std::cout << "[INFO] NexusRegistryServer is running on port " << port << std::endl;

  while (isRunning) {
    sockaddr_in clientAddr{};
    socklen_t clientLen = sizeof(clientAddr);
    int clientSocket =
        accept(serverSocket, reinterpret_cast<struct sockaddr *>(&clientAddr), &clientLen);

    if (clientSocket < 0) {
      std::cerr << "[ERROR] Failed to accept connection." << std::endl;
      continue;
    }

    std::thread(&NexusRegistryServer::handleClient, this, clientSocket).detach();
  }
}

void NexusRegistryServer::stop() {
  isRunning = false;
  close(serverSocket);
  std::cout << "[INFO] NexusRegistryServer stopped." << std::endl;
}

void NexusRegistryServer::processRequest(const std::string &request, std::string &response) {
  std::cout << "[DEBUG] Received request: " << request << std::endl;

  Json::Reader reader;
  Json::Value root;
  if (!reader.parse(request, root)) {
    response = R"({"error": "Invalid JSON format"})";
    return;
  }

  std::string action = root["action"].asString();

  if (action == "register") {
    std::pair<double, double> coords = {roundToTwoDecimalPlaces(std::stod(root["x"].asString())),
                                        roundToTwoDecimalPlaces(std::stod(root["y"].asString()))};

    NodeInfo node = {root["type"].asString(), root["name"].asString(), root["ip"].asString(),
                     coords, root["port"].asInt()};
    registerNode(node);
    response = R"({"message": "Node registered successfully"})";
  } else if (action == "deregister") {
    deregisterNode(root["name"].asString());
    response = R"({"message": "Node deregistered successfully"})";
  } else if (action == "list") {
    response = getNodeList();
  } else {
    response = R"({"error": "Unknown action"})";
  }

  std::cout << std::endl;
}

void NexusRegistryServer::registerNode(const NodeInfo &node) {
  std::lock_guard<std::mutex> lock(nodesMutex);
  nodes.push_back(node);
  std::cout << "[INFO] Registered node: " << node.type << " " << node.name << " (" << node.ip << ":"
            << node.port << ")"
            << " at [" << node.coords.first << ", " << node.coords.second << "]." << std::endl;
}

void NexusRegistryServer::deregisterNode(const std::string &name) {
  std::lock_guard<std::mutex> lock(nodesMutex);
  nodes.erase(std::remove_if(nodes.begin(), nodes.end(),
                             [&name](const NodeInfo &node) { return node.name == name; }),
              nodes.end());
  std::cout << "[INFO] Deregistered node: " << name << std::endl;
}

std::string NexusRegistryServer::getNodeList() {
  std::lock_guard<std::mutex> lock(nodesMutex);
  Json::Value root;
  for (const auto &node : nodes) {
    Json::Value n;
    n["type"] = node.type;
    n["name"] = node.name;
    n["ip"] = node.ip;
    n["port"] = node.port;
    n["x"] = node.coords.first;
    n["y"] = node.coords.second;
    root.append(n);
  }
  Json::StreamWriterBuilder writer;
  return Json::writeString(writer, root);
}

std::string NexusRegistryServer::readFromSocket(int socket) {
  char buffer[4096]; // Increased buffer size for larger requests
  int bytesRead = recv(socket, buffer, sizeof(buffer) - 1, 0);
  if (bytesRead <= 0)
    return "";

  buffer[bytesRead] = '\0'; // Null-terminate the string
  std::string request(buffer);

  // Find the double CRLF that separates headers from the body
  size_t bodyPos = request.find("\r\n\r\n");
  if (bodyPos != std::string::npos) {
    return request.substr(bodyPos + 4); // Extract the body
  }

  return ""; // No body found
}

void NexusRegistryServer::writeToSocket(int socket, const std::string &response) {
  std::string httpResponse = "HTTP/1.1 200 OK\r\n"
                             "Content-Type: application/json\r\n"
                             "Content-Length: " +
                             std::to_string(response.size()) +
                             "\r\n"
                             "\r\n" +
                             response;

  send(socket, httpResponse.c_str(), httpResponse.size(), 0);
}

void NexusRegistryServer::handleClient(int clientSocket) {
  std::string request = readFromSocket(clientSocket);
  std::string response;
  processRequest(request, response);
  writeToSocket(clientSocket, response);
  close(clientSocket);
}