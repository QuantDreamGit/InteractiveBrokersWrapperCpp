//
// Created by Giuseppe Priolo on 15/10/25.
//

#ifndef TEST_H
#define TEST_H

#include <string>

class IBWrapper {
public:
    IBWrapper();
    void connect(const std::string& host, int port, int clientId);
};

#endif //TEST_H
