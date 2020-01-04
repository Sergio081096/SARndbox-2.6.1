/***********************************************************************
DEM: Clase para representar modelos digitales de elevación (DEM) como 
objetos de textura de valor flotante.
Copyright (c) 2013-2016 Oliver Kreylos

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

#ifndef DEM_INCLUDED
#define DEM_INCLUDED

#include <GL/gl.h>
#include <GL/GLObject.h>

#include "Types.h"

class DEM:public GLObject
	{
	/* Clases integradas: */
	private:
	struct DataItem:public GLObject::DataItem
		{
		/* Elementos: */
		public:
		GLuint textureObjectId; // ID del objeto de textura con modelo de elevación digital
		
		/* Constructores y destructores: */
		DataItem(void);
		virtual ~DataItem(void);
		};
	
	/* Elementos: */
	private:
	int demSize[2]; // Ancho y alto de la cuadrícula DEM
	Scalar demBox[4]; // Coordenadas de la esquina inferior izquierda y superior derecha del DEM
	float* dem; // Matriz de mediciones de elevación DEM
	OGTransform transform; // Transformación del espacio de la cámara al espacio DEM (z arriba)
	Scalar verticalScale; // Factor de escala vertical (exageración)
	Scalar verticalScaleBase; // Elevación base alrededor de la cual se aplica la escala vertical
	PTransform demTransform; // Matriz de transformación completa del espacio de la cámara al espacio de píxeles DEM
	GLfloat demTransformMatrix[16]; // Matriz de transformación completa del espacio de la cámara al espacio de píxeles DEM para cargar en OpenGL
	
	/* Métodos privados: */
	void calcMatrix(void); // Calcula el espacio de la cámara a la transformación de espacio de píxeles DEM
	
	/* Constructores y destructores: */
	public:
	DEM(void); // Crea un DEM no inicializado
	virtual ~DEM(void);
	
	/* Métodos de la clase GLObject: */
	virtual void initContext(GLContextData& contextData) const;
	
	/* Nuevos métodos: */
	void load(const char* demFileName); // Carga el DEM del archivo dado
	const Scalar* getDemBox(void) const // Devuelve el cuadro delimitador del DEM como x inferior izquierda, inferior izquierda y, superior derecha x, superior derecha y
		{
		return demBox;
		}
	float calcAverageElevation(void) const; // Calcula la elevación promedio del DEM
	void setTransform(const OGTransform& newTransform,Scalar newVerticalScale,Scalar newVerticalScaleBase); // Establece la transformación DEM
	const PTransform& getDemTransform(void) const // Devuelve la transformación completa del espacio de la cámara al espacio de píxeles DEM escalado verticalmente
		{
		return demTransform;
		}
	Scalar getVerticalScale(void) const // Devuelve el factor de escala de las elevaciones del espacio de la cámara a las elevaciones DEM
		{
		return transform.getScaling()/verticalScale;
		}
	void bindTexture(GLContextData& contextData) const; // Vincula el objeto de textura DEM a la unidad de textura actualmente activa
	void uploadDemTransform(GLint location) const; // Carga la transformación DEM en la matriz GLSL 4x4 en la ubicación uniforme dada
	};

#endif
