/***********************************************************************
FrameFilter: Clase para filtrar flujos de cuadros de profundidad que llegan
desde una cámara de profundidad, con código para detectar valores inestables 
en cada píxel y rellenar los orificios resultantes de muestras no válidas.
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

#include "FrameFilter.h"

#include <Misc/FunctionCalls.h>
#include <Geometry/HVector.h>
#include <Geometry/Matrix.h>
#include <iostream>

/****************************
Methods of class FrameFilter:
****************************/

void* FrameFilter::filterThreadMethod(void)
	{
	unsigned int lastInputFrameVersion=0;
	std::cout<<"filterThreadMethod" << std::endl;
	while(true)
	{
		Kinect::FrameBuffer frame;
		{
		Threads::MutexCond::Lock inputLock(inputCond);
		
		/* Espere hasta que llegue un nuevo marco o el programa se apague: */
		while(runFilterThread && lastInputFrameVersion == inputFrameVersion)
			inputCond.wait(inputLock);
		
		/* Salte si el programa se está cerrando: */
		if(!runFilterThread)
			break;
		
		/* Trabajar en el nuevo marco: */
		frame = inputFrame;
		lastInputFrameVersion=inputFrameVersion;
		}
		
		/* Preparar un nuevo marco de salida: */
		Kinect::FrameBuffer& newOutputFrame=outputFrames.startNewValue();
		
		/* Ingrese el nuevo marco en el búfer de promedio y calcule los valores de píxeles de los marcos de salida: */
		const RawDepth* ifPtr=inputFrame.getData<RawDepth>();
		RawDepth* abPtr = averagingBuffer + averagingSlotIndex*size[1]*size[0];
		unsigned int* sPtr = statBuffer;
		float* ofPtr=validBuffer;
		float* nofPtr=newOutputFrame.getData<float>();
		const PixelDepthCorrection* pdcPtr=pixelDepthCorrection;
		for(unsigned int y=0;y<size[1];++y)
			{
			float py=float(y)+0.5f;
			for(unsigned int x=0;x<size[0];++x,++ifPtr,++pdcPtr,++abPtr,sPtr+=3,++ofPtr,++nofPtr)
				{
				float px=float(x)+0.5f;
				
				unsigned int oldVal=*abPtr;
				unsigned int newVal=*ifPtr;
				
				/* Corrija en profundidad el nuevo valor: */
				float newCVal=pdcPtr->correct(newVal);
				
				/* Conecte el nuevo valor corregido en profundidad en las ecuaciones de plano mínimo y máximo para determinar su validez: */
				float minD=minPlane[0]*px+minPlane[1]*py+minPlane[2]*newCVal+minPlane[3];
				float maxD=maxPlane[0]*px+maxPlane[1]*py+maxPlane[2]*newCVal+maxPlane[3];
				if(minD>=0.0f&&maxD<=0.0f)
					{
					/* Almacenar el nuevo valor de entrada: */
					*abPtr=newVal;
					
					/* Actualizar las estadísticas del píxel: */
					++sPtr[0]; // Número de muestras válidas
					sPtr[1]+=newVal; // Suma de muestras validas
					sPtr[2]+=newVal*newVal; // Suma de cuadrados de muestras válidas.
					
					/* Compruebe si el valor anterior en el búfer de promedio era válido: */
					if(oldVal!=2048U)
						{
						--sPtr[0]; // Número de muestras válidas
						sPtr[1]-=oldVal; // Suma de muestras validas
						sPtr[2]-=oldVal*oldVal; // Suma de cuadrados de muestras válidas.
						}
					}
				else if(!retainValids)
					{
					/* Almacenar un valor de entrada no válido: */
					*abPtr=2048U;
					
					/* Compruebe si el valor anterior en el búfer de promedio era válido: */
					if(oldVal!=2048U)
						{
						--sPtr[0]; // Número de muestras válidas
						sPtr[1]-=oldVal; // Suma de muestras validas
						sPtr[2]-=oldVal*oldVal; // Suma de cuadrados de muestras válidas.
						}
					}
				
				/* Compruebe si el píxel se considera "estable": */
				if(sPtr[0]>=minNumSamples&&sPtr[2]*sPtr[0]<=maxVariance*sPtr[0]*sPtr[0]+sPtr[1]*sPtr[1])
					{
					/* Compruebe si la nueva media de carrera corregida en profundidad está fuera de la envolvente del valor anterior: */
					float newFiltered=pdcPtr->correct(float(sPtr[1])/float(sPtr[0]));
					if(Math::abs(newFiltered-*ofPtr)>=hysteresis)
						{
						/* Establezca el valor de píxel de salida en la media de ejecución corregida en profundidad: */
						*nofPtr=*ofPtr=newFiltered;
						}
					else
						{
						/* Deja el píxel en su valor anterior: */
						*nofPtr=*ofPtr;
						}
					}
				else if(retainValids)
					{
					/* Deja el píxel en su valor anterior: */
					*nofPtr=*ofPtr;
					}
				else
					{
					/* Asignar valor predeterminado a píxeles inestables: */
					*nofPtr=instableValue;
					}
				}
			}
		
		/* Ir a la siguiente ranura de promedio: */
		if(++averagingSlotIndex == numAveragingSlots)
			averagingSlotIndex=0U;
		
		/* Aplicar un filtro espacial si se solicita: */
		if(spatialFilter)
		{
			for(int filterPass=0;filterPass<2;++filterPass)
			{
				/* Filtro de paso bajo en todo el cuadro de salida en el lugar: */
				for(unsigned int x=0;x<size[0];++x)
				{
					/* Obtener un puntero a la columna actual: */
					float* colPtr=newOutputFrame.getData<float>()+x;
					
					/* Filtrar el primer píxel en la columna: */
					float lastVal=*colPtr;
					*colPtr=(colPtr[0]*2.0f+colPtr[size[0]])/3.0f;
					colPtr+=size[0];
					
					/* Filtra los píxeles interiores en la columna: */
					for(unsigned int y=1;y<size[1]-1;++y,colPtr+=size[0])
					{
						/* Filtrar el píxel: */
						float nextLastVal=*colPtr;
						*colPtr=(lastVal+colPtr[0]*2.0f+colPtr[size[0]])*0.25f;
						lastVal=nextLastVal;
					}
					
					/* Filtrar el último píxel en la columna: */
					*colPtr=(lastVal+colPtr[0]*2.0f)/3.0f;
				}
				float* rowPtr=newOutputFrame.getData<float>();
				for(unsigned int y=0;y<size[1];++y)
				{
					/* Filtra el primer píxel en la fila: */
					float lastVal=*rowPtr;
					*rowPtr=(rowPtr[0]*2.0f+rowPtr[1])/3.0f;
					++rowPtr;
					
					/* Filtra los píxeles interiores en la fila: */
					for(unsigned int x=1;x<size[0]-1;++x,++rowPtr)
					{
						/* Filtrar el píxel: */
						float nextLastVal=*rowPtr;
						*rowPtr=(lastVal+rowPtr[0]*2.0f+rowPtr[1])*0.25f;
						lastVal=nextLastVal;
					}
					
					/* Filtra el último píxel en la fila:: */
					*rowPtr=(lastVal+rowPtr[0]*2.0f)/3.0f;
					++rowPtr;
				}
			}
		}
		
		/* Finalice el nuevo cuadro de salida en el búfer de salida: */
		outputFrames.postNewValue();
		
		/* Pase el nuevo cuadro de salida al receptor registrado: */
		if(outputFrameFunction!=0)
			(*outputFrameFunction)(newOutputFrame);
	}
	
	return 0;
	}

