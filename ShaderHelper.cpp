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

#include "ShaderHelper.h"

#include <string>
#include <GL/gl.h>
#include <GL/Extensions/GLARBFragmentShader.h>
#include <GL/Extensions/GLARBShaderObjects.h>
#include <GL/Extensions/GLARBVertexShader.h>

#include "Config.h"

GLhandleARB compileVertexShader(const char* vertexShaderFileName)
	{
	/* Construya el nombre completo del archivo de origen del sombreador: */
	std::string fullShaderFileName=CONFIG_SHADERDIR;
	fullShaderFileName.push_back('/');
	fullShaderFileName.append(vertexShaderFileName);
	fullShaderFileName.append(".vs");
	
	/* Compila y devuelve el sombreador de vértices: */
	return glCompileVertexShaderFromFile(fullShaderFileName.c_str());
	}

GLhandleARB compileFragmentShader(const char* fragmentShaderFileName)
	{
	/* Construya el nombre completo del archivo de origen del sombreador: */
	std::string fullShaderFileName=CONFIG_SHADERDIR;
	fullShaderFileName.push_back('/');
	fullShaderFileName.append(fragmentShaderFileName);
	fullShaderFileName.append(".fs");
	
	/* Compila y devuelve el fragmento shader: */
	return glCompileFragmentShaderFromFile(fullShaderFileName.c_str());
	}

GLhandleARB linkVertexAndFragmentShader(const char* shaderFileName)
	{
	/* Compila los sombreadores de vértices y fragmentos: */
	GLhandleARB vertexShader=compileVertexShader(shaderFileName);
	GLhandleARB fragmentShader=compileFragmentShader(shaderFileName);
	
	/* Enlace el programa de sombreado: */
	GLhandleARB shaderProgram=glLinkShader(vertexShader,fragmentShader);
	
	/* Libere los sombreadores compilados (no se eliminarán hasta que se lance el programa de sombreado): */
	glDeleteObjectARB(vertexShader);
	glDeleteObjectARB(fragmentShader);
	
	return shaderProgram;
	}
