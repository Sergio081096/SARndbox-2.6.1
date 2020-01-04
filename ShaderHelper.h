/***********************************************************************
ShaderHelper: Funciones de ayuda para crear sombreadores GLSL a partir 
de archivos de texto.
Copyright (c) 2014 Oliver Kreylos

This file is part of the Augmented Reality Sandbox (SARndbox).

The Augmented Reality Sandbox is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 2 of the
License, or (at your option) any later version.

The Augmented Reality Sandbox is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
General Public License for more details.

You should have received a copy of the GNU General Public License along
with the Augmented Reality Sandbox; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
***********************************************************************/

#ifndef SHADERHELPER_INCLUDED
#define SHADERHELPER_INCLUDED

#include <GL/gl.h>
#include <GL/Extensions/GLARBShaderObjects.h>

GLhandleARB compileVertexShader(const char* vertexShaderFileName); // Devuelve un identificador a un sombreador de vértices compilado a partir del archivo fuente dado en el directorio de sombreadores de SARndbox
GLhandleARB compileFragmentShader(const char* fragmentShaderFileName); // Devuelve un identificador a un sombreador de fragmentos compilado del archivo fuente dado en el directorio de sombreadores de SARndbox
GLhandleARB linkVertexAndFragmentShader(const char* shaderFileName); // Devuelve un identificador a un programa de sombreador vinculado desde un sombreador de vértices y un sombreador de fragmentos compilado a partir de los archivos fuente dados en el directorio de sombreadores de SARndbox

#endif
