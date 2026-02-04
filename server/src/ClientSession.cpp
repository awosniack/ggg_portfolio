#include "ClientSession.hpp"

namespace inventory {

ClientSession::ClientSession(int socket) 
    : socket_(socket), 
      lastActivity_(std::chrono::steady_clock::now()) {
}

ClientSession::~ClientSession() {
}

} // namespace inventory
