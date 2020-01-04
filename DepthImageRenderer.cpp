/***********************************************************************
DepthImageRenderer: Clase para centralizar el almacenamiento de imágenes 
en profundidad sin procesar o filtradas en la GPU, y realizar tareas 
simples de representación repetitiva, como la representación de valores 
de elevación en un búfer de cuadros.
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

#include "DepthImageRenderer.h"

#include <GL/gl.h>
#include <GL/GLVertexArrayParts.h>
#include <GL/GLContextData.h>
#include <GL/Extensions/GLARBFragmentShader.h>
#include <GL/Extensions/GLARBMultitexture.h>
#include <GL/Extensions/GLARBShaderObjects.h>
#include <GL/Extensions/GLARBTextureFloat.h>
#include <GL/Extensions/GLARBTextureRectangle.h>
#include <GL/Extensions/GLARBTextureRg.h>
#include <GL/Extensions/GLARBVertexBufferObject.h>
#include <GL/Extensions/GLARBVertexShader.h>
#include <GL/GLTransformationWrappers.h>

#include "ShaderHelper.h"

// DEBUGGING
#include <iostream>

/*********************************************
Methods of class DepthImageRenderer::DataItem:
*********************************************/

DepthImageRenderer::DataItem::DataItem(void)
	:vertexBuffer(0),indexBuffer(0),
	 depthTexture(0),depthTextureVersion(0),
	 depthShader(0),elevationShader(0)
	{
	/* Inicialice todas las extensiones requeridas: */
	GLARBFragmentShader::initExtension();
	GLARBMultitexture::initExtension();
	GLARBShaderObjects::initExtension();
	GLARBTextureFloat::initExtension();
	GLARBTextureRectangle::initExtension();
	GLARBTextureRg::initExtension();
	GLARBVertexBufferObject::initExtension();
	GLARBVertexShader::initExtension();
	
	/* Asignar los buffers y texturas: */
	glGenBuffersARB(1,&vertexBuffer);
	glGenBuffersARB(1,&indexBuffer);
	glGenTextures(1,&depthTexture);
	}

DepthImageRenderer::DataItem::~DataItem(void)
	{
	/* Libere todos los búferes, texturas y sombreadores asignados: */
	glDeleteBuffersARB(1,&vertexBuffer);
	glDeleteBuffersARB(1,&indexBuffer);
	glDeleteTextures(1,&depthTexture);
	glDeleteObjectARB(depthShader);
	glDeleteObjectARB(elevationShader);
	}

/***********************************
Methods of class DepthImageRenderer:
***********************************/

DepthImageRenderer::DepthImageRenderer(const unsigned int sDepthImageSize[2])
	:depthImageVersion(0)
	{
	std::cout<<"12: DepthImageRenderer "<< std::endl;
	/* Copia el tamaño de la imagen de profundidad: */
	for(int i=0;i<2;++i)
	{
		depthImageSize[i]=sDepthImageSize[i];// 640 x 480
	}
	
	/* Inicialice la imagen de profundidad: */
	depthImage=Kinect::FrameBuffer(depthImageSize[0],depthImageSize[1],depthImageSize[1]*depthImageSize[0]*sizeof(float));
	float* diPtr=depthImage.getData<float>();
	for(unsigned int y=0;y<depthImageSize[1];++y)
		for(unsigned int x=0;x<depthImageSize[0];++x,++diPtr)
			*diPtr=0.0f;
	++depthImageVersion;
	}

