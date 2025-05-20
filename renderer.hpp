#pragma once

#include <memory>

#include "math.hpp"
#include "window.hpp"

// TODO : Changer le nom de ce namespace une fois que j'aurai une idee de nom
namespace sk
{
    // constructeur : initialise vulkan et renvoie la fenetre
    std::shared_ptr<sk::Window> begin();

    // destructeur : libere la memoire utilisee par le rendu
    void end();

    // Envoie des donnees au Vertex shader
    void setUniforms(float uTime, math::vec3 uClr);

    // Appeler cette fonction 1 fois par frame max
    void draw();
};