FrameFilter::FrameFilter(const unsigned int sSize[2],
	unsigned int sNumAveragingSlots,
	const FrameFilter::PixelDepthCorrection* sPixelDepthCorrection,
	const PTransform& depthProjection,
	const Plane& basePlane)
	:pixelDepthCorrection(sPixelDepthCorrection),
	 averagingBuffer(0),
	 statBuffer(0),
	 outputFrameFunction(0)
	{
	std::cout<<"9: FrameFilter " << std::endl;
	/* Recuerda el tamaño del marco: */
	for(int i=0;i<2;++i)
	{
		size[i]=sSize[i];// 640 x 480
	}
	
	/* Inicialice la ranura de marco de entrada: */
	inputFrameVersion=0;
	
	/* Inicialice el rango de profundidad válido: */
	FrameFilter::setValidDepthInterval(0U,2046U);
	
	/* Inicialice el búfer de promedio: */
	numAveragingSlots = sNumAveragingSlots;// 30	
	averagingBuffer=new RawDepth[numAveragingSlots*size[1]*size[0]];
	RawDepth* abPtr=averagingBuffer;
	for(unsigned int i=0;i<numAveragingSlots;++i)
		for(unsigned int y=0;y<size[1];++y)
			for(unsigned int x=0;x<size[0];++x,++abPtr)
				*abPtr=2048U; // Marcar muestra como inválida
	averagingSlotIndex=0U;
	
	/* Inicializar el búfer de estadísticas: */
	statBuffer=new unsigned int[size[1]*size[0]*3];
	unsigned int* sbPtr=statBuffer;
	for(unsigned int y=0;y<size[1];++y)
		for(unsigned int x=0;x<size[0];++x)
			for(int i=0;i<3;++i,++sbPtr)
				*sbPtr=0;
	
	/* Inicialice el criterio de estabilidad: */
	minNumSamples=(numAveragingSlots+1)/2;// Alterar
	maxVariance=4;
	hysteresis=0.1f;
	retainValids=true;
	instableValue=0.0;
	
	/* Inicializar el criterio de estabilidad: */
	spatialFilter=true;
	
	/* Convierta la ecuación del plano base del espacio de la cámara al espacio de imagen en profundidad: */
	PTransform::HVector basePlaneCc(basePlane.getNormal());
	basePlaneCc[3]=-basePlane.getOffset();
	PTransform::HVector basePlaneDic(depthProjection.getMatrix().transposeMultiply(basePlaneCc));
	basePlaneDic/=Geometry::mag(basePlaneDic.toVector());
	
	/* Inicialice el búfer válido: */
	validBuffer=new float[size[1]*size[0]];// 307200
	
	float* vbPtr=validBuffer;
	for(unsigned int y=0;y<size[1];++y)
		for(unsigned int x=0;x<size[0];++x,++vbPtr)
			*vbPtr=float(-((double(x)+0.5)*basePlaneDic[0]+(double(y)+0.5)*basePlaneDic[1]+basePlaneDic[3])/basePlaneDic[2]);
	
	/* Inicialice el buffer de cuadros de salida: */
	for(int i=0;i<3;++i)
		outputFrames.getBuffer(i)=Kinect::FrameBuffer(size[0],size[1],size[1]*size[0]*sizeof(float));
	
	/* Iniciar el hilo de filtrado: */
	runFilterThread=true;
	filterThread.start(this,&FrameFilter::filterThreadMethod);/// Importante
	}

