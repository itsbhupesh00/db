#ifndef CLI_HPP
#define CLI_HPP

#include "../database/database.hpp"

#include <string>

class CLI
{
public:
    CLI();

    void run();

private:
    Database database;

    void printBanner();
    void printPrompt();
    void clearScreen();

    std::string readInput();
};

#endif
