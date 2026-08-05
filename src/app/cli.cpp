#include "cli.hpp"

#include <iostream>

CLI::CLI()
    : database("data/database.bdb")
{
    database.initialize();
}

void CLI::run()
{
    printBanner();

    while (true)
    {
        printPrompt();

        std::string input = readInput();

        if (input.empty())
            continue;

        if (input == "exit")
            break;

        if (input == "clear" || input == "cls" || input == "anil")
        {
            clearScreen();
            continue;
        }

        std::string result = database.execute(input);

        if (!result.empty())
            std::cout << result << '\n';
    }

    std::cout << "Goodbye!\n";
}

void CLI::printBanner()
{
    std::cout
        << "=========================================\n"
        << "            BinaryDB v0.1\n"
        << "=========================================\n"
        << "Type 'help' for available commands.\n"
        << "Type 'exit' to quit.\n\n";
}

void CLI::printPrompt()
{
    std::cout << "db> ";
}

void CLI::clearScreen()
{
    std::cout << "\033[2J\033[H" << std::flush;
}

std::string CLI::readInput()
{
    std::string input;

    std::getline(std::cin, input);

    return input;
}
