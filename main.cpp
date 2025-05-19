#include <iostream>

#include "renderer.hpp"

int main()
{
    try
    {
        auto window = rd::begin();

        while(window->isAlive())
        {
            rd::draw();
        }

        rd::end();
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
