# Compiler and standard
CXX = g++
CXXFLAGS = -std=c++11 -pthread -g

# Paths to include and library directories
JSONCPP_INC = /opt/homebrew/Cellar/jsoncpp/1.9.6/include
JSONCPP_LIB = /opt/homebrew/Cellar/jsoncpp/1.9.6/lib
CURL_INC = /opt/homebrew/opt/curl/include
CURL_LIB = /opt/homebrew/opt/curl/lib

# Libraries to link
LIBS = -ljsoncpp -lcurl

# Source files
NEXUS_SOURCES = nexus_main/main.cpp src/Node.cpp src/NetworkManager.cpp
REGISTRY_SOURCES = registry_main/main.cpp src/NexusRegistryServer.cpp

# Targets
all: nexus registry_server

nexus: $(NEXUS_SOURCES)
	$(CXX) $(CXXFLAGS) -I$(JSONCPP_INC) -I$(CURL_INC) \
	-L$(JSONCPP_LIB) -L$(CURL_LIB) \
	$(NEXUS_SOURCES) $(LIBS) -o nexus

registry_server: $(REGISTRY_SOURCES)
	$(CXX) $(CXXFLAGS) -I$(JSONCPP_INC) -I$(CURL_INC) \
	-L$(JSONCPP_LIB) -L$(CURL_LIB) \
	$(REGISTRY_SOURCES) $(LIBS) -o registry_server

clean:
	rm -f nexus registry_server
