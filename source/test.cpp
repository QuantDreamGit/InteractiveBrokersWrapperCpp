//
// Created by Giuseppe Priolo on 15/10/25.
//

#include "../include/ibwrapper_test/test.h"

#include <iostream>

IBWrapper::IBWrapper() {
  std::cout << "IBWrapper initialized." << std::endl;
}

void IBWrapper::connect(const std::string& host, int port, int clientId) {
  std::cout << "Connecting to " << host << ":" << port
            << " with clientId " << clientId << std::endl;
}