void DepthImageRenderer::initContext(GLContextData& contextData) const
	{
	/* Cree un elemento de datos y agréguelo al contexto: */
	DataItem* dataItem=new DataItem;
	contextData.addDataItem(this,dataItem);
	
	/* Sube la cuadrícula de vértices de plantilla en el búfer de vértice: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,dataItem->vertexBuffer);
	glBufferDataARB(GL_ARRAY_BUFFER_ARB,depthImageSize[1]*depthImageSize[0]*sizeof(Vertex),0,GL_STATIC_DRAW_ARB);
	Vertex* vPtr=static_cast<Vertex*>(glMapBufferARB(GL_ARRAY_BUFFER_ARB,GL_WRITE_ONLY_ARB));
	if(lensDistortion.isIdentity())
		{
		/* Crear posiciones de píxel no corregidas: */
		for(unsigned int y=0;y<depthImageSize[1];++y)
			for(unsigned int x=0;x<depthImageSize[0];++x,++vPtr)
				{
				vPtr->position[0]=Scalar(x)+Scalar(0.5);
				vPtr->position[1]=Scalar(y)+Scalar(0.5);
				}
		}
	else
		{
		/* Crear posiciones de píxel corregidas por distorsión de la lente: */
		for(unsigned int y=0;y<depthImageSize[1];++y)
			for(unsigned int x=0;x<depthImageSize[0];++x,++vPtr)
				{
				/* Desvincule el punto de la imagen: */
				Kinect::LensDistortion::Point dp(Kinect::LensDistortion::Scalar(x)+Kinect::LensDistortion::Scalar(0.5),Kinect::LensDistortion::Scalar(y)+Kinect::LensDistortion::Scalar(0.5));
				Kinect::LensDistortion::Point up=lensDistortion.undistortPixel(dp);
				
				/* Almacenar el punto no distorsionado: */
				vPtr->position[0]=Scalar(up[0]);
				vPtr->position[1]=Scalar(up[1]);
				}
		}
	glUnmapBufferARB(GL_ARRAY_BUFFER_ARB);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,0);
	
	/* Sube los índices del triángulo de la superficie en el búfer de índice: */
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,dataItem->indexBuffer);
	glBufferDataARB(GL_ELEMENT_ARRAY_BUFFER_ARB,(depthImageSize[1]-1)*depthImageSize[0]*2*sizeof(GLuint),0,GL_STATIC_DRAW_ARB);
	GLuint* iPtr=static_cast<GLuint*>(glMapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,GL_WRITE_ONLY_ARB));
	for(unsigned int y=1;y<depthImageSize[1];++y)
		for(unsigned int x=0;x<depthImageSize[0];++x,iPtr+=2)
			{
			iPtr[0]=GLuint(y*depthImageSize[0]+x);
			iPtr[1]=GLuint((y-1)*depthImageSize[0]+x);
			}
	glUnmapBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);
	
	/* Inicialice la textura de la imagen de profundidad: */
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->depthTexture);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_S,GL_CLAMP);
	glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_T,GL_CLAMP);
	glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,0,GL_LUMINANCE32F_ARB,depthImageSize[0],depthImageSize[1],0,GL_LUMINANCE,GL_FLOAT,0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
	
	/* Crea el shader de renderizado en profundidad: */
	dataItem->depthShader=linkVertexAndFragmentShader("SurfaceDepthShader");
	dataItem->depthShaderUniforms[0]=glGetUniformLocationARB(dataItem->depthShader,"depthSampler");
	dataItem->depthShaderUniforms[1]=glGetUniformLocationARB(dataItem->depthShader,"projectionModelviewDepthProjection");
	
	/* Crea el shader de renderizado de elevación: */
	dataItem->elevationShader=linkVertexAndFragmentShader("SurfaceElevationShader");
	dataItem->elevationShaderUniforms[0]=glGetUniformLocationARB(dataItem->elevationShader,"depthSampler");
	dataItem->elevationShaderUniforms[1]=glGetUniformLocationARB(dataItem->elevationShader,"basePlaneDic");
	dataItem->elevationShaderUniforms[2]=glGetUniformLocationARB(dataItem->elevationShader,"weightDic");
	dataItem->elevationShaderUniforms[3]=glGetUniformLocationARB(dataItem->elevationShader,"projectionModelviewDepthProjection");
	}

void DepthImageRenderer::setDepthProjection(const PTransform& newDepthProjection)
	{
	/* Establecer la matriz de proyección de profundidad: */
	depthProjection=newDepthProjection;
	
	/* Convierta la matriz de proyección de profundidad al formato OpenGL principal de la columna: */
	GLfloat* dpmPtr=depthProjectionMatrix;
	for(int j=0;j<4;++j)
		for(int i=0;i<4;++i,++dpmPtr)
			*dpmPtr=GLfloat(depthProjection.getMatrix()(i,j));
	
	/* Crear la ecuación de cálculo de peso: */
	for(int i=0;i<4;++i)
		weightDicEq[i]=GLfloat(depthProjection.getMatrix()(3,i));
	
	/* Recalcular la ecuación del plano base en el espacio de la imagen de profundidad: */
	setBasePlane(basePlane);
	}

