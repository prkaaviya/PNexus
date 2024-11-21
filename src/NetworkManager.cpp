#include <cmath>
#include <memory>
#include <iostream>
#include <curl/curl.h>
#include <json/json.h>

#include "Node.h"
#include "NetworkManager.h"

NetworkManager::NetworkManager(std::string registryAddress)
    : registryAddress(std::move(registryAddress)) {}

// Helper: Check if a node exists in the list
bool NetworkManager::nodeExists(const std::shared_ptr<Node>& node) const {
  return std::any_of(nodes.begin(), nodes.end(), [&](const std::shared_ptr<Node>& existingNode) {
    return existingNode->getName() == node->getName() &&
           existingNode->getIP() == node->getIP() &&
           existingNode->getPort() == node->getPort();
  });
}

// Helper: Perform CURL requests
bool NetworkManager::performCurlRequest(const std::string& url, const std::string& payload) {
  CURL* curl = curl_easy_init();
  if (!curl) {
    std::cerr << "[ERROR] Failed to initialize CURL" << std::endl;
    return false;
  }

  struct curl_slist* headers = curl_slist_append(nullptr, "Content-Type: application/json");
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, payload.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

  CURLcode res = curl_easy_perform(curl);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  if (res != CURLE_OK) {
    std::cerr << "[ERROR] CURL request failed: " << curl_easy_strerror(res) << std::endl;
    return false;
  }
  return true;
}

void NetworkManager::addNode(const std::shared_ptr<Node>& node) {
  if (nodeExists(node)) {
    for (auto& existingNode : nodes) {
      if (existingNode->getName() == node->getName()) {
        existingNode->setCoords(node->getCoords());
        return;
      }
    }
  }

  nodes.push_back(node);
  std::cout << "[INFO] Added node: " << node->getName() << " (" << node->getId() << ") at "
            << node->getIP() << ":" << node->getPort() << " to the network." << std::endl;
}

void NetworkManager::removeNode(const std::string& id) {
  const auto node = std::remove_if(nodes.begin(), nodes.end(), [&id, node](const std::shared_ptr<Node>&) {
    return node->get == id;
  });

  if (node != nodes.end()) {
    nodes.erase(node, nodes.end());
    std::cout << "[INFO] Removed node with ID: " << id << std::endl;
  } else {
    std::cerr << "[WARNING] Node with ID " << id << " not found in the network." << std::endl;
  }
}

void NetworkManager::listNodes() const {
  if (nodes.empty()) {
    std::cout << "[INFO] No nodes are currently registered in the network." << std::endl;
    return;
  }

  std::cout << "[INFO] Current nodes in the network:" << std::endl;
  for (const std::shared_ptr<Node>& node : nodes) {
    auto coords = node->getCoords();
    std::cout << NodeType::toString(node->getType()) << " " << node->getName() << " ("
              << node->getId() << ") at " << node->getIP() << ":" << node->getPort() << " ["
              << coords.first << ", " << coords.second << "]" << std::endl;
  }
}

std::vector<std::shared_ptr<Node>> NetworkManager::getSatelliteNodes() const {
  std::vector<std::shared_ptr<Node>> satellites;
  for (const std::shared_ptr<Node>& node : nodes) {
    if (node->getType() == NodeType::SATELLITE) {
      satellites.push_back(node);
    }
  }
  return satellites;
}

bool NetworkManager::registerNodeWithRegistry(const std::shared_ptr<Node>& node) {
  Json::Value payload = createNodePayload("register", node);
  std::string url = registryAddress + "/register";
  return performCurlRequest(url, payload.toStyledString());
}

void NetworkManager::deregisterNodeWithRegistry(const std::shared_ptr<Node>& node) {
  Json::Value payload = createNodePayload("deregister", node);
  std::string url = registryAddress + "/deregister";
  performCurlRequest(url, payload.toStyledString());
}

Json::Value NetworkManager::createNodePayload(const std::string& action, const std::shared_ptr<Node>& node) {
  Json::Value payload;
  payload["action"] = action;
  payload["type"] = NodeType::toString(node->getType());
  payload["name"] = node->getName();
  payload["ip"] = node->getIP();
  payload["port"] = node->getPort();
  auto coords = node->getCoords();
  payload["x"] = coords.first;
  payload["y"] = coords.second;
  return payload;
}

