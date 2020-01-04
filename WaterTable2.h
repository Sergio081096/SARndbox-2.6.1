/***********************************************************************
WaterTable2: Clase para simular el flujo de agua sobre una superficie 
utilizando una simulación de flujo de agua mejorada basada en el sistema 
de ecuaciones diferenciales parciales de Saint-Venant.
Copyright (c) 2012-2016 Oliver Kreylos

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

#ifndef WATERTABLE2_INCLUDED
#define WATERTABLE2_INCLUDED

#include <vector>
#include <Misc/FunctionCalls.h>
#include <Geometry/Point.h>
#include <Geometry/Box.h>
#include <Geometry/OrthonormalTransformation.h>
#include <GL/gl.h>
#include <GL/Extensions/GLARBShaderObjects.h>
#include <GL/GLObject.h>
#include <GL/GLContextData.h>

#include "Types.h"

/* Declaraciones de reenvío: */
class DepthImageRenderer;

typedef Misc::FunctionCall<GLContextData&> AddWaterFunction; // Escriba para las funciones de representación llamadas para agregar agua localmente a la capa freática

class WaterTable2:public GLObject
	{
	/* Clases integradas: */
	public:
	typedef Geometry::Box<Scalar,3> Box;
	typedef Geometry::OrthonormalTransformation<Scalar,3> ONTransform;
	
	private:
	struct DataItem:public GLObject::DataItem // Estructura que mantiene el estado por contexto
		{
		/* Elementos: */
		public:
		GLuint bathymetryTextureObjects[2]; // Objeto de textura de color flotante de un componente con doble búfer que sostiene la cuadrícula de batimetría centrada en el vértice
		int currentBathymetry; // Índice de textura de batimetría que contiene la cuadrícula de batimetría más reciente
		unsigned int bathymetryVersion; // Número de versión de la cuadrícula de batimetría más reciente
		GLuint quantityTextureObjects[3]; // Objeto de textura de color de tres componentes con doble búfer que contiene la cuadrícula de cantidad conservada centrada en la celda (w, hu, hv)
		int currentQuantity; // Índice de textura de cantidad que contiene la cuadrícula de cantidad conservada más reciente
		GLuint derivativeTextureObject; // Objeto de textura de color de tres componentes que contiene la cuadrícula derivada temporal centrada en la celda
		GLuint maxStepSizeTextureObjects[2]; // Objetos de textura de color de un componente con doble búfer para reunir el tamaño de paso máximo para los pasos de integración de Runge-Kutta
		GLuint waterTextureObject; // Objeto de textura de color de un componente para agregar o eliminar agua a / de la cuadrícula de cantidad conservada
		GLuint bathymetryFramebufferObject; // Tampón de marco utilizado para representar la superficie de batimetría en la cuadrícula de batimetría
		GLuint derivativeFramebufferObject; // Memoria intermedia de trama utilizada para el cálculo derivativo temporal
		GLuint maxStepSizeFramebufferObject; // El buffer de trama se usa para calcular el tamaño máximo del paso de integración
		GLuint integrationFramebufferObject; // Frame buffer utilizado para los pasos de integración de Euler y Runge-Kutta
		GLuint waterFramebufferObject; // Frame buffer utilizado para el paso de renderizado de agua
		GLhandleARB bathymetryShader; // Shader para actualizar cantidades conservadas centradas en celdas después de un cambio en la cuadrícula de batimetría
		GLint bathymetryShaderUniformLocations[3];
		GLhandleARB waterAdaptShader; // Shader para adaptar una nueva cuadrícula de cantidad conservada a la cuadrícula de batimetría actual
		GLint waterAdaptShaderUniformLocations[2];
		GLhandleARB derivativeShader; // Shader para calcular flujos parciales centrados en la cara y derivadas temporales centradas en la celda
		GLint derivativeShaderUniformLocations[6];
		GLhandleARB maxStepSizeShader; // Shader para calcular un tamaño de paso máximo para un paso de integración Runge-Kutta posterior
		GLint maxStepSizeShaderUniformLocations[2];
		GLhandleARB boundaryShader; // Shader para imponer condiciones de contorno en la cuadrícula de cantidades
		GLint boundaryShaderUniformLocations[1];
		GLhandleARB eulerStepShader; // Shader para calcular un paso de integración de Euler
		GLint eulerStepShaderUniformLocations[4];
		GLhandleARB rungeKuttaStepShader; // Shader para calcular un paso de integración Runge-Kutta
		GLint rungeKuttaStepShaderUniformLocations[5];
		GLhandleARB waterAddShader; // Shader para renderizar objetos sumadores de agua
		GLint waterAddShaderUniformLocations[3];
		GLhandleARB waterShader; // Shader para agregar o eliminar agua de la cuadrícula de cantidades conservadas
		GLint waterShaderUniformLocations[3];
		
		/* Constructores y destructores: */
		DataItem(void);
		virtual ~DataItem(void);
		};
	
	/* Elementos: */
	GLsizei size[2]; // Ancho y alto de la capa freática en píxeles
	const DepthImageRenderer* depthImageRenderer; // Objeto de renderizador utilizado para actualizar la cuadrícula de batimetría de la capa freática
	ONTransform baseTransform; // Transformación del espacio de la cámara al espacio del mapa de elevación vertical
	Box domain; // Dominio del espacio del mapa de elevación en el espacio girado de la cámara
	GLfloat cellSize[2]; // Ancho y alto de las celdas de la capa freática en unidades de coordenadas mundiales
	PTransform bathymetryPmv; // Proyección combinada y matriz de vista de modelo para representar la superficie actual en la cuadrícula de batimetría
	PTransform waterAddPmv; // Proyección combinada y matriz de vista de modelo para representar la geometría de adición de agua en la cuadrícula de agua
	GLfloat waterAddPmvMatrix[16]; // Igual, en formato compatible con GLSL
	GLfloat theta; // Coeficiente para operador diferencial minmod limitante de flujo
	GLfloat g; // Constante de aceleración gravitacional
	GLfloat epsilon; // Coeficiente para desingularizar operador de división
	GLfloat attenuation; // Factor de atenuación para descargas parciales
	GLfloat maxStepSize; // Tamaño máximo de paso para cada paso de integración Runge-Kutta
	PTransform waterTextureTransform; // Transformación proyectiva del espacio de la cámara al espacio de textura del nivel del agua
	GLfloat waterTextureTransformMatrix[16]; // Lo mismo en formato compatible con GLSL
	std::vector<const AddWaterFunction*> renderFunctions; // Una lista de funciones que se llaman después de cada paso de simulación de flujo de agua para agregar o eliminar agua localmente de la capa freática
	GLfloat waterDeposit; // Una cantidad fija de agua agregada en cada iteración de la simulación de flujo, para evaporación, etc.
	bool dryBoundary; // Marque si se deben aplicar condiciones de límite seco al final de cada paso de simulación
	unsigned int readBathymetryRequest; // Solicitar token para volver a leer la cuadrícula de batimetría actual de la GPU
	mutable GLfloat* readBathymetryBuffer; // Buffer en el que leer la cuadrícula de batimetría actual
	mutable unsigned int readBathymetryReply; // Token de respuesta después de volver a leer la cuadrícula de batimetría actual
	
	/* Métodos privados: */
	void calcTransformations(void); // Calcula transformaciones derivadas
	GLfloat calcDerivative(DataItem* dataItem,GLuint quantityTextureObject,bool calcMaxStepSize) const; // Calcula la derivada temporal de las cantidades conservadas en el objeto de textura dado y devuelve el tamaño de paso máximo si la marca es verdadera
	
	/* Constructores y destructores: */
	public:
	WaterTable2(GLsizei width,GLsizei height,const GLfloat sCellSize[2]); // Crea una capa freática para la simulación fuera de línea.
	WaterTable2(GLsizei width,GLsizei height,const DepthImageRenderer* sDepthImageRenderer,const Point basePlaneCorners[4]); // Crea una tabla de agua del tamaño dado en píxeles, para el cuadrilátero del plano base definido por la ecuación del plano del renderizador de imágenes de profundidad y cuatro puntos de esquina
	virtual ~WaterTable2(void);
	
	/* Métodos de GLObject: */
	virtual void initContext(GLContextData& contextData) const;
	
	/* Nuevos métodos: */
	const GLsizei* getSize(void) const // Devuelve el tamaño de la capa freática.
		{
		return size;
		}
	const ONTransform& getBaseTransform(void) const // Devuelve la transformación del espacio de la cámara al espacio del mapa de elevación vertical
		{
		return baseTransform;
		}
	const Box& getDomain(void) const // Devuelve el dominio de la capa freática en el espacio girado de la cámara
		{
		return domain;
		}
	const GLfloat* getCellSize(void) const // Devuelve el tamaño de celda de la capa freática.
		{
		return cellSize;
		}
	GLfloat getAttenuation(void) const // Devuelve el factor de atenuación para descargas parciales
		{
		return attenuation;
		}
	bool getDryBoundary(void) const // Devuelve verdadero si se aplican límites secos después de cada paso de simulación
		{
		return dryBoundary;
		}
	void setElevationRange(Scalar newMin,Scalar newMax); // Establece el rango de elevaciones posibles en la capa freática.
	void setAttenuation(GLfloat newAttenuation); // Establece el factor de atenuación para descargas parciales
	void setMaxStepSize(GLfloat newMaxStepSize); // Establece el tamaño de paso máximo para todos los pasos de integración posteriores
	const PTransform& getWaterTextureTransform(void) const // Devuelve la matriz que se transforma del espacio de la cámara en espacio de textura de agua
		{
		return waterTextureTransform;
		}
	void addRenderFunction(const AddWaterFunction* newRenderFunction); // Agrega una función de render a la lista; objeto sigue siendo propiedad de la persona que llama
	void removeRenderFunction(const AddWaterFunction* removeRenderFunction); // Elimina la función de representación dada de la lista pero no la elimina
	GLfloat getWaterDeposit(void) const // Devuelve la cantidad actual de agua depositada en cada paso de simulación
		{
		return waterDeposit;
		}
	void setWaterDeposit(GLfloat newWaterDeposit); // Establece la cantidad de agua depositada
	void setDryBoundary(bool newDryBoundary); // Habilita o deshabilita la aplicación de límites secos
	void updateBathymetry(GLContextData& contextData) const; // Prepara el nivel freático para llamadas posteriores al método runSimulationStep()
	void updateBathymetry(const GLfloat* bathymetryGrid,GLContextData& contextData) const; // Actualiza la batimetría directamente con una cuadrícula de elevación centrada en el vértice de tamaño de cuadrícula menos 1
	void setWaterLevel(const GLfloat* waterGrid,GLContextData& contextData) const; // Establece el nivel de agua actual en la cuadrícula dada y restablece los componentes de flujo a cero
	GLfloat runSimulationStep(bool forceStepSize,GLContextData& contextData) const; // Ejecuta un paso de simulación de flujo de agua, siempre usa maxStepSize si la marca es verdadera (puede provocar inestabilidad); devuelve el tamaño del paso tomado por el paso de integración Runge-Kutta
	void bindBathymetryTexture(GLContextData& contextData) const; // Vincula el objeto de textura batimetría a la unidad de textura activa
	void bindQuantityTexture(GLContextData& contextData) const; // Vincula el objeto de textura de cantidades conservadas más reciente a la unidad de textura activa
	void uploadWaterTextureTransform(GLint location) const; // Carga la transformación de la textura del agua en la matriz GLSL 4x4 en la ubicación uniforme dada
	GLsizei getBathymetrySize(int index) const // Devuelve el ancho o alto de la cuadrícula de batimetría
		{
		return size[index]-1;
		}
	bool requestBathymetry(GLfloat* newReadBathymetryBuffer); // Solicita la lectura de la cuadrícula de batimetría actual de la GPU durante el siguiente ciclo de representación; devuelve verdadero si se puede otorgar la solicitud
	bool haveBathymetry(void) const // Devuelve verdadero si se ha cumplido la solicitud de batimetría más reciente
		{
		return readBathymetryReply==readBathymetryRequest;
		}
	};

#endif
