/***********************************************************************
DepthImageRenderer: Clase para centralizar el almacenamiento de imágenes 
de profundidad sin procesar o filtradas en la GPU, y realizar tareas de 
representación repetitivas simples, como representar valores de elevación 
en un búfer de cuadros.
Copyright (c) 2014-2018 Oliver Kreylos

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

#ifndef DEPTHIMAGERENDERER_INCLUDED
#define DEPTHIMAGERENDERER_INCLUDED

#include <GL/gl.h>
#include <GL/Extensions/GLARBShaderObjects.h>
#include <GL/GLObject.h>
#include <GL/GLGeometryVertex.h>
#include <Kinect/FrameBuffer.h>
#include <Kinect/FrameSource.h>

#include "Types.h"

class DepthImageRenderer:public GLObject
	{
	/* Clases integradas: */
	private:
	typedef GLGeometry::Vertex<void,0,void,0,void,GLfloat,2> Vertex; // Escriba para vértices de plantilla
	
	struct DataItem:public GLObject::DataItem // Estructura que almacena el estado OpenGL por contexto
		{
		/* Elementos: */
		public:
		
		/* Gestión de estado de OpenGL: */
		GLuint vertexBuffer; // ID del objeto de búfer de vértices que contiene los vértices de la plantilla de la superficie
		GLuint indexBuffer; // ID del objeto buffer de índice que contiene triángulos de superficie
		GLuint depthTexture; // ID del objeto de textura que sostiene las elevaciones de vértices de la superficie en el espacio de imagen de profundidad
		unsigned int depthTextureVersion; // Número de versión de la textura de la imagen de profundidad.
		
		/* Gestión de sombreadores GLSL: */
		GLhandleARB depthShader; // Programa de sombreado para representar solo la profundidad de la superficie
		GLint depthShaderUniforms[2]; // Ubicaciones de las variables uniformes del sombreador de profundidad.
		GLhandleARB elevationShader; // Programa de sombreado para representar la elevación de la superficie en relación con un plano
		GLint elevationShaderUniforms[4]; // Ubicaciones de las variables uniformes del sombreador de elevación
		
		/* Constructores y destructores: */
		DataItem(void);
		virtual ~DataItem(void);
		};
	
	/* Elementos: */
	unsigned int depthImageSize[2]; // Tamaño de textura de imagen de profundidad
	Kinect::LensDistortion lensDistortion; // Parámetros de distorsión de lentes 2D
	PTransform depthProjection; // Matriz de proyección desde el espacio de imagen en profundidad al espacio de la cámara 3D
	GLfloat depthProjectionMatrix[16]; // Igual, en formato compatible con GLSL
	GLfloat weightDicEq[4]; // Ecuación para calcular el peso de un punto de espacio de imagen de profundidad en el espacio de la cámara 3D
	Plane basePlane; // Plano base para calcular la elevación de la superficie
	GLfloat basePlaneDicEq[4]; // Ecuación del plano base en el espacio de imagen en profundidad en formato compatible con GLSL
	
	/* Estado transitorio: */
	Kinect::FrameBuffer depthImage; // La imagen de profundidad de píxel flotante más reciente
	unsigned int depthImageVersion; // Número de versión de la imagen de profundidad.
	
	/* Constructores y destructores: */
	public:
	DepthImageRenderer(const unsigned int sDepthImageSize[2]); // Crea un renderizador de elevación para el tamaño de imagen de profundidad dado
	
	/* Métodos de GLObject: */
	virtual void initContext(GLContextData& contextData) const;
	
	/* Nuevos métodos: */
	const unsigned int* getDepthImageSize(void) const // Devuelve el tamaño de la imagen de profundidad.
		{
		return depthImageSize;
		}
	unsigned int getDepthImageSize(int index) const // Devuelve un componente del tamaño de imagen de profundidad
		{
		return depthImageSize[index];
		}
	const PTransform& getDepthProjection(void) const // Devuelve la matriz de profundidad de proyección
		{
		return depthProjection;
		}
	const Plane& getBasePlane(void) const // Devuelve el plano base de elevación
		{
		return basePlane;
		}
	void setDepthProjection(const PTransform& newDepthProjection); // Establece una nueva matriz de proyección de profundidad
	void setIntrinsics(const Kinect::FrameSource::IntrinsicParameters& ips); // Establece una nueva matriz de proyección de profundidad y, si está presente, parámetros de distorsión de lente 2D
	void setBasePlane(const Plane& newBasePlane); // Establece un nuevo plano base para la representación de elevación
	void setDepthImage(const Kinect::FrameBuffer& newDepthImage); // Establece una nueva imagen de profundidad para la posterior representación de la superficie.
	Scalar intersectLine(const Point& p0,const Point& p1,Scalar elevationMin,Scalar elevationMax) const; // Interseca un segmento de línea con la imagen de profundidad actual en el espacio de la cámara; devuelve el parámetro del punto de intersección a lo largo de la línea
	unsigned int getDepthImageVersion(void) const // Devuelve el número de versión de la imagen de profundidad actual
		{
		return depthImageVersion;
		}
	void uploadDepthProjection(GLint location) const; // Carga la matriz de proyección de profundidad en la matriz GLSL 4x4 en la ubicación uniforme dada
	void bindDepthTexture(GLContextData& contextData) const; // Vincula la imagen de textura de profundidad actualizada a la unidad de textura activa actualmente
	void renderSurfaceTemplate(GLContextData& contextData) const; // Representa la malla de la tira cuádruple de la plantilla utilizando la configuración actual de OpenGL
	void renderDepth(const PTransform& projectionModelview,GLContextData& contextData) const; // Representa la superficie en un búfer de profundidad pura, para el sacrificio inicial de z o pases de sombra, etc.
	void renderElevation(const PTransform& projectionModelview,GLContextData& contextData) const; // Representa la elevación de la superficie en relación con el plano base en el búfer de marco con valor de coma flotante de un componente actual
	};

#endif