void DepthImageRenderer::setIntrinsics(const Kinect::FrameSource::IntrinsicParameters& ips)
	{
	std::cout<<"12.1: SetIntrinsics "<<  std::endl;
	/* Ajuste los parámetros de distorsión de la lente: */
	lensDistortion=ips.depthLensDistortion;
	
	/* Establecer la matriz de proyección de profundidad: */
	depthProjection=ips.depthProjection;
	
	/* Convierta la matriz de proyección de profundidad al formato OpenGL principal de la columna: */
	GLfloat* dpmPtr=depthProjectionMatrix;
	for(int j=0;j<4;++j)
		for(int i=0;i<4;++i,++dpmPtr)
			*dpmPtr=GLfloat(depthProjection.getMatrix()(i,j));
	
	/* Crear la ecuación de cálculo de peso: */
	for(int i=0;i<4;++i)
		weightDicEq[i]=GLfloat(depthProjection.getMatrix()(3,i));
	
	/* Recalcular la ecuación del plano base en el espacio de la imagen de profundidad: */
	DepthImageRenderer::setBasePlane(basePlane);
	}

void DepthImageRenderer::setBasePlane(const Plane& newBasePlane)
	{
	std::cout<<"12.~: SetBasePlane "<<  std::endl;
	/* Establecer el plano base: */
	basePlane=newBasePlane;
	
	/* Transforme el plano base en espacio de imagen de profundidad y en un formato compatible con GLSL: */
	const PTransform::Matrix& dpm=depthProjection.getMatrix();
	const Plane::Vector& bpn=basePlane.getNormal();
	Scalar bpo=basePlane.getOffset();
	for(int i=0;i<4;++i)
	{
		basePlaneDicEq[i]=GLfloat(dpm(0,i)*bpn[0]+dpm(1,i)*bpn[1]+dpm(2,i)*bpn[2]-dpm(3,i)*bpo);
		//std::cout<<"basePlaneDicEq "<< basePlaneDicEq[i] << std::endl;
	}
	}

void DepthImageRenderer::setDepthImage(const Kinect::FrameBuffer& newDepthImage)
	{
	/* Actualizar la imagen de profundidad: */
	depthImage=newDepthImage;
	++depthImageVersion;
	}

Scalar DepthImageRenderer::intersectLine(const Point& p0,const Point& p1,Scalar elevationMin,Scalar elevationMax) const
	{
	/* Inicializar el segmento de línea: */
	Scalar lambda0=Scalar(0);
	Scalar lambda1=Scalar(1);
	
	/* Intersecta el segmento de línea con el plano de elevación superior: */
	Scalar d0=basePlane.calcDistance(p0);
	Scalar d1=basePlane.calcDistance(p1);
	if(d0*d1<Scalar(0))
		{
		/* Calcula el parámetro de intersección: */
		
		// IMPLEMENT ME!
		
		return Scalar(2);
		}
	else if(d1>Scalar(0))
		{
		/* Rechazo trivial con máxima intercepción: */
		return Scalar(2);
		}
	
	return Scalar(2);
	}

void DepthImageRenderer::uploadDepthProjection(GLint location) const
	{
	/* Sube la matriz a OpenGL: */
	glUniformMatrix4fvARB(location,1,GL_FALSE,depthProjectionMatrix);
	}

void DepthImageRenderer::bindDepthTexture(GLContextData& contextData) const
	{
	/* Obtener el elemento de datos: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Enlazar la textura de la imagen de profundidad: */
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->depthTexture);
	
	/* Compruebe si la textura está desactualizada: */
	if(dataItem->depthTextureVersion!=depthImageVersion)
		{
		/* Sube la nueva textura de profundidad: */
		glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB,0,0,0,depthImageSize[0],depthImageSize[1],GL_LUMINANCE,GL_FLOAT,depthImage.getData<GLfloat>());
		
		/* Marque la textura de profundidad como actual: */
		dataItem->depthTextureVersion=depthImageVersion;
		}
	}