FrameFilter::~FrameFilter(void)
	{
	/* Apague el hilo de filtrado: */
	{
	Threads::MutexCond::Lock inputLock(inputCond);
	runFilterThread=false;
	inputCond.signal();
	}
	filterThread.join();
	
	/* Liberar todos los buffers asignados: */
	delete[] averagingBuffer;
	delete[] statBuffer;
	delete[] validBuffer;
	delete outputFrameFunction;
	}

void FrameFilter::setValidDepthInterval(unsigned int newMinDepth,unsigned int newMaxDepth)
	{
	/* Establezca las ecuaciones para el plano mínimo y máximo en el espacio de imagen en profundidad: */
	minPlane[0]=0.0f;
	minPlane[1]=0.0f;
	minPlane[2]=1.0f;
	minPlane[3]=-float(newMinDepth)+0.5f;
	maxPlane[0]=0.0f;
	maxPlane[1]=0.0f;
	maxPlane[2]=1.0f;
	maxPlane[3]=-float(newMaxDepth)-0.5f;
	}

void FrameFilter::setValidElevationInterval(const PTransform& depthProjection,const Plane& basePlane,double newMinElevation,double newMaxElevation)
	{
	/* Calcule las ecuaciones de los planos de elevación mínimo y máximo en el espacio de la cámara: */
	std::cout<<"9.1: SetValidElevationInterval " << std::endl;
	PTransform::HVector minPlaneCc(basePlane.getNormal());
	minPlaneCc[3]=-(basePlane.getOffset()+newMinElevation*basePlane.getNormal().mag()); // 100.1
	PTransform::HVector maxPlaneCc(basePlane.getNormal());
	maxPlaneCc[3]=-(basePlane.getOffset()+newMaxElevation*basePlane.getNormal().mag());// 35.1
	/* Transforme las ecuaciones de los planos en el espacio de la imagen de profundidad y gire e intercambie los planos mínimo y máximo 
	   porque la elevación aumenta en oposición a la profundidad bruta: */
	PTransform::HVector minPlaneDic(depthProjection.getMatrix().transposeMultiply(minPlaneCc));
	double minPlaneScale=-1.0/Geometry::mag(minPlaneDic.toVector());// -3450.95
	for(int i=0;i<4;++i)
	{
		maxPlane[i]=float(minPlaneDic[i]*minPlaneScale);// 0.00456588, -0.0162839, 0.999857, -741.847
		//std::cout<<"maxPlane-> "<< maxPlane[i] << std::endl;
	}
	PTransform::HVector maxPlaneDic(depthProjection.getMatrix().transposeMultiply(maxPlaneCc));
	double maxPlaneScale=-1.0/Geometry::mag(maxPlaneDic.toVector());
	for(int i=0;i<4;++i)
	{
		minPlane[i]=float(maxPlaneDic[i]*maxPlaneScale);// 0.013008, -0.0463919, 0.998839, -98.405
		//std::cout<<"minPlane-> "<< minPlane[i] << std::endl;
	}
	}

