#pragma once

#include <memory>

#include "window.hpp"

// TODO : Changer le nom de ce namespace une fois que j'aurai une idee de nom
namespace rd
{
    // constructeur : initialise vulkan et renvoie la fenetre
    std::shared_ptr<rd::Window> begin();

    // destructeur : libere la memoire utilisee par le rendu
    void end();

    // Appeler cette fonction 1 fois par frame max
    void draw();
};
