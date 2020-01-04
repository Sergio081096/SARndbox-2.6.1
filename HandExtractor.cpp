/***********************************************************************
HandExtractor - Clase para identificar manos de una imagen de profundidad.
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

#include "HandExtractor.h"

#include <Misc/Utility.h>
#include <Misc/FunctionCalls.h>
#include <Math/Math.h> 
#include <Math/Interval.h>
#include <Geometry/Vector.h>

// DEBUGGING
#include <iostream>

namespace {

/**************
Helper classes:
**************/

struct Span // Estructura auxiliar para extraer manchas de primer plano de una imagen de profundidad
	{
	/* Elements: */
	public:
	unsigned int y; // Índice de fila del tramo
	unsigned int start; // Columna inicial del tramo
	unsigned int end; // Columna final del tramo
	unsigned int parent; // Span's parent span //Padre
	unsigned int numPixels; // Número de píxeles en el subárbol del tramo.
	unsigned int blobId; // ID de blob de un tramo raíz
	};

struct BlobOrigin // Estructura auxiliar para almacenar un punto en el borde de una burbuja en primer plano
	{
	/* Elementos: */
	public:
	bool assigned; // Marca si el origen del blob ya ha sido asignado
	unsigned int x,y; // Coordenadas de origen de blob en marco de profundidad.
	const unsigned short* biPtr; // Puntero al origen de blob en la imagen de ID de blob
	};

struct Corner // Clase de ayuda para almacenar esquinas en imágenes de blob
	{
	/* Elementos: */
	public:
	int cornerType; // Tipo de esquina, +1: punta del dedo, -1: rincón del dedo
	unsigned start; // Índice de píxeles de límite en el que comenzó la esquina
	int x,y; // Posición de esquina en marco de profundidad
	};

typedef Math::Interval<float> Interval;
typedef Geometry::Point<float,2> Point2;
typedef Geometry::Vector<float,2> Vector2;

/****************
Helper functions:
****************/

void drawLine(Images::RGBImage& image,const Point2& p0,const Point2& p1,const Images::RGBImage::Color& color)
	{
	int w=int(image.getWidth());
	int h=int(image.getHeight());
	int x0=int(Math::floor(p0[0]));
	int y0=int(Math::floor(p0[1]));
	int x1=int(Math::floor(p1[0]));
	int y1=int(Math::floor(p1[1]));
	int dx=x1-x0;
	int dy=y1-y0;
	ptrdiff_t stride=w;
	if(Math::abs(dx)>Math::abs(dy))
		{
		/* X direction leads: X dirección conduce: */
		if(dx<0)
			{
			x0=x1;
			y0=y1;
			dx=-dx;
			dy=-dy;
			}
		Images::RGBImage::Color* lPtr=image.modifyPixels()+y0*stride+x0;
		int yf=dx/2;
		int y=0;
		for(int x=0;x<=dx;++x,++lPtr)
			{
			if(x0+x>=0&&x0+x<w&&y0+y>=0&&y0+y<h)
				*lPtr=color;
			yf+=dy;
			if(yf>=dx)
				{
				++y;
				lPtr+=stride;
				yf-=dx;
				}
			else if(yf<=-dx)
				{
				--y;
				lPtr-=stride;
				yf+=dx;
				}
			}
		}
	else
		{
		/* Y direction leads: Y dirección conduce:*/
		if(dy<0)
			{
			x0=x1;
			y0=y1;
			dx=-dx;
			dy=-dy;
			}
		Images::RGBImage::Color* lPtr=image.modifyPixels()+y0*stride+x0;
		int xf=dy/2;
		int x=0;
		for(int y=0;y<=dy;++y,lPtr+=stride)
			{
			if(x0+x>=0&&x0+x<w&&y0+y>=0&&y0+y<h)
				*lPtr=color;
			xf+=dx;
			if(xf>=dy)
				{
				++x;
				++lPtr;
				xf-=dy;
				}
			else if(xf<=-dy)
				{
				--x;
				--lPtr;
				xf+=dy;
				}
			}
		}
	}