void FrameFilter::setStableParameters(unsigned int newMinNumSamples,unsigned int newMaxVariance)
	{
	std::cout<<"9.2: SetValidDepthInterval " << std::endl;
	minNumSamples=newMinNumSamples;
	maxVariance=newMaxVariance;
	}

void FrameFilter::setHysteresis(float newHysteresis)
	{
	std::cout<<"9.3: SetHysteresis " << std::endl;
	hysteresis=newHysteresis;
	}

void FrameFilter::setRetainValids(bool newRetainValids)
	{
	retainValids=newRetainValids;
	}

void FrameFilter::setInstableValue(float newInstableValue)
	{
	instableValue=newInstableValue;
	}

void FrameFilter::setSpatialFilter(bool newSpatialFilter)
	{
	std::cout<<"9.4: SetSpatialFilter " << std::endl;
	spatialFilter=newSpatialFilter;
	}

void FrameFilter::setOutputFrameFunction(FrameFilter::OutputFrameFunction* newOutputFrameFunction)
	{
	std::cout<<"9.5: SetOutputFrameFunction " << std::endl;
	delete outputFrameFunction;
	outputFrameFunction=newOutputFrameFunction;
	}

void FrameFilter::receiveRawFrame(const Kinect::FrameBuffer& newFrame)
	{
	Threads::MutexCond::Lock inputLock(inputCond);
	
	/* Almacena el nuevo buffer en el buffer de entrada: */
	inputFrame=newFrame;
	++inputFrameVersion;
	
	/* Señale el hilo de fondo: */
	inputCond.signal();
	}
