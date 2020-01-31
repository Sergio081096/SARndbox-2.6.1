/***********************************************************************
HandExtractor - Clase para identificar manos a partir de una imagen de 
profundidad.
Copyright (c) 2015-2016 Oliver Kreylos

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

#ifndef HANDEXTRACTOR_INCLUDED
#define HANDEXTRACTOR_INCLUDED

#include <stddef.h>
#include <vector>
#include <Misc/SizedTypes.h>
#include <Threads/Thread.h>
#include <Threads/MutexCond.h>
#include <Threads/TripleBuffer.h>
#include <Images/RGBImage.h>
#include <Kinect/FrameBuffer.h>
#include <Kinect/FrameSource.h>

#include "Types.h"

/* Declaraciones de reenvío: */
namespace Misc {
template <class ParameterParam>
class FunctionCall;
}

class HandExtractor
	{
	/* Clases integradas: */
	public:
	typedef Misc::UInt16 DepthPixel; // Tipo para píxeles de fotograma de profundidad
	typedef Kinect::FrameSource::DepthCorrection::PixelCorrection PixelDepthCorrection; // Escriba para factores de corrección de profundidad por píxel

	struct Hand // Estructura para informar posiciones de manos detectadas
		{
		/* Elementos: */
		public:
		Point center; // Centro de la mano en profundidad espacio de imagen
		double radius; // Radio aproximado de la mano en profundidad espacio de imagen
		int direction;
		};
	
	typedef std::vector<Hand> HandList; // Escriba para listas de posiciones de manos
	typedef Misc::FunctionCall<const HandList&> HandsExtractedFunction; // Escriba para las funciones llamadas cuando se extrae una nueva lista de manos
	
	private:
	struct EdgePixel // Estructura auxiliar que almacena un píxel de borde de una gota
		{
		/* Elementos: */
		public:
		int x,y; // Posición del píxel del borde en el marco de profundidad
		const unsigned short* biPtr; // Puntero al píxel de borde en la imagen de ID de blob
		};

	/* Elementos: */
	private:
	unsigned int depthFrameSize[2]; // Tamaño de los marcos de profundidad entrantes
	const PixelDepthCorrection* pixelDepthCorrection; // Buffer de coeficientes de corrección de profundidad por píxel
	PTransform depthProjection; // Transformación proyectiva desde el espacio de imagen de profundidad al espacio de cámara
	
	Threads::MutexCond inputCond; // Condición variable para indicar la llegada de una nueva trama de entrada
	Kinect::FrameBuffer inputFrame; // El cuadro de entrada más reciente
	unsigned int inputFrameVersion; // Número de versión del marco de entrada
	volatile bool runExtractorThread; // Marcador para mantener el hilo de extracción en segundo plano en ejecución
	Threads::Thread extractorThread; // El hilo de filtrado de fondo
	
	DepthPixel maxFgDepth; // Valor de profundidad máxima para blobs en primer plano
	unsigned int maxDepthDist; // Distancia de profundidad máxima entre píxeles adyacentes para pertenecer al mismo blob de primer plano
	unsigned int minBlobSize,maxBlobSize; // Número mínimo y máximo de píxeles para considerar un blob como candidato a mano
	unsigned short* blobIdImage; // Imagen de ID de blob por píxel con una capa límite de un píxel
	ptrdiff_t biStride; // Paso de fila en imagen de ID de blob
	static const unsigned short invalidBlobId; // ID de blob no válido
	static const int walkDx[8]; // Matriz de pasos para caminar al borde en el sentido de las agujas del reloj en x
	static const int walkDy[8]; // Matriz de pasos para caminar por el borde en el sentido de las agujas del reloj en y
	ptrdiff_t walkOffsets[8]; // Matriz de desplazamientos de puntero para pasos de borde en orden de las agujas del reloj
	unsigned int snakeLength; // Longitud de la "serpiente" caminando alrededor de los bordes de las gotas para detectar esquinas
	EdgePixel* snake; // Matriz de píxeles de serpiente
	int maxCornerEnterDist; // Distancia máxima entre la cabeza y la cola de la serpiente para ingresar al estado de la esquina
	int minCenterDist; // Distancia mínima desde el centro de la serpiente a la línea definida por su cabeza y cola para ingresar al estado de la esquina
	int minCornerExitDist; // Distancia mínima entre la cabeza y la cola de la serpiente para salir del estado de la esquina
	float minHandProbability; // Calificación de probabilidad mínima para aceptar una gota como mano
	
	Threads::TripleBuffer<HandList> extractedHands; // Triple buffer de listas de manos extraídas
	HandsExtractedFunction* handsExtractedFunction; // Función llamada cuando una nueva lista de manos extraídas está lista
	
	/* Métodos privados: */
	void* extractorThreadMethod(void); // Método para el hilo de extracción manual de fondo
	
	/* Constructores y destructores: */
	public:
	HandExtractor(const unsigned int sDepthFrameSize[2],const PixelDepthCorrection* sPixelDepthCorrection,const PTransform& sDepthProjection); // Crea un extractor manual para marcos de profundidad del tamaño dado
	private:
	HandExtractor(const HandExtractor& source); // Prohibir copia constructor
	HandExtractor& operator=(const HandExtractor& source); // Prohibir operador de asignación
	public:
	~HandExtractor(void);
	
	/* Metodos: */
	DepthPixel getMaxFgDepth(void) const // Devuelve el valor de profundidad máxima para blobs en primer plano
		{
		return maxFgDepth;
		}
	void setMaxFgDepth(DepthPixel newMaxFgDepth); // Establece el valor de profundidad máxima para blobs en primer plano
	unsigned int getMaxDepthDist(void) const // Devuelve la distancia de profundidad máxima entre píxeles adyacentes para pertenecer al mismo blob en primer plano
		{
		return maxDepthDist;
		}
	void setMaxDepthDist(unsigned int newMaxDepthDist); // Establece la distancia de profundidad máxima entre píxeles adyacentes para pertenecer al mismo blob en primer plano
	unsigned int getMinBlobSize(void) const // Devuelve el número mínimo de píxeles para considerar a un blob candidato a mano
		{
		return minBlobSize;
		}
	unsigned int getMaxBlobSize(void) const // Devuelve el número máximo de píxeles para considerar a un blob como candidato a mano
		{
		return minBlobSize;
		}
	void setBlobSizeRange(unsigned int newMinBlobSize,unsigned int newMaxBlobSize); // Establece el rango de números de píxeles para considerar un blob como candidato a mano
	unsigned int getSnakeLength(void) const // Devuelve la longitud de la "serpiente" de detección de esquinas
		{
		return snakeLength;
		}
	void setSnakeLength(unsigned int newSnakeLength); // Establece la longitud de la "serpiente" de detección de esquinas
	int getMaxCornerEnterDist(void) const // Devuelve la distancia máxima entre la cabeza y la cola de la serpiente para ingresar al estado de la esquina
		{
		return maxCornerEnterDist;
		}
	int getMinCenterDist(void) const // Devuelve la distancia mínima desde el centro de la serpiente a la línea definida por su cabeza y cola para ingresar al estado de la esquina
		{
		return minCenterDist;
		}
	int getMinCornerExitDist(void) const // Devuelve la distancia mínima entre la cabeza y la cola de la serpiente para salir del estado de la esquina
		{
		return minCornerExitDist;
		}
	void setCornerDists(int newMaxCornerEnterDist,int newMinCenterDist,int newMinCornerExitDist); // Establece distancias entre la cabeza y la cola de la serpiente para entrar y salir del estado de la esquina, respectivamente
	void extractHands(const DepthPixel* depthFrame,HandList& hands,Images::RGBImage* blobImage); // Extrae manos del marco de profundidad dado
	void setHandsExtractedFunction(HandsExtractedFunction* newHandsExtractedFunction); // Establece la función de salida; adopta un objeto functor dado
	void receiveRawFrame(const Kinect::FrameBuffer& newFrame); // Llamado para recibir un nuevo marco de profundidad sin procesar
	bool lockNewExtractedHands(void) // Bloquea la lista de salida producida más recientemente de manos extraídas para lectura; devuelve verdadero si la lista bloqueada es nueva
		{
		return extractedHands.lockNewValue();
		}
	const HandList& getLockedExtractedHands(void) const // Devuelve la lista de salida bloqueada más recientemente de manos extraídas
		{
		return extractedHands.getLockedValue();
		}
	};

#endif
/// 8b4771cb-0c9c-42c7-adee-94353e9c18d7