void drawCircle(Images::RGBImage& image,const Point2& center,float radius,const Images::RGBImage::Color& color)
	{
	Images::RGBImage::Color* imgBase=image.modifyPixels();
	int size[2];
	size[0]=int(image.getSize(0));
	size[1]=int(image.getSize(1));
	
	/* Dibuja el círculo: */
	int cx=int(Math::floor(center[0]));
	int cy=int(Math::floor(center[1]));
	int r=int(Math::floor(radius+0.5f));
	ptrdiff_t stride=ptrdiff_t(size[0]);
	#if 0 // Draw a filled rain circle
	int ymin=cy-r>=0?cy-r:0;
	int ymax=cy+r<=size[1]-1?cy+r:size[1]-1;
	for(int y=ymin;y<=ymax;++y)
		{
		int ry=int(Math::floor(Math::sqrt(float(r*r)-float((y-cy)*(y-cy)))+0.5f));
		int xmin=cx-ry>=0?cx-ry:0;
		int xmax=cx+ry<=size[0]-1?cx+ry:size[0]-1;
		Images::RGBImage::Color* iPtr=imgBase+(y*stride+xmin);
		for(int x=xmin;x<=xmax;++x,++iPtr)
			*iPtr=color;
		}
	#else // Dibuja un círculo de lluvia hueco.
	Images::RGBImage::Color* imgCenter=imgBase+(cy*stride+cx);
	for(int y=0;;++y)
		{
		int x=int(Math::floor(Math::sqrt(float(r*r)-float(y*y))+0.5f));
		if(x<y)
			break;
		if(cy+y<size[1])
			{
			if(cx+x<size[0])
				imgCenter[y*stride+x]=color;
			if(cx-x>=0)
				imgCenter[y*stride-x]=color;
			}
		if(cy+x<size[1])
			{
			if(cx+y<size[0])
				imgCenter[x*stride+y]=color;
			if(cx-y>=0)
				imgCenter[x*stride-y]=color;
			}
		if(cy-y>=0)
			{
			if(cx+x<size[0])
				imgCenter[-y*stride+x]=color;
			if(cx-x>=0)
				imgCenter[-y*stride-x]=color;
			}
		if(cy-x>=0)
			{
			if(cx+y<size[0])
				imgCenter[-x*stride+y]=color;
			if(cx-y>=0)
				imgCenter[-x*stride-y]=color;
			}
		}
	#endif
	}

}

/**************************************
Static elements of class HandExtractor:
**************************************/

const unsigned short HandExtractor::invalidBlobId=0xffffU;
const int HandExtractor::walkDx[8]={ 1, 1, 0,-1,-1,-1, 0, 1};
const int HandExtractor::walkDy[8]={ 0, 1, 1, 1, 0,-1,-1,-1};

/******************************
Methods of class HandExtractor:
******************************/

void* HandExtractor::extractorThreadMethod(void)
	{
	unsigned int lastInputFrameVersion=0;	
	std::cout<<"11.2: ExtractorThreadMethod "<< std::endl;
	std::cout<<"11.3: ExtractHands "<< std::endl;
	while(true)
	{
		Kinect::FrameBuffer frame;
		{
		Threads::MutexCond::Lock inputLock(inputCond);
		
		/* Espere hasta que llegue un nuevo marco o el programa se apague: */
		while(runExtractorThread && lastInputFrameVersion==inputFrameVersion)
			inputCond.wait(inputLock);// agregar el anterior
		
		/* Salte si el programa se está cerrando: */
		if(!runExtractorThread)
			break;
		
		/* Trabaja en el nuevo marco: */
		frame=inputFrame;

		//std::cout<<"frame-> "<< inputFrameVersion << std::endl;
		lastInputFrameVersion=inputFrameVersion;
		}
		
		/* Prepare una nueva lista manual de salida: */
		HandList& newHandList=extractedHands.startNewValue();
		
		/* Extraer manos del nuevo marco de entrada: */
		HandExtractor::extractHands(frame.getData<DepthPixel>(),newHandList,0);
		
		/* Finalice la nueva lista de manos extraídas en el búfer de salida: */
		extractedHands.postNewValue();
		
		/* Pase el nuevo marco de salida al receptor registrado: */
		if(handsExtractedFunction!=0)
			(*handsExtractedFunction)(newHandList);
	}
	
	return 0;
	}

