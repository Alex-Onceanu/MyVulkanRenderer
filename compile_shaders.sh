#!/bin/bash

for file in shaders/*.{vert,frag}; do
  glslc "$file" -o "shaders/out/$(basename "$file").spv"
done