bool NetworkManager::updateNodeInRegistry(const std::shared_ptr<Node>& node) {
  Json::Value payload = createNodePayload("update", node);
  std::string url = registryAddress + "/update";
  return performCurlRequest(url, payload.toStyledString());
}

void NetworkManager::fetchNodesFromRegistry() {
  Json::Value payload;
  payload["action"] = "list";
  std::string response;
  performCurlRequest(registryAddress, payload.toStyledString());

  Json::Value nodeListJson;
  Json::CharReaderBuilder reader;
  std::istringstream responseStream(response);
  std::string errs;

  if (!Json::parseFromStream(reader, responseStream, &nodeListJson, &errs)) {
    std::cerr << "[ERROR] Failed to parse node list: " << errs << std::endl;
    return;
  }

  if (!nodeListJson.isArray()) {
    std::cerr << "[ERROR] Unexpected response format: Expected an array." << std::endl;
    return;
  }

  for (const auto& nodeJson : nodeListJson) {
    if (!nodeJson.isObject()) {
      std::cerr << "[ERROR] Malformed node entry in response." << std::endl;
      continue;
    }

    auto node = parseNodeFromJson(nodeJson);
    if (node) addNode(node);
  }
}

std::shared_ptr<Node> NetworkManager::parseNodeFromJson(const Json::Value& nodeJson) const {
  if (!nodeJson.isMember("name") || !nodeJson.isMember("ip") || !nodeJson.isMember("port")) {
    std::cerr << "[ERROR] Missing required fields in node JSON." << std::endl;
    return nullptr;
  }

  double x = nodeJson.get("x", 0.0).asDouble();
  double y = nodeJson.get("y", 0.0).asDouble();
  NodeType::Type type = NodeType::fromString(nodeJson["type"].asString());

  return std::make_shared<Node>(type, nodeJson["name"].asString(), nodeJson["ip"].asString(),
                                nodeJson["port"].asInt(), std::make_pair(x, y), *this);
}

void NetworkManager::createRoutingTable() {
  topology.assign(nodes.size(), std::vector<int>(nodes.size(), 0));
}

void NetworkManager::updateRoutingTable(const std::shared_ptr<Node>& src) {
  if (nodes.size() != topology.size()) {
    createRoutingTable();
  }

  for (size_t i = 0; i < nodes.size(); ++i) {
    for (size_t j = 0; j < nodes.size(); ++j) {
      auto i_coords = nodes[i]->getCoords();
      auto j_coords = nodes[j]->getCoords();

      int weight = (nodes[i]->getType() == NodeType::GROUND || nodes[j]->getType() == NodeType::GROUND) ? 50 : 0;
      topology[i][j] = std::sqrt(std::pow(i_coords.first - j_coords.first, 2) +
                                 std::pow(i_coords.second - j_coords.second, 2)) + weight;
    }
  }

  route(findNodeIndex(src));
}

void NetworkManager::route(int src_idx) {
  std::vector<int> minDist(nodes.size(), INT_MAX);
  std::vector<bool> visited(nodes.size(), false);
  minDist[src_idx] = 0;

  for (size_t i = 0; i < nodes.size(); ++i) {
    int minIdx = -1;

    for (size_t j = 0; j < nodes.size(); ++j) {
      if (!visited[j] && (minIdx == -1 || minDist[j] < minDist[minIdx])) {
        minIdx = j;
      }
    }

    visited[minIdx] = true;

    for (size_t j = 0; j < nodes.size(); ++j) {
      if (topology[minIdx][j] && minDist[minIdx] + topology[minIdx][j] < minDist[j]) {
        minDist[j] = minDist[minIdx] + topology[minIdx][j];
        nextHop[j] = (minIdx == src_idx) ? j : minIdx;
      }
    }
  }
}

int NetworkManager::findNodeIndex(const std::shared_ptr<Node>& node) const {
  auto it = std::find_if(nodes.begin(), nodes.end(), [&](const auto& n) {
    return n->getId() == node->getId();
  });
  return (it != nodes.end()) ? std::distance(nodes.begin(), it) : -1;
}

std::shared_ptr<Node> NetworkManager::getNextHop(const std::string& name) {
  auto it = std::find_if(nodes.begin(), nodes.end(), [&](const auto& n) {
    return n->getName() == name;
  });
  return (it != nodes.end()) ? nodes[nextHop[std::distance(nodes.begin(), it)]] : nullptr;
}
