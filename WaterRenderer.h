/***********************************************************************
WaterRenderer: Clase para representar una superficie de agua definida 
por cuadrículas regulares de batimetría centrada en vértices y valores 
de nivel de agua centrados en celdas.
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

#ifndef WATERRENDERER_INCLUDED
#define WATERRENDERER_INCLUDED

#include <GL/gl.h>
#include <GL/Extensions/GLARBShaderObjects.h>
#include <GL/GLObject.h>
#include <GL/GLGeometryVertex.h>

#include "Types.h"

/* Declaraciones de reenvío: */
class WaterTable2;

class WaterRenderer:public GLObject
	{
	/* Clases integradas: */
	private:
	typedef GLGeometry::Vertex<void,0,void,0,void,GLfloat,2> Vertex; // Escriba para vértices de plantilla
	
	struct DataItem:public GLObject::DataItem // Estructura que almacena el estado OpenGL por contexto
		{
		/* Elementos: */
		public:
		
		/* Gestión de estado de OpenGL: */
		GLuint vertexBuffer; // ID del objeto buffer de vértices que contiene los vértices de la plantilla de la superficie del agua
		GLuint indexBuffer; // ID del objeto tampón índice que contiene los triángulos de la superficie del agua
		
		/* Gestión de sombreadores GLSL: */
		GLhandleARB waterShader; // Programa de sombreado para renderizar la superficie del agua
		GLint waterShaderUniforms[5]; // Ubicaciones de las variables uniformes del sombreador de agua.
		
		/* Constructores y destructores: */
		DataItem(void);
		virtual ~DataItem(void);
		};
	
	/* Elementos: */
	const WaterTable2* waterTable; // Tabla de agua cuya superficie de agua se representa
	unsigned int bathymetryGridSize[2]; // Tamaño de la cuadrícula de batimetría centrada en el vértice
	unsigned int waterGridSize[2]; // Tamaño de la cuadrícula de nivel de agua centrada en la celda; una celda más grande que la cuadrícula de batimetría
	GLfloat cellSize[2]; // Tamaño de celda de las rejillas de batimetría y nivel de agua en unidades de coordenadas mundiales
	PTransform gridTransform; // Transformación de vértices del espacio de la cuadrícula al espacio mundial
	PTransform tangentGridTransform; // Transformación del plano tangente transpuesto del espacio de la cuadrícula al espacio mundial
	
	/* Constructores y destructores: */
	public:
	WaterRenderer(const WaterTable2* sWaterTable); // Crea un renderizador de agua para la capa freática dada.
	
	/* Métodos de GLObject: */
	virtual void initContext(GLContextData& contextData) const;
	
	/* Nuevos métodos: */
	void render(const PTransform& projection,const OGTransform& modelview,GLContextData& contextData) const; // Renderiza la superficie del agua
	};

#endif
