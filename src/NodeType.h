#ifndef NODE_TYPE_H
#define NODE_TYPE_H

#include <string>

class NodeType {
public:
    enum Type {
        GROUND,
        SATELLITE,
        UNKNOWN
    };

    // Convert enum to string for better readability
    static std::string toString(Type type) {
        switch (type) {
            case GROUND: return "Ground";
            case SATELLITE: return "Satellite";
            default: return "Unknown";
        }
    }

    // Convert string to enum for parsing
    static Type fromString(const std::string& typeStr) {
        if (typeStr == "ground" || typeStr == "Ground") return GROUND;
        if (typeStr == "satellite" || typeStr == "Satellite") return SATELLITE;
        return UNKNOWN;
    }
};

#endif // NODE_TYPE_H