HandExtractor::HandExtractor(const unsigned int sDepthFrameSize[2],
	const HandExtractor::PixelDepthCorrection* sPixelDepthCorrection,
	const PTransform& sDepthProjection)
	:pixelDepthCorrection(sPixelDepthCorrection),
	 depthProjection(sDepthProjection),
	 inputFrameVersion(0),
	 runExtractorThread(false),
	 maxFgDepth(0x07ffU-1U),
	 maxDepthDist(1),
	 minBlobSize(1500),
	 maxBlobSize(150000),
	 blobIdImage(0),
	 snakeLength(50),
	 snake(0),
	 maxCornerEnterDist(28),
	 minCenterDist(10),
	 minCornerExitDist(32),
	 minHandProbability(0.15f),
	 handsExtractedFunction(0)
	{
	std::cout<<"11: HandExtractor " << std::endl;
	/* Copie el tamaño del marco de profundidad: */
	for(int i=0;i<2;++i)
	{
		depthFrameSize[i]=sDepthFrameSize[i];// 640 x 480
	}
	
	/* Asigne la imagen de ID de blob: */
	blobIdImage=new unsigned short[(depthFrameSize[1]+2)*(depthFrameSize[0]+2)];
	biStride=depthFrameSize[0]+2;// 642
	
	/* Inicialice el borde de la imagen de ID de blob: */
	unsigned short* biPtr=blobIdImage;
	for(unsigned int x=1;x<depthFrameSize[0]+2;++x,++biPtr)
		*biPtr=invalidBlobId;
	for(unsigned int y=1;y<depthFrameSize[1]+2;++y,biPtr+=biStride)
		*biPtr=invalidBlobId;
	for(unsigned int x=1;x<depthFrameSize[0]+2;++x,--biPtr)
		*biPtr=invalidBlobId;
	for(unsigned int y=1;y<depthFrameSize[1]+2;++y,biPtr-=biStride)
		*biPtr=invalidBlobId;
	
	/* Calcule la matriz de compensaciones de puntero para caminar por el borde: */
	for(int i=0;i<8;++i)
	{
		walkOffsets[i]=walkDy[i]*biStride+walkDx[i];// 1 643 642 641 * -1
	}
	
	/* Calcule la matriz de compensaciones de puntero para caminar al borde: */	
	HandExtractor::setSnakeLength(snakeLength);// 50
	
	/* Iniciar el hilo de extracción de mano: */
	runExtractorThread=true;
	extractorThread.start(this,&HandExtractor::extractorThreadMethod);
	}

HandExtractor::~HandExtractor(void)
	{
	/* Apague el hilo de extracción: */
	{
	Threads::MutexCond::Lock inputLock(inputCond);
	runExtractorThread=false;
	inputCond.signal();
	}
	extractorThread.join();
	
	delete[] blobIdImage;
	delete[] snake;
	}

void HandExtractor::setMaxFgDepth(DepthPixel newMaxFgDepth)
	{
	maxFgDepth=newMaxFgDepth;
	}

void HandExtractor::setMaxDepthDist(unsigned int newMaxDepthDist)
	{
	maxDepthDist=newMaxDepthDist;
	}

void HandExtractor::setBlobSizeRange(unsigned int newMinBlobSize,unsigned int newMaxBlobSize)
	{
	minBlobSize=newMinBlobSize;
	maxBlobSize=newMaxBlobSize;
	}

void HandExtractor::setSnakeLength(unsigned int newSnakeLength)
	{
	std::cout<<"11.1: SetSnakeLength " << std::endl;
	snakeLength=newSnakeLength;// 50	
	/* Vuelva a asignar la matriz de serpientes: */
	delete[] snake;
	snake=new EdgePixel[snakeLength];
	}

void HandExtractor::setCornerDists(int newMaxCornerEnterDist,int newMinCenterDist,int newMinCornerExitDist)
	{
	maxCornerEnterDist=newMaxCornerEnterDist;
	minCenterDist=newMinCenterDist;
	minCornerExitDist=newMinCornerExitDist;
	}

