/***********************************************************************
ElevationColorMap - Clase para representar mapas de color de elevación
para mapas topográficos.
Copyright (c) 2014-2016 Oliver Kreylos

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

#ifndef ELEVATIONCOLORMAP_INCLUDED
#define ELEVATIONCOLORMAP_INCLUDED

#include <GL/gl.h>
#include <GL/GLColorMap.h>
#include <GL/GLTextureObject.h>

#include "Types.h"

/* Declaraciones de reenvío: */
class DepthImageRenderer;

class ElevationColorMap:public GLColorMap,public GLTextureObject
	{
	/* Elementos: */
	public:
	GLfloat texturePlaneEq[4]; // Ecuación del plano de mapeado de texturas en formato compatible con GLSL
	
	/* Constructores y destructores: */
	public:
	ElevationColorMap(const char* heightMapName); // Crea un mapa de color de elevación al cargar el archivo de mapa de altura dado
	
	/* Métodos de GLObject: */
	virtual void initContext(GLContextData& contextData) const;
	
	/* Nuevos métodos: */
	void load(const char* heightMapName); // Anula el mapa de color de elevación al cargar el archivo de mapa de altura dado
	void calcTexturePlane(const Plane& basePlane); // Calcula el plano de mapeado de texturas para la ecuación del plano base dado
	void calcTexturePlane(const DepthImageRenderer* depthImageRenderer); // Calcula el plano de mapeado de texturas para el renderizador de imágenes de profundidad dado
	void bindTexture(GLContextData& contextData) const; // Vincula el objeto de textura del mapa de color de elevación a la unidad de textura activa actualmente
	void uploadTexturePlane(GLint location) const; // Carga la ecuación del plano de mapeado de texturas en el vector GLSL 4 en la ubicación uniforme dada
	};

#endif