void DepthImageRenderer::renderSurfaceTemplate(GLContextData& contextData) const
	{
	/* Obtener el elemento de datos: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Enlazar los buffers de vértice e índice: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,dataItem->vertexBuffer);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,dataItem->indexBuffer);
	
	/* Dibuje la plantilla de superficie: */
	GLVertexArrayParts::enable(Vertex::getPartsMask());
	glVertexPointer(static_cast<const Vertex*>(0));
	GLuint* indexPtr=0;
	for(unsigned int y=1;y<depthImageSize[1];++y,indexPtr+=depthImageSize[0]*2)
		glDrawElements(GL_QUAD_STRIP,depthImageSize[0]*2,GL_UNSIGNED_INT,indexPtr);
	GLVertexArrayParts::disable(Vertex::getPartsMask());
	
	/* Desenlazar los buffers de vértice e índice: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,0);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);
	}

void DepthImageRenderer::renderDepth(const PTransform& projectionModelview,GLContextData& contextData) const
	{
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Obtener el elemento de datos: */
	glUseProgramObjectARB(dataItem->depthShader);
	
	/* Enlazar los buffers de vértice e índice: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,dataItem->vertexBuffer);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,dataItem->indexBuffer);
	
	/* Enlazar la textura de la imagen de profundidad: */
	glActiveTextureARB(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->depthTexture);
	
	/* Compruebe si la textura está desactualizada: */
	if(dataItem->depthTextureVersion!=depthImageVersion)
		{
		/* Sube la nueva textura de profundidad: */
		glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB,0,0,0,depthImageSize[0],depthImageSize[1],GL_LUMINANCE,GL_FLOAT,depthImage.getData<GLfloat>());
		
		/* Marque la textura de profundidad como actual: */
		dataItem->depthTextureVersion=depthImageVersion;
		}
	glUniform1iARB(dataItem->depthShaderUniforms[0],0); // Tell the shader that the depth texture is in texture unit 0
	
	/* Cargue la matriz combinada de proyección, vista de modelo y proyección de profundidad: */
	PTransform pmvdp=projectionModelview;
	pmvdp*=depthProjection;
	glUniformARB(dataItem->depthShaderUniforms[1],pmvdp);
	
	/* Dibuja la superficie: */
	GLVertexArrayParts::enable(Vertex::getPartsMask());
	glVertexPointer(static_cast<const Vertex*>(0));
	GLuint* indexPtr=0;
	for(unsigned int y=1;y<depthImageSize[1];++y,indexPtr+=depthImageSize[0]*2)
		glDrawElements(GL_QUAD_STRIP,depthImageSize[0]*2,GL_UNSIGNED_INT,indexPtr);
	GLVertexArrayParts::disable(Vertex::getPartsMask());
	
	/* Desenlazar todas las texturas y buffers: */
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,0);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);
	
	/* Desenlazar el sombreado de representación de profundidad: */
	glUseProgramObjectARB(0);
	}

void DepthImageRenderer::renderElevation(const PTransform& projectionModelview,GLContextData& contextData) const
	{
	/* Obtener el elemento de datos: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Enlazar el sombreado de representación de elevación: */
	glUseProgramObjectARB(dataItem->elevationShader);
	
	/* Configurar la textura de la imagen de profundidad: */
	glActiveTextureARB(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->depthTexture);
	
	/* Compruebe si la textura está desactualizada: */
	if(dataItem->depthTextureVersion!=depthImageVersion)
		{
		/* Sube la nueva textura de profundidad: */
		glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB,0,0,0,depthImageSize[0],depthImageSize[1],GL_LUMINANCE,GL_FLOAT,depthImage.getData<GLfloat>());
		
		/* Marque la textura de profundidad como actual: */
		dataItem->depthTextureVersion=depthImageVersion;
		}
	glUniform1iARB(dataItem->elevationShaderUniforms[0],0); // Tell the shader that the depth texture is in texture unit 0
	
	/* Sube la ecuación del plano base en el espacio de la imagen en profundidad: */
	glUniformARB<4>(dataItem->elevationShaderUniforms[1],1,basePlaneDicEq);
	
	/* Sube la ecuación de peso base en el espacio de imagen en profundidad: */
	glUniformARB<4>(dataItem->elevationShaderUniforms[2],1,weightDicEq);
	
	/* Cargue la matriz combinada de proyección, vista de modelo y proyección de profundidad: */
	PTransform pmvdp=projectionModelview;
	pmvdp*=depthProjection;
	glUniformARB(dataItem->elevationShaderUniforms[3],pmvdp);
	
	/* Enlazar los buffers de vértice e índice: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,dataItem->vertexBuffer);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,dataItem->indexBuffer);
	
	/* Dibuja la superficie: */
	GLVertexArrayParts::enable(Vertex::getPartsMask());
	glVertexPointer(static_cast<const Vertex*>(0));
	GLuint* indexPtr=0;
	for(unsigned int y=1;y<depthImageSize[1];++y,indexPtr+=depthImageSize[0]*2)
		glDrawElements(GL_QUAD_STRIP,depthImageSize[0]*2,GL_UNSIGNED_INT,indexPtr);
	GLVertexArrayParts::disable(Vertex::getPartsMask());
	
	/* Desenlazar todas las texturas y buffers: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,0);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
	
	/* Desenlazar el sombreado de representación de elevación: */
	glUseProgramObjectARB(0);
	}
