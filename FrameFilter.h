/***********************************************************************
FrameFilter: Clase para filtrar secuencias de fotogramas de profundidad 
que llegan desde una cámara de profundidad, con código para detectar 
valores inestables en cada píxel y rellenar agujeros resultantes de 
muestras no válidas.
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

#ifndef FRAMEFILTER_INCLUDED
#define FRAMEFILTER_INCLUDED

#include <Threads/Thread.h>
#include <Threads/MutexCond.h>
#include <Threads/TripleBuffer.h>
#include <Kinect/FrameBuffer.h>
#include <Kinect/FrameSource.h>

#include "Types.h"

/* Declaraciones de reenvío: */
namespace Misc {
template <class ParameterParam>
class FunctionCall;
}

class FrameFilter
	{
	/* Clases integradas: */
	public:
	typedef unsigned short RawDepth; // Tipo de datos para valores de profundidad sin procesar
	typedef float FilteredDepth; // Tipo de datos para valores de profundidad filtrados
	typedef Misc::FunctionCall<const Kinect::FrameBuffer&> OutputFrameFunction; // Escriba para funciones llamadas cuando un nuevo marco de salida está listo
	typedef Kinect::FrameSource::DepthCorrection::PixelCorrection PixelDepthCorrection; // Escriba para factores de corrección de profundidad por píxel
	
	/* Elementos: */
	private:
	unsigned int size[2]; // Ancho y alto de marcos procesados
	const PixelDepthCorrection* pixelDepthCorrection; // Buffer de coeficientes de corrección de profundidad por píxel
	Threads::MutexCond inputCond; // Condición variable para indicar la llegada de una nueva trama de entrada
	Kinect::FrameBuffer inputFrame; // El cuadro de entrada más reciente
	unsigned int inputFrameVersion; // Número de versión del marco de entrada
	volatile bool runFilterThread; // Marcador para mantener el hilo de filtrado de fondo funcionando
	Threads::Thread filterThread; // El hilo de filtrado de fondo
	float minPlane[4]; // Ecuación plana del límite inferior de valores de profundidad válidos en el espacio de imagen de profundidad
	float maxPlane[4]; // Ecuación plana del límite superior de valores de profundidad válidos en el espacio de imagen de profundidad
	unsigned int numAveragingSlots; // Número de ranuras en el búfer promedio de cada píxel
	RawDepth* averagingBuffer; // Buffer para calcular promedios de carrera del valor de profundidad de cada píxel
	unsigned int averagingSlotIndex; // Índice de ranura de promedio en la que almacenar los valores de profundidad del siguiente fotograma
	unsigned int* statBuffer; // Buffer que retiene los medios de ejecución y las variaciones del valor de profundidad de cada píxel
	unsigned int minNumSamples; // Número mínimo de muestras válidas necesarias para considerar un píxel estable
	unsigned int maxVariance; // Variación máxima para considerar un píxel estable
	float hysteresis; // Cantidad por la cual un nuevo valor filtrado tiene que diferir del valor actual para actualizar
	bool retainValids; // Marque si desea retener los valores estables anteriores si un nuevo píxel es inestable, o restablecer a un valor predeterminado
	float instableValue; // Valor para asignar a píxeles inestables si retenVálidos es falso
	bool spatialFilter; // Marque si se debe aplicar un filtro espacial a valores de profundidad promediados en el tiempo
	float* validBuffer; // Buffer que contiene el valor de profundidad estable más reciente para cada píxel
	Threads::TripleBuffer<Kinect::FrameBuffer> outputFrames; // Triple buffer de tramas de salida
	OutputFrameFunction* outputFrameFunction; // Función llamada cuando un nuevo marco de salida está listo
	
	/* Métodos privados: */
	void* filterThreadMethod(void); // Método para el hilo de filtrado de fondo
	
	/* Constructores y destructores: */
	public:
	FrameFilter(const unsigned int sSize[2],unsigned int sNumAveragingSlots,const PixelDepthCorrection* sPixelDepthCorrection,const PTransform& depthProjection,const Plane& basePlane); // Crea un filtro para cuadros del tamaño dado y la longitud promedio de carrera dada
	~FrameFilter(void); // Destruye el filtro de marco
	
	/* Métodos: */
	void setValidDepthInterval(unsigned int newMinDepth,unsigned int newMaxDepth); // Establece el intervalo de valores de profundidad considerado por el filtro de imagen de profundidad
	void setValidElevationInterval(const PTransform& depthProjection,const Plane& basePlane,double newMinElevation,double newMaxElevation); // Establece el intervalo de elevaciones en relación con el plano base dado considerado por el filtro de imagen de profundidad
	void setStableParameters(unsigned int newMinNumSamples,unsigned int newMaxVariance); // Establece las propiedades estadísticas para considerar un píxel estable
	void setHysteresis(float newHysteresis); // Establece la envolvente de histéresis de valor estable
	void setRetainValids(bool newRetainValids); // Establece si el filtro retiene valores estables anteriores para píxeles inestables
	void setInstableValue(float newInstableValue); // Establece el valor de profundidad para asignar a píxeles inestables
	void setSpatialFilter(bool newSpatialFilter); // Establece la bandera de filtrado espacial
	void setOutputFrameFunction(OutputFrameFunction* newOutputFrameFunction); // Establece la función de salida; adopta un objeto functor dado
	void receiveRawFrame(const Kinect::FrameBuffer& newFrame); // Llamado para recibir un nuevo marco de profundidad sin procesar
	bool lockNewFrame(void) // Bloquea el marco de salida producido más recientemente para lectura; devuelve verdadero si el marco bloqueado es nuevo
		{
		return outputFrames.lockNewValue();
		}
	const Kinect::FrameBuffer& getLockedFrame(void) const // Devuelve el marco de salida bloqueado más recientemente
		{
		return outputFrames.getLockedValue();
		}
	};

#endif