void HandExtractor::extractHands(const HandExtractor::DepthPixel* depthFrame,HandExtractor::HandList& hands,Images::RGBImage* blobImage)
	{
	//std::cout<<"11.3: ExtractHands "<< std::endl;
	Images::RGBImage::Color* imgPtr=0;
	if(blobImage!=0)
		{
		/* Crea la imagen del resultado: */
		blobImage->clear(Images::RGBImage::Color(0,0,0));
		imgPtr=blobImage->replacePixels();
		}
	
	/* Extraiga todos los blobs de primer plano conectados a cuatro del marco de profundidad dado: */
	std::vector<Span> spans;
	unsigned int numSpans=0;
	unsigned int lastRowSpan=0;
	const DepthPixel* dfRowPtr=depthFrame;
	for(unsigned int y=0;y<depthFrameSize[1];++y,dfRowPtr+=depthFrameSize[0])// 640 x 480
	{
		const DepthPixel* dfPtr=dfRowPtr;
		unsigned int rowSpan=numSpans;
		unsigned int x=0;
		while(true)
		{
			/* Encuentra el comienzo del siguiente tramo de primer plano: */
			for(;x<depthFrameSize[0] && *dfPtr>maxFgDepth; ++x,++dfPtr)
				;
			if(x>=depthFrameSize[0])
				break;
			
			/* Iniciar un nuevo tramo de primer plano: */
			Span newSpan;
			newSpan.y=y;
			newSpan.start=x;
			
			/* Traza el lapso actual en primer plano: */
			DepthPixel lastDepth=*dfPtr;
			++x;
			++dfPtr;
			for(;x<depthFrameSize[0] && *dfPtr <= maxFgDepth && *dfPtr + maxDepthDist >= lastDepth && *dfPtr <= lastDepth + maxDepthDist;++x,++dfPtr)
				lastDepth=*dfPtr;
			
			/* Finalice y almacene el nuevo lapso de primer plano: */
			newSpan.end=x;
			newSpan.parent=numSpans;
			newSpan.numPixels=newSpan.end-newSpan.start;
			newSpan.blobId=invalidBlobId;
			spans.push_back(newSpan);
			++numSpans;
			
			/* Omita cualquier intervalo de la fila anterior que acaba de pasar: */
			for(;lastRowSpan<rowSpan&&spans[lastRowSpan].end<newSpan.start;++lastRowSpan)
				;
			
			/* Compruebe si el intervalo actual se vincula con alguno de la fila anterior: */
			for(unsigned int lrs=lastRowSpan;lrs<rowSpan&&spans[lrs].start<=newSpan.end;++lrs)
			{
				/* Compruebe si los dos tramos tienen profundidad en común: */
				unsigned int o1=Misc::max(newSpan.start,spans[lrs].start);
				unsigned int o2=Misc::min(newSpan.end,spans[lrs].end);
				const DepthPixel* lrsPtr1=dfRowPtr+o1;
				const DepthPixel* lrsPtr0=lrsPtr1-depthFrameSize[0];
				bool canLink=false;
				for(unsigned int o=o1;o<o2&&!canLink;++o,++lrsPtr0,++lrsPtr1)
					canLink=*lrsPtr0+maxDepthDist >= *lrsPtr1 && *lrsPtr0 <= *lrsPtr1 + maxDepthDist;
				
				/* Combina los dos tramos si pueden enlazar: */
				if(canLink)
				{
					/* Encuentre las raíces de los respectivos subárboles de los dos tramos: */
					unsigned int root1=lrs;
					while(root1!=spans[root1].parent)
						root1=spans[root1].parent;
					unsigned int root2=numSpans-1;
					while(root2!=spans[root2].parent)
						root2=spans[root2].parent;
					
					if(root1<root2)
					{
						/* Haz que el primer lapso sea la nueva raíz: */
						spans[root2].parent=root1;
						spans[root1].numPixels+=spans[root2].numPixels;
					}
					else if(root1>root2)
					{
						/* Haz que el segundo tramo de la nueva raíz: */
						spans[root1].parent=root2;
						spans[root2].numPixels+=spans[root1].numPixels;
					}
				}
			}
		}
		
		/* Omita cualquier espacio restante de la fila anterior: */
		lastRowSpan=rowSpan;
	}
	
	/* Asigne ID de blob consecutivos a todos los tramos raíz: */
	unsigned int nextBlobId=0;
	for(unsigned int i=0;i<numSpans;++i)
	{
		/* Compruebe si el tramo es un tramo raíz: */
		if(spans[i].parent==i)
		{
			if(spans[i].numPixels>=minBlobSize&&spans[i].numPixels<=maxBlobSize)
			{
				spans[i].blobId=nextBlobId;
				++nextBlobId;
			}
		}
		else
		{
			/* Encuentre la raíz del subárbol del span: */
			unsigned int root=spans[i].parent;
			while(root!=spans[root].parent)
				root=spans[root].parent;
			
			/* Asigne la ID de blob del tramo desde la raíz: */
			spans[i].blobId=spans[root].blobId;
		}
	}
	
	#if 0
	
	/* Crea la imagen de color resultante: */
	static const Images::RGBImage::Color blobColors[18]=
		{
		Images::RGBImage::Color(255,0,0),
		Images::RGBImage::Color(255,255,0),
		Images::RGBImage::Color(0,255,0),
		Images::RGBImage::Color(0,255,255),
		Images::RGBImage::Color(0,0,255),
		Images::RGBImage::Color(255,0,255),
		Images::RGBImage::Color(128,0,0),
		Images::RGBImage::Color(128,128,0),
		Images::RGBImage::Color(0,128,0),
		Images::RGBImage::Color(0,128,128),
		Images::RGBImage::Color(0,0,128),
		Images::RGBImage::Color(128,0,128),
		Images::RGBImage::Color(255,128,128),
		Images::RGBImage::Color(255,255,128),
		Images::RGBImage::Color(128,255,128),
		Images::RGBImage::Color(128,255,255),
		Images::RGBImage::Color(128,128,255),
		Images::RGBImage::Color(255,128,255)
		};
	
	for(unsigned int i=0;i<numSpans;++i)
		{
		/* Encuentra la raíz del tramo: */
		int root=i;
		while(spans[root].parent!=root)
			root=spans[root].parent;
		
		if(spans[root].blobId!=invalidBlobId)
			{
			/* Fill in the span: */
			Images::RGBImage::Color* cPtr=result.modifyPixelRow(spans[i].y)+spans[i].start;
			for(int x=spans[i].start;x<spans[i].end;++x,++cPtr)
				*cPtr=blobColors[spans[root].blobId%18];
			}
		}
	
	#endif
	
	/* Crear una matriz de puntos de origen de blob: */
	BlobOrigin* blobOrigins=new BlobOrigin[nextBlobId];
	for(unsigned int i=0;i<nextBlobId;++i)
		blobOrigins[i].assigned=false;
	
	/* Crear la imagen de ID blob: */
	unsigned short* biRowPtr=blobIdImage+biStride+1;
	unsigned int spanIndex=0;
	for(unsigned int y=0;y<depthFrameSize[1];++y,biRowPtr+=biStride)
		{
		/* Procese todos los espacios y espacios entre los espacios en la fila actual: */
		unsigned int x=0;
		unsigned short* biPtr=biRowPtr;
		while(true)
			{
			/* Encuentre el inicio del siguiente tramo en la fila actual: */
			unsigned int nextSpanStart=depthFrameSize[0];
			if(spanIndex<numSpans&&spans[spanIndex].y==y)
				nextSpanStart=spans[spanIndex].start;
			
			/* Asigne los ID de blob no válidos hasta el inicio del siguiente intervalo: */
			for(;x<nextSpanStart;++x,++biPtr)
				*biPtr=invalidBlobId;
			
			/* Rescate si la fila actual está hecha: */
			if(x==depthFrameSize[0])
				break;
			
			/* Verifique si la burbuja del tramo actual es válida, y se encontró por primera vez:lapso */
			unsigned int blobId=spans[spanIndex].blobId;
			if(blobId<nextBlobId&&!blobOrigins[blobId].assigned)
				{
				/* Almacene el comienzo del intervalo actual como el origen del blob: */
				blobOrigins[blobId].assigned=true;
				blobOrigins[blobId].x=x;
				blobOrigins[blobId].y=y;
				blobOrigins[blobId].biPtr=biPtr;
				}
			
			/* Asigne la ID de blob del tramo actual: */
			for(;x<spans[spanIndex].end;++x,++biPtr)
				*biPtr=blobId;
			
			/* Ir al siguiente lapso: */
			++spanIndex;
			}
		}
	
	/* Inicializar la lista de resultados: */
	hands.clear();
	
	/* Moverse alrededor de los bordes de todas las burbujas de primer plano en el sentido contrario a las agujas del reloj y decida si tienen forma de mano: */
	EdgePixel* snakeEnd=snake+snakeLength;
	int enterDist2=Math::sqr(maxCornerEnterDist);
	int centerDist2=Math::sqr(minCenterDist);
	int exitDist2=Math::sqr(minCornerExitDist);
	std::vector<Corner> corners;
	corners.reserve(10);
	for(unsigned int blobId=0;blobId<nextBlobId;++blobId)
		{
		/* Inicializa la serpiente que camina al borde: */
		EdgePixel* snakeHead=snake;
		snakeHead->x=int(blobOrigins[blobId].x);
		snakeHead->y=int(blobOrigins[blobId].y);
		snakeHead->biPtr=blobOrigins[blobId].biPtr;
		unsigned int walkDir=0; // El origen del blob es el píxel inferior izquierdo del blob, por lo que 0 es la dirección inicial correcta para moverse		for(unsigned int i=1;i<snakeLength;++i)
			{
			/* Gire 90 grados en sentido horario: */
			walkDir=(walkDir+6)&0x7U;
			
			/* Gire en sentido antihorario hasta que el siguiente paso permanezca en el mismo blob: */
			while(snakeHead->biPtr[walkOffsets[walkDir]]!=blobId)
				walkDir=(walkDir+1)&0x7U;
			
			/* Camina un paso a lo largo del borde de la mancha: */
			snakeHead[1].x=snakeHead->x+walkDx[walkDir];
			snakeHead[1].y=snakeHead->y+walkDy[walkDir];
			snakeHead[1].biPtr=snakeHead->biPtr+walkOffsets[walkDir];
			
			/* Mueve la cabeza de serpiente hacia adelante: */
			++snakeHead;
			}
		EdgePixel* snakeTail=snake;
		EdgePixel* snakeMid=snake+snakeLength/2;
		
		/* Camina la serpiente exactamente una vez alrededor de la mancha: */
		Corner corner;
		corner.cornerType=0;
		int cornerDist2=0;
		unsigned int pixelIndex=0;
		int firstCornerDist2=0;
		unsigned int firstCornerStart=0;
		do
			{
			/* Compruebe si la serpiente actual se encuentra en una esquina: */
			int newCornerType=0;
			int headTailDist2=Math::sqr(snakeHead->x-snakeTail->x)+Math::sqr(snakeHead->y-snakeTail->y);
			int centerElevation2=0;
			if(headTailDist2<=enterDist2)
				{
				/* Determine el tipo de esquina comparando el punto central de la serpiente con la línea definida por su cabeza y cola: */
				int nx=snakeTail->y-snakeHead->y;
				int ny=snakeHead->x-snakeTail->x;
				int d=nx*(snakeMid->x-snakeTail->x)+ny*(snakeMid->y-snakeTail->y);
				if(Math::sqr(d)>=centerDist2*headTailDist2)
					{
					/* Introduzca el estado de la esquina: */
					if(d<0)
						newCornerType=1; // Punta del dedo
					else
						newCornerType=-1; // Rincón de los dedos
					if(headTailDist2>0)
						centerElevation2=Math::sqr(d)/headTailDist2;
					else
						centerElevation2=Math::sqr(snakeMid->x-snakeTail->x)+Math::sqr(snakeMid->y-snakeTail->y);
					}
				}
			
			/* Compruebe si la serpiente cambió el tipo de esquina desde el último paso: */
			if(corner.cornerType!=newCornerType)
				{
				if(corner.cornerType!=0)
					{
					/* Si la esquina anterior es la primera, recuerda su distancia de la esquina: */
					if(corners.empty())
						firstCornerDist2=cornerDist2;
					
					/* Almacenar la esquina anterior: */
					corners.push_back(corner);
					}
				
				if(newCornerType!=0)
					{
					/* Comience una nueva esquina: */
					corner.start=pixelIndex;
					corner.x=snakeMid->x;
					corner.y=snakeMid->y;
					cornerDist2=centerElevation2;
					
					/* Si esta es la primera esquina, recuerde su píxel inicial: */
					if(corners.empty())
						firstCornerStart=pixelIndex;
					}
				
				/* Cambiar el tipo de la esquina actual: */
				corner.cornerType=newCornerType;
				}
			else if(corner.cornerType!=0&&cornerDist2<centerElevation2)
				{
				/* Actualiza la esquina actual: */
				corner.x=snakeMid->x;
				corner.y=snakeMid->y;
				cornerDist2=centerElevation2;
				}
			
			if(imgPtr!=0)
				{
				/* Dibuja el punto central de la serpiente: */
				Images::RGBImage::Color* cPtr=imgPtr+(snakeMid->y*depthFrameSize[0]+snakeMid->x);
				if(corner.cornerType==1)
					*cPtr=Images::RGBImage::Color(96,160,96);
				else if(corner.cornerType==-1)
					*cPtr=Images::RGBImage::Color(160,96,160);
				else
					{
					#if 1
					*cPtr=Images::RGBImage::Color(128,128,128);
					#else
					for(int i=0;i<3;++i)
						(*cPtr)[i]=(*cPtr)[i]+(255U-(*cPtr)[i])/2;
					#endif
					}
				}
			
			/* Moverse un paso a lo largo del borde de la mancha: */
			walkDir=(walkDir+6)&0x7U; // Gire 90 grados en sentido antihorario
			while(snakeHead->biPtr[walkOffsets[walkDir]]!=blobId)
				walkDir=(walkDir+1)&0x7U;
			snakeTail->x=snakeHead->x+walkDx[walkDir];
			snakeTail->y=snakeHead->y+walkDy[walkDir];
			snakeTail->biPtr=snakeHead->biPtr+walkOffsets[walkDir];
			
			/* Mueve la cabeza de serpiente hacia adelante: */
			snakeHead=snakeTail;
			if(++snakeMid==snakeEnd)
				snakeMid=snake;
			if(++snakeTail==snakeEnd)
				snakeTail=snake;
			
			++pixelIndex;
			}
		while(snakeTail->biPtr!=blobOrigins[blobId].biPtr);
		
		if(corner.cornerType!=0)
			{
			if(!corners.empty()&&firstCornerStart==0&&corners.front().cornerType==corner.cornerType)
				{
				/* Combina las primeras y últimas esquinas: */
				if(firstCornerDist2<cornerDist2)
					{
					corners.front().x=corner.x;
					corners.front().y=corner.y;
					}
				}
			else
				{
				/* Almacenar la última esquina: */
				corners.push_back(corner);
				}
			}
		
		if(imgPtr!=0)
			{
			/* Dibuja todas las esquinas: */
			for(std::vector<Corner>::iterator cIt=corners.begin();cIt!=corners.end();++cIt)
				{
				Images::RGBImage::Color* cPtr=imgPtr+(cIt->y*depthFrameSize[0]+cIt->x);
				if(cIt->cornerType==1)
					*cPtr=Images::RGBImage::Color(0,255,0);
				else if(cIt->cornerType==-1)
					*cPtr=Images::RGBImage::Color(255,0,255);
				}
			}
		
		/* Compruebe si el conjunto de esquinas extraído coincide con un modelo de mano: */
		float maxProb=minHandProbability;
		Point2 center=Point2::origin; // Punto central de la mano
		float depth=0.0f; // Valor de profundidad promedio de la mano
		float radius=0.0f; // Radio de la mano
		size_t numCorners=corners.size();
		if(numCorners>=8) // Al menos cuatro puntas de los dedos, tres rincones y una punta del pulgar (rincon opcional)
			{
			for(size_t i=0;i<numCorners;++i)
				{
				/* Compruebe si la esquina actual inicia una secuencia de cuatro puntas intercaladas con tres rincones: */
				Corner& t0=corners[i];
				Corner& n1=corners[(i+1)%numCorners];
				Corner& t1=corners[(i+2)%numCorners];
				Corner& n2=corners[(i+3)%numCorners];
				Corner& t2=corners[(i+4)%numCorners];
				Corner& n3=corners[(i+5)%numCorners];
				Corner& t3=corners[(i+6)%numCorners];
				if(t0.cornerType==1&&
				   n1.cornerType==-1&&t1.cornerType==1&&
				   n2.cornerType==-1&&t2.cornerType==1&&
				   n3.cornerType==-1&&t3.cornerType==1)
					{
					/* Construye un modelo de mano: */
					Point2 tp0(float(t0.x)+0.5f,float(t0.y)+0.5f);
					Point2 np1(float(n1.x)+0.5f,float(n1.y)+0.5f);
					Point2 tp1(float(t1.x)+0.5f,float(t1.y)+0.5f);
					Point2 np2(float(n2.x)+0.5f,float(n2.y)+0.5f);
					Point2 tp2(float(t2.x)+0.5f,float(t2.y)+0.5f);
					Point2 np3(float(n3.x)+0.5f,float(n3.y)+0.5f);
					Point2 tp3(float(t3.x)+0.5f,float(t3.y)+0.5f);
					
					/* Calcule el rango de distancias de la punta del dedo: */
					Interval tipDistance(Geometry::dist(tp0,tp1));
					tipDistance.addValue(Geometry::dist(tp1,tp2));
					tipDistance.addValue(Geometry::dist(tp2,tp3));
					
					/* Calcule el rango de distancias de los rincones de los dedos: */
					Interval nookDistance(Geometry::dist(np1,np2));
					nookDistance.addValue(Geometry::dist(np2,np3));
					
					/* Calcular los puntos de la raíz del dedo: */
					Vector2 curve=Geometry::mid(np1,np3)-np2;
					Point2 rp0=np1+(np1-np2)*0.5f+curve;
					Point2 rp1=Geometry::mid(np1,np2);
					Point2 rp2=Geometry::mid(np2,np3);
					Point2 rp3=np3+(np3-np2)*0.5f+curve;
					
					/* Calcule el rango de longitudes de los dedos: */
					Interval fingerLength(Geometry::dist(tp0,rp0));
					fingerLength.addValue(Geometry::dist(tp1,rp1));
					fingerLength.addValue(Geometry::dist(tp2,rp2));
					fingerLength.addValue(Geometry::dist(tp3,rp3));
					
					/* Calcule la probabilidad de que esta sea una mano: */
					float prob=1.0f;
					prob*=Math::sqr(tipDistance.getMin()/tipDistance.getMax());
					prob*=nookDistance.getMin()/nookDistance.getMax();
					prob*=fingerLength.getMin()/fingerLength.getMax();
					
					if(maxProb<prob)
						{
						/* Calcular la relación entre la longitud del dedo y la distancia del rincón: */
						float fdNdRatio=Math::mid(Geometry::dist(tp1,rp1),Geometry::dist(tp2,rp2))/Math::mid(Geometry::dist(np1,np2),Geometry::dist(np2,np3));
						
						/* Calcula el centro de la mano y el radio: */
						float centerOffset=1.0f/fdNdRatio;
						center=Geometry::mid(Geometry::mid(rp0+(rp0-tp0)*centerOffset,rp1+(rp1-tp1)*centerOffset),
						                     Geometry::mid(rp2+(rp2-tp2)*centerOffset,rp3+(rp3-tp3)*centerOffset));
						center=Geometry::mid(rp1+(rp1-tp1)*centerOffset,rp2+(rp2-tp2)*centerOffset);
						radius=(Geometry::dist(center,tp0)+Geometry::dist(center,tp1)+Geometry::dist(center,tp2)+Geometry::dist(center,tp3))*0.25f;
						
						/* Calcule la profundidad promedio de la mano en el espacio de imagen de profundidad con corrección de profundidad: */
						depth=0.0f;
						if(pixelDepthCorrection!=0)
							{
							ptrdiff_t t0Off=t0.y*depthFrameSize[0]+t0.x;
							depth+=pixelDepthCorrection[t0Off].correct(float(depthFrame[t0Off]));
							ptrdiff_t n1Off=n1.y*depthFrameSize[0]+n1.x;
							depth+=pixelDepthCorrection[n1Off].correct(float(depthFrame[n1Off]));
							ptrdiff_t t1Off=t1.y*depthFrameSize[0]+t1.x;
							depth+=pixelDepthCorrection[t1Off].correct(float(depthFrame[t1Off]));
							ptrdiff_t n2Off=n2.y*depthFrameSize[0]+n2.x;
							depth+=pixelDepthCorrection[n2Off].correct(float(depthFrame[n2Off]));
							ptrdiff_t t2Off=t2.y*depthFrameSize[0]+t2.x;
							depth+=pixelDepthCorrection[t2Off].correct(float(depthFrame[t2Off]));
							ptrdiff_t n3Off=n3.y*depthFrameSize[0]+n3.x;
							depth+=pixelDepthCorrection[n3Off].correct(float(depthFrame[n3Off]));
							ptrdiff_t t3Off=t3.y*depthFrameSize[0]+t3.x;
							depth+=pixelDepthCorrection[t3Off].correct(float(depthFrame[t3Off]));
							}
						else
							{
							depth+=float(depthFrame[t0.y*depthFrameSize[0]+t0.x]);
							depth+=float(depthFrame[n1.y*depthFrameSize[0]+n1.x]);
							depth+=float(depthFrame[t1.y*depthFrameSize[0]+t1.x]);
							depth+=float(depthFrame[n2.y*depthFrameSize[0]+n2.x]);
							depth+=float(depthFrame[t2.y*depthFrameSize[0]+t2.x]);
							depth+=float(depthFrame[n3.y*depthFrameSize[0]+n3.x]);
							depth+=float(depthFrame[t3.y*depthFrameSize[0]+t3.x]);
							}
						depth/=7.0f;
						
						maxProb=prob;
						
						if(imgPtr!=0)
							{
							/* Dibuja la mano: */
							drawLine(*blobImage,tp0,rp0,Images::RGBImage::Color(255,255,255));
							drawLine(*blobImage,tp1,rp1,Images::RGBImage::Color(255,255,255));
							drawLine(*blobImage,tp2,rp2,Images::RGBImage::Color(255,255,255));
							drawLine(*blobImage,tp3,rp3,Images::RGBImage::Color(255,255,255));
							drawCircle(*blobImage,center,radius,Images::RGBImage::Color(255,255,255));
							}
						}
					}
				}
			}
		
		/* Comprueba si la gota coincide con una mano: */
		if(maxProb>minHandProbability)
			{
			// DEBUGGING
			// std::cout<<"Hand in depth space: "<<center[0]<<", "<<center[1]<<", "<<depth<<", "<<radius<<std::endl;
			
			/* Guarde la mano en el espacio de la cámara: */
			Hand newHand;
			newHand.center=depthProjection.transform(Point(center[0],center[1],depth));
			newHand.radius=Geometry::dist(newHand.center,depthProjection.transform(Point(center[0]+radius,center[1],depth)));
			hands.push_back(newHand);
			
			// DEBUGGING
			// std::cout<<"Hand in camera space: "<<newHand.center[0]<<", "<<newHand.center[1]<<", "<<newHand.center[2]<<", "<<newHand.radius<<std::endl;
			}
		
		/* Limpiar: */
		corners.clear();
		}
	
	/* Limpiar: */
	delete[] blobOrigins;
	}

void HandExtractor::setHandsExtractedFunction(HandExtractor::HandsExtractedFunction* newHandsExtractedFunction)
	{
	delete handsExtractedFunction;
	handsExtractedFunction=newHandsExtractedFunction;
	}

void HandExtractor::receiveRawFrame(const Kinect::FrameBuffer& newFrame)
	{
	Threads::MutexCond::Lock inputLock(inputCond);
	
	/* Almacene el nuevo búfer en el búfer de entrada: */
	inputFrame=newFrame;
	++inputFrameVersion;
	
	/* Señale el hilo de fondo: */
	inputCond.signal();
	}
