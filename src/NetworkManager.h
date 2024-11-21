#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <string>
#include <vector>

#include <json/json.h>

using matrix = std::vector<std::vector<int>>;

class Node;

class NetworkManager {
public:
    explicit NetworkManager(std::string registryAddress);

    bool nodeExists(const std::shared_ptr<Node>& node) const;
    static bool performCurlRequest(const std::string& url, const std::string& payload);
    static Json::Value createNodePayload(const std::string& action, const std::shared_ptr<Node>& node);
    std::shared_ptr<Node> parseNodeFromJson(const Json::Value& nodeJson) const;

    void addNode(const std::shared_ptr<Node> &node);
    void removeNode(const std::string &id);
    void listNodes() const;

    std::vector<std::shared_ptr<Node>> getSatelliteNodes() const;

    void fetchNodesFromRegistry();
    static std::string serializeNode(const std::shared_ptr<Node>& node);
    bool registerNodeWithRegistry(const std::shared_ptr<Node> &node);
    void deregisterNodeWithRegistry(const std::shared_ptr<Node> &node);
    bool sendToRegistryServer(const std::string& endpoint, const std::string& jsonPayload);
    bool updateNodeInRegistry(const std::shared_ptr<Node>& node);

    void createRoutingTable();
    void updateRoutingTable(const std::shared_ptr<Node>& src);
    void route(int src_idx);
    int findNodeIndex(const std::shared_ptr<Node>& node) const;
    std::shared_ptr<Node> getNextHop(const std::string& name);

    std::vector<int> nextHop;

private:
    matrix topology;

    std::vector<std::shared_ptr<Node>> nodes;
    std::string registryAddress;
};

#endif
