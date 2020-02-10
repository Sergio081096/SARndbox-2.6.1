/***********************************************************************
SurfaceRenderer: Clase para representar una superficie definida por 
una cuadrícula regular en el espacio de imagen de profundidad.
Copyright (c) 2012-2018 Oliver Kreylos

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

#ifndef SURFACERENDERER_INCLUDED
#define SURFACERENDERER_INCLUDED

#include <IO/FileMonitor.h>
#include <Geometry/ProjectiveTransformation.h>
#include <Geometry/Plane.h>
#include <GL/gl.h>
#include <GL/Extensions/GLARBShaderObjects.h>
#include <GL/GLObject.h>
#include <Kinect/FrameBuffer.h>

#include "Types.h"

/* Declaraciones de reenvío: */
class DepthImageRenderer;
class ElevationColorMap;
class GLLightTracker;
class DEM;
class WaterTable2;

class SurfaceRenderer:public GLObject
	{
	/* Clases integradas: */
	public:
	typedef Geometry::Plane<GLfloat,3> Plane; // Escriba para ecuaciones planas
	
	private:
	struct DataItem:public GLObject::DataItem
		{
		/* Elementos: */
		public:
		GLuint contourLineFramebufferSize[2]; // Anchura y altura actuales del búfer del marco de representación de línea de contorno
		GLuint contourLineFramebufferObject; // Objeto buffer de trama utilizado para representar líneas de contorno topográficas
		GLuint contourLineDepthBufferObject; // Tampón de renderizado de profundidad para tampón de marco de línea de contorno topográfico
		GLuint contourLineColorTextureObject; // Objeto de textura de color para búfer de marco de línea de contorno topográfico
		unsigned int contourLineVersion; // Número de versión de la imagen de profundidad utilizada para la generación de líneas de contorno
		GLhandleARB heightMapShader; // Programa de sombreado para renderizar la superficie usando un mapa de color de altura
		GLint heightMapShaderUniforms[16]; // Ubicaciones de las variables uniformes del sombreador del mapa de altura
		unsigned int surfaceSettingsVersion; // Número de versión de la configuración de superficie para la cual se construyó el sombreador de mapa de altura
		unsigned int lightTrackerVersion; // Número de versión del estado del rastreador de luz para el que se construyó el sombreador del mapa de altura
		GLhandleARB globalAmbientHeightMapShader; // Programa de sombreado para representar el componente ambiental global de la superficie utilizando un mapa de color de altura
		GLint globalAmbientHeightMapShaderUniforms[13]; // Ubicaciones de las variables uniformes del sombreador del mapa de altura ambiental global
		GLhandleARB shadowedIlluminatedHeightMapShader; // Programa de sombreado para renderizar la superficie usando iluminación con sombras y un mapa de color de altura
		GLint shadowedIlluminatedHeightMapShaderUniforms[14]; // Ubicaciones de las variables uniformes del sombreador del mapa de altura iluminado sombreado
		
		/* Constructores y destructores: */
		DataItem(void);
		virtual ~DataItem(void);
		};
	
	/* Elementos: */
	const DepthImageRenderer* depthImageRenderer; // Renderizador para renderizado de superficie de bajo nivel
	unsigned int depthImageSize[2]; // Tamaño de textura de imagen de profundidad
	PTransform tangentDepthProjection; // Matriz de proyección de profundidad transpuesta para planos tangentes, es decir, vectores normales homogéneos
	IO::FileMonitor fileMonitor; // Monitor para ver los archivos fuente del sombreador externo del renderizador
	
	bool drawContourLines; // Marcar si las líneas de contorno topográficas están habilitadas
	GLfloat contourLineFactor; // Distancia de elevación inversa entre líneas de contorno topográficas adyacentes
	
	ElevationColorMap* elevationColorMap; // Puntero a un mapa de color para colorear el mapa de elevación topográfico
	
	bool drawDippingBed; // Marcar para dibujar un plano de cama potencialmente sumergido
	bool dippingBedFolded; // Marque si la cama de inmersión está doblada o es plana
	Plane dippingBedPlane; // Ecuación plana del lecho de inmersión plano
	GLfloat dippingBedCoeffs[5]; // Coeficientes de lecho de inmersión plegado
	GLfloat dippingBedThickness; // Espesor de la cama de inmersión en unidades de espacio de cámara
	
	DEM* dem; // Puntero a un modelo de elevación digital prefabricado para crear una superficie cero para mapeo de color de altura
	GLfloat demDistScale; // Desviación máxima de superficie a DEM en unidades de espacio de cámara
	
	bool illuminate; // Marque si la superficie debe estar iluminada
	
	bool lava;
	WaterTable2* waterTable; // Puntero al objeto de la capa freática; si es NULL, se ignora el agua
	bool advectWaterTexture; // Marque si las coordenadas de textura del agua se advectan para visualizar el flujo de agua
	GLfloat waterOpacity; // Factor de escala para la opacidad del agua.
	
	unsigned int surfaceSettingsVersion; // Número de versión de la configuración de superficie para invalidar el sombreado de representación de superficie en los cambios
	double animationTime; // Valor de tiempo para la animación del agua.
	
	/* Métodos privados: */
	void shaderSourceFileChanged(const IO::FileMonitor::Event& event); // Devolución de llamada cuando se cambia uno de los archivos de origen del sombreador externo
	GLhandleARB createSinglePassSurfaceShader(const GLLightTracker& lt,GLint* uniformLocations) const; // Crea un sombreador de renderizado de superficie de un solo paso basado en la configuración actual del renderizador
	void renderPixelCornerElevations(const int viewport[4],const PTransform& projectionModelview,GLContextData& contextData,DataItem* dataItem) const; // Crea texturas que contienen elevaciones de esquina de píxeles basadas en la imagen de profundidad actual
	
	/* Constructores y destructores: */
	public:
	SurfaceRenderer(const DepthImageRenderer* sDepthImageRenderer); // Crea un renderizador para el renderizador de imagen de profundidad dado
	
	/* Métodos de GLObject: */
	virtual void initContext(GLContextData& contextData) const;
	
	/* Nuevos métodos: */
	void setDrawContourLines(bool newDrawContourLines); // Habilita o deshabilita las líneas de contorno topográficas.
	void setContourLineDistance(GLfloat newContourLineDistance); // Establece la distancia de elevación entre las líneas de contorno topográficas adyacentes.
	void setElevationColorMap(ElevationColorMap* newElevationColorMap); // Establece un mapa de color de elevación
	void setDrawDippingBed(bool newDrawDippingBed); // Establece la bandera de la cama de inmersión.
	void setDippingBedPlane(const Plane& newDippingBedPlane); // Establece la ecuación del plano de la cama de inmersión
	void setDippingBedCoeffs(const GLfloat newDippingBedCoeffs[5]); // Establece los coeficientes de la cama de inmersión plegable
	void setDippingBedThickness(GLfloat newDippingBedThickness); // Establece el grosor de la cama de inmersión en unidades de espacio de cámara
	void setDem(DEM* newDem); // Establece un modelo de elevación digital prefabricado para crear una superficie cero para la asignación de color de altura
	void setDemDistScale(GLfloat newDemDistScale); // Establece la desviación de DEM a superficie para saturar el mapa de color de desviación
	void setIlluminate(bool newIlluminate); // Establece la bandera de iluminación.
	void setLava(bool newLava); // Establece la bandera de lava.
	void setWaterTable(WaterTable2* newWaterTable); // Establece el puntero a la capa freática; NULL deshabilita el manejo del agua
	void setAdvectWaterTexture(bool newAdvectWaterTexture); // Establece la bandera de advección de coordenadas de textura de agua
	void setWaterOpacity(GLfloat newWaterOpacity); // Establece el factor de opacidad del agua.
	void setAnimationTime(double newAnimationTime); // Establece el tiempo para la animación del agua en segundos.
	void renderSinglePass(const int viewport[4],const PTransform& projection,const OGTransform& modelview,GLContextData& contextData) const; // Representa la superficie en una sola pasada utilizando la configuración de superficie actual
	#if 0
	void renderGlobalAmbientHeightMap(GLuint heightColorMapTexture,GLContextData& contextData) const; // Renders the global ambient component of the surface as an illuminated height map in the current OpenGL context using the given pixel-corner elevation texture and 1D height color map
	void renderShadowedIlluminatedHeightMap(GLuint heightColorMapTexture,GLuint shadowTexture,const PTransform& shadowProjection,GLContextData& contextData) const; // Renders the surface as an illuminated height map in the current OpenGL context using the given pixel-corner elevation texture and 1D height color map
	#endif
	};

#endif
