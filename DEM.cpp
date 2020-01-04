/***********************************************************************
DEM: clase para representar modelos digitales de elevación (DEM) como 
objetos de textura con valor flotante.
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

#include "DEM.h"

#include <IO/File.h>
#include <IO/OpenFile.h>
#include <GL/gl.h>
#include <GL/GLContextData.h>
#include <GL/Extensions/GLARBTextureFloat.h>
#include <GL/Extensions/GLARBTextureRectangle.h>
#include <GL/Extensions/GLARBTextureRg.h>
#include <GL/Extensions/GLARBShaderObjects.h>
#include <Geometry/Matrix.h>

/******************************
Methods of class DEM::DataItem:
******************************/

DEM::DataItem::DataItem(void)
	:textureObjectId(0)
	{
	/* Verifique e inicialice todas las extensiones OpenGL requeridas: */
	GLARBTextureFloat::initExtension();
	GLARBTextureRectangle::initExtension();
	GLARBTextureRg::initExtension();
	GLARBShaderObjects::initExtension();
	
	/* Crea el objeto de textura: */
	glGenTextures(1,&textureObjectId);
	}

DEM::DataItem::~DataItem(void)
	{
	/* Destruye el objeto de textura: */
	glDeleteTextures(1,&textureObjectId);
	}

/********************
Methods of class DEM:
********************/

void DEM::calcMatrix(void)
	{
	/* Convertir la transformación DEM en una matriz de transformación proyectiva: */
	demTransform=PTransform(transform);
	PTransform::Matrix& dtm=demTransform.getMatrix();
	
	/* Multiplica previamente la matriz de transformación proyectiva con el espacio DEM a la transformación de espacio de píxeles DEM: */
	PTransform dem;
	dem.getMatrix()(0,0)=Scalar(demSize[0]-1)/(demBox[2]-demBox[0]);
	dem.getMatrix()(0,3)=Scalar(0.5)-Scalar(demSize[0]-1)/(demBox[2]-demBox[0])*demBox[0];
	dem.getMatrix()(1,1)=Scalar(demSize[1]-1)/(demBox[3]-demBox[1]);
	dem.getMatrix()(1,3)=Scalar(0.5)-Scalar(demSize[1]-1)/(demBox[3]-demBox[1])*demBox[1];
	dem.getMatrix()(2,2)=Scalar(1)/verticalScale;
	dem.getMatrix()(2,3)=verticalScaleBase-verticalScaleBase/verticalScale;
	demTransform.leftMultiply(dem);
	
	/* Convierta la transformación completa al formato OpenGL principal de la columna: */
	GLfloat* dtmPtr=demTransformMatrix;
	for(int j=0;j<4;++j)
		for(int i=0;i<4;++i,++dtmPtr)
			*dtmPtr=GLfloat(dtm(i,j));
	}

DEM::DEM(void)
	:dem(0),
	 transform(OGTransform::identity),
	 verticalScale(1),verticalScaleBase(0)
	{
	demSize[0]=demSize[1]=0;
	}

DEM::~DEM(void)
	{
	delete[] dem;
	}

void DEM::initContext(GLContextData& contextData) const
	{
	/* Crea y registra un elemento de datos: */
	DataItem* dataItem=new DataItem;
	contextData.addDataItem(this,dataItem);
	
	/* Cargue la matriz DEM en el objeto de textura: */
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->textureObjectId);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_S,GL_CLAMP);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_T,GL_CLAMP);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,0,GL_LUMINANCE32F_ARB,demSize[0],demSize[1],0,GL_LUMINANCE,GL_FLOAT,dem);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
	}

void DEM::load(const char* demFileName)
	{
	/* Lea el archivo DEM: */
	IO::FilePtr demFile=IO::openFile(demFileName);
	demFile->setEndianness(Misc::LittleEndian);
	demFile->read<int>(demSize,2);
	dem=new float[demSize[1]*demSize[0]];
	for(int i=0;i<4;++i)
		demBox[i]=double(demFile->read<float>());
	demFile->read<float>(dem,demSize[1]*demSize[0]);
	
	/* Actualice la transformación DEM: */
	calcMatrix();
	}

float DEM::calcAverageElevation(void) const
	{
	/* Suma todas las medidas de elevación: */
	double elevSum=0.0;
	const float* demPtr=dem;
	for(int i=demSize[1]*demSize[0];i>0;--i,++demPtr)
		elevSum+=double(*demPtr);
	
	/* Devolver la elevación media: */
	return float(elevSum/double(demSize[1]*demSize[0]));
	}

void DEM::setTransform(const OGTransform& newTransform,Scalar newVerticalScale,Scalar newVerticalScaleBase)
	{
	transform=newTransform;
	verticalScale=newVerticalScale;
	verticalScaleBase=newVerticalScaleBase;
	
	/* Actualice la transformación DEM: */
	calcMatrix();
	}

void DEM::bindTexture(GLContextData& contextData) const
	{
	/* Obtener el elemento de datos de contexto: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Vincula la textura DEM: */
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->textureObjectId);
	}

void DEM::uploadDemTransform(GLint location) const
	{
	/* Sube la matriz a OpenGL: */
	glUniformMatrix4fvARB(location,1,GL_FALSE,demTransformMatrix);
	}
