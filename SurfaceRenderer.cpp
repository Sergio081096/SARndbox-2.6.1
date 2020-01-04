/***********************************************************************
SurfaceRenderer: Clase para representar una superficie definida por una 
cuadrícula regular en el espacio de imagen en profundidad.
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

#include "SurfaceRenderer.h"

#include <string>
#include <vector>
#include <iostream>
#include <Misc/PrintInteger.h>
#include <Misc/ThrowStdErr.h>
#include <Misc/MessageLogger.h>
#include <GL/gl.h>
#include <GL/GLVertexArrayParts.h>
#include <GL/Extensions/GLARBFragmentShader.h>
#include <GL/Extensions/GLARBMultitexture.h>
#include <GL/Extensions/GLARBShaderObjects.h>
#include <GL/Extensions/GLARBTextureFloat.h>
#include <GL/Extensions/GLARBTextureRectangle.h>
#include <GL/Extensions/GLARBTextureRg.h>
#include <GL/Extensions/GLARBVertexShader.h>
#include <GL/Extensions/GLEXTFramebufferObject.h>
#include <GL/GLLightTracker.h>
#include <GL/GLContextData.h>
#include <GL/GLTransformationWrappers.h>
#include <GL/GLGeometryVertex.h>

#include "DepthImageRenderer.h"
#include "ElevationColorMap.h"
#include "DEM.h"
#include "WaterTable2.h"
#include "ShaderHelper.h"
#include "Config.h"

/******************************************
Methods of class SurfaceRenderer::DataItem:
******************************************/

SurfaceRenderer::DataItem::DataItem(void)
	:contourLineFramebufferObject(0),
	contourLineDepthBufferObject(0),
	contourLineColorTextureObject(0),
	contourLineVersion(0),
	heightMapShader(0),
	surfaceSettingsVersion(0),
	lightTrackerVersion(0),
	globalAmbientHeightMapShader(0),
	shadowedIlluminatedHeightMapShader(0)
	{
	/* Inicialice todas las extensiones requeridas: */
	GLARBFragmentShader::initExtension();
	GLARBMultitexture::initExtension();
	GLARBShaderObjects::initExtension();
	GLARBTextureFloat::initExtension();
	GLARBTextureRectangle::initExtension();
	GLARBTextureRg::initExtension();
	GLARBVertexShader::initExtension();
	GLEXTFramebufferObject::initExtension();
	}

SurfaceRenderer::DataItem::~DataItem(void)
	{
	/* Libere todos los búferes, texturas y sombreadores asignados: */
	glDeleteFramebuffersEXT(1,&contourLineFramebufferObject);
	glDeleteRenderbuffersEXT(1,&contourLineDepthBufferObject);
	glDeleteTextures(1,&contourLineColorTextureObject);
	glDeleteObjectARB(heightMapShader);
	glDeleteObjectARB(globalAmbientHeightMapShader);
	glDeleteObjectARB(shadowedIlluminatedHeightMapShader);
	}

/********************************
Methods of class SurfaceRenderer:
********************************/

void SurfaceRenderer::shaderSourceFileChanged(const IO::FileMonitor::Event& event)
	{
	std::cout<<"16.~: ShaderSourceFileChanged" << std::endl;
	/* Invalide el sombreador de superficie de un solo paso: */
	++surfaceSettingsVersion;
	}

GLhandleARB SurfaceRenderer::createSinglePassSurfaceShader(const GLLightTracker& lt,GLint* uniformLocations) const
	{
	GLhandleARB result=0;
	
	std::vector<GLhandleARB> shaders;
	try
		{
		/****************************************************************************
		Ensamble y compile el sombreado de vértices de representación de superficie:
		****************************************************************************/
		
		/* Ensamble las cadenas de función y declaración: */
		std::string vertexFunctions="\
			#extension GL_ARB_texture_rectangle : enable\n";
		
		std::string vertexUniforms="\
			uniform sampler2DRect depthSampler; // Sampler for the depth image-space elevation texture\n\
			uniform mat4 depthProjection; // Transformation from depth image space to camera space\n\
			uniform mat4 projectionModelviewDepthProjection; // Transformation from depth image space to clip space\n";
		
		std::string vertexVaryings;
		
		/* Ensamble la función principal de sombreadores de vértices: */
		std::string vertexMain="\
			void main()\n\
				{\n\
				/* Get the vertex' depth image-space z coordinate from the texture: */\n\
				vec4 vertexDic=gl_Vertex;\n\
				vertexDic.z=texture2DRect(depthSampler,gl_Vertex.xy).r;\n\
				\n\
				/* Transform the vertex from depth image space to camera space and normalize it: */\n\
				vec4 vertexCc=depthProjection*vertexDic;\n\
				vertexCc/=vertexCc.w;\n\
				\n";
		
		if(dem!=0)
			{
			/* Agregue el código coincidente de DEM a la función principal del sombreador de vértice: */
			vertexUniforms+="\
				uniform mat4 demTransform; // Transformation from camera space to DEM space\n\
				uniform sampler2DRect demSampler; // Sampler for the DEM texture\n\
				uniform float demDistScale; // Distance from surface to DEM at which the color map saturates\n";
			
			vertexVaryings+="\
				varying float demDist; // Scaled signed distance from surface to DEM\n";
			
			/* Agregue el código coincidente de DEM a la función principal de sombreadores de vértices: */
			vertexMain+="\
				/* Transform the camera-space vertex to scaled DEM space: */\n\
				vec4 vertexDem=demTransform*vertexCc;\n\
				\n\
				/* Calculate scaled DEM-surface distance: */\n\
				demDist=(vertexDem.z-texture2DRect(demSampler,vertexDem.xy).r)*demDistScale;\n\
				\n";
			}
		else
			{
			if(elevationColorMap!=0)
				{
				/* Agregue declaraciones para el mapeo de altura: */
				vertexUniforms+="\
					uniform vec4 heightColorMapPlaneEq; // Plane equation of the base plane in camera space, scaled for height map textures\n";
				
				vertexVaryings+="\
					varying float heightColorMapTexCoord; // Texture coordinate for the height color map\n";
				
				/* Agregue el código de asignación de altura a la función principal del sombreador de vértice: */
				vertexMain+="\
					/* Plug camera-space vertex into the scaled and offset base plane equation: */\n\
					heightColorMapTexCoord=dot(heightColorMapPlaneEq,vertexCc);\n\
					\n";
				}
			
			if(drawDippingBed)
				{
				/* Agregue las declaraciones para la representación de la cama de inmersión: */
				if(dippingBedFolded)
					{
					vertexUniforms+="\
						uniform float dbc[5]; // Dipping bed coefficients\n";
					}
				else
					{
					vertexUniforms+="\
						uniform vec4 dippingBedPlaneEq; // Plane equation of the dipping bed\n";
					}
				
				vertexVaryings+="\
					varying float dippingBedDistance; // Vertex distance to dipping bed\n";
				
				/* Agregue el código de la cama de inmersión a la función principal del sombreador de vértice: */
				if(dippingBedFolded)
					{
					vertexMain+="\
						/* Calculate distance from camera-space vertex to dipping bed equation: */\n\
						dippingBedDistance=vertexCc.z-(((1.0-dbc[3])+cos(dbc[0]*vertexCc.x)*dbc[3])*sin(dbc[1]*vertexCc.y)*dbc[2]+dbc[4]);\n\
						\n";
					}
				else
					{
					vertexMain+="\
						/* Plug camera-space vertex into the dipping bed equation: */\n\
						dippingBedDistance=dot(dippingBedPlaneEq,vertexCc);\n\
						\n";
					}
				}
			}
		
		if(illuminate)
			{
			/* Añadir declaraciones para la iluminación: */
			vertexUniforms+="\
				uniform mat4 modelview; // Transformation from camera space to eye space\n\
				uniform mat4 tangentModelviewDepthProjection; // Transformation from depth image space to eye space for tangent planes\n";
			
			vertexVaryings+="\
				varying vec4 diffColor,specColor; // Diffuse and specular colors, interpolated separately for correct highlights\n";
			
			/* Agregue el código de iluminación a la función principal del sombreador de vértice: */
			vertexMain+="\
				/* Calculate the vertex' tangent plane equation in depth image space: */\n\
				vec4 tangentDic;\n\
				tangentDic.x=texture2DRect(depthSampler,vec2(vertexDic.x-1.0,vertexDic.y)).r-texture2DRect(depthSampler,vec2(vertexDic.x+1.0,vertexDic.y)).r;\n\
				tangentDic.y=texture2DRect(depthSampler,vec2(vertexDic.x,vertexDic.y-1.0)).r-texture2DRect(depthSampler,vec2(vertexDic.x,vertexDic.y+1.0)).r;\n\
				tangentDic.z=2.0;\n\
				tangentDic.w=-dot(vertexDic.xyz,tangentDic.xyz)/vertexDic.w;\n\
				\n\
				/* Transform the vertex and its tangent plane from depth image space to eye space: */\n\
				vec4 vertexEc=modelview*vertexCc;\n\
				vec3 normalEc=normalize((tangentModelviewDepthProjection*tangentDic).xyz);\n\
				\n\
				/* Initialize the color accumulators: */\n\
				diffColor=gl_LightModel.ambient*gl_FrontMaterial.ambient;\n\
				specColor=vec4(0.0,0.0,0.0,0.0);\n\
				\n";
			
			/* Llame a la función de acumulación de luz adecuada para cada fuente de luz habilitada: */
			bool firstLight=true;
			for(int lightIndex=0;lightIndex<lt.getMaxNumLights();++lightIndex)
				if(lt.getLightState(lightIndex).isEnabled())
					{
					/* Crear la función de acumulación de luz: */
					vertexFunctions.push_back('\n');
					vertexFunctions+=lt.createAccumulateLightFunction(lightIndex);
					
					if(firstLight)
						{
						vertexMain+="\
							/* Call the light accumulation functions for all enabled light sources: */\n";
						firstLight=false;
						}
					
					/* Llame a la función de acumulación de luz desde la función principal del sombreador de vértice: */
					vertexMain+="\
						accumulateLight";
					char liBuffer[12];
					vertexMain.append(Misc::print(lightIndex,liBuffer+11));
					vertexMain+="(vertexEc,normalEc,gl_FrontMaterial.ambient,gl_FrontMaterial.diffuse,gl_FrontMaterial.specular,gl_FrontMaterial.shininess,diffColor,specColor);\n";
					}
			if(!firstLight)
				vertexMain+="\
					\n";
			}
		
		if(waterTable!=0&&dem==0)
			{
			/* Añadir declaraciones para el manejo del agua: */
			vertexUniforms+="\
				uniform mat4 waterTransform; // Transformation from camera space to water level texture coordinate space\n";
			vertexVaryings+="\
				varying vec2 waterTexCoord; // Texture coordinate for water level texture\n";
			
			/* Agregue el código de manejo de agua a la función principal del sombreador de vértice: */
			vertexMain+="\
				/* Transform the vertex from camera space to water level texture coordinate space: */\n\
				waterTexCoord=(waterTransform*vertexCc).xy;\n\
				\n";
			}
		
		/* Termina la función principal del sombreador de vértices: */
		vertexMain+="\
				/* Transform vertex from depth image space to clip space: */\n\
				gl_Position=projectionModelviewDepthProjection*vertexDic;\n\
				}\n";
		
		/* Compila el sombreador de vértices: */
		shaders.push_back(glCompileVertexShaderFromStrings(7,vertexFunctions.c_str(),"\t\t\n",vertexUniforms.c_str(),"\t\t\n",vertexVaryings.c_str(),"\t\t\n",vertexMain.c_str()));
		
		/*********************************************************************************
		Ensamble y compile los sombreadores de fragmentos de representación de superficie:
		*********************************************************************************/
		
		/* Ensamble las declaraciones de función del sombreador de fragmentos: */
		std::string fragmentDeclarations;
		
		/* Ensamble las diversas variables y uniformes del fragmento de sombreado: */
		std::string fragmentUniforms;
		std::string fragmentVaryings;
		
		/* Ensamble la función principal del sombreador de fragmentos: */
		std::string fragmentMain="\
			void main()\n\
				{\n";
		
		if(dem!=0)
			{
			/* Añadir declaraciones para la coincidencia de DEM: */
			fragmentVaryings+="\
				varying float demDist; // Scaled signed distance from surface to DEM\n";
			
			/* Agregue el código coincidente de DEM a la función principal del sombreador de fragmentos: */
			fragmentMain+="\
				/* Calculate the fragment's color from a double-ramp function: */\n\
				vec4 baseColor;\n\
				if(demDist<0.0)\n\
					baseColor=mix(vec4(1.0,1.0,1.0,1.0),vec4(1.0,0.0,0.0,1.0),min(-demDist,1.0));\n\
				else\n\
					baseColor=mix(vec4(1.0,1.0,1.0,1.0),vec4(0.0,0.0,1.0,1.0),min(demDist,1.0));\n\
				\n";
			}
		else
			{
			if(elevationColorMap!=0)
				{
				/* Agregue declaraciones para el mapeo de altura: */
				fragmentUniforms+="\
					uniform sampler1D heightColorMapSampler;\n";
				fragmentVaryings+="\
					varying float heightColorMapTexCoord; // Texture coordinate for the height color map\n";
				
				/* Agregue el código de asignación de altura a la función principal del sombreador de fragmentos: */
				fragmentMain+="\
					/* Get the fragment's color from the height color map: */\n\
					vec4 baseColor=texture1D(heightColorMapSampler,heightColorMapTexCoord);\n\
					\n";
				}
			else
				{
				fragmentMain+="\
					/* Set the surface's base color to white: */\n\
					vec4 baseColor=vec4(1.0,1.0,1.0,1.0);\n\
					\n";
				}
			
			if(drawDippingBed)
				{
				/* Agregue las declaraciones para la representación de la cama de inmersión: */
				fragmentUniforms+="\
					uniform float dippingBedThickness; // Thickness of dipping bed in camera-space units\n";
				
				fragmentVaryings+="\
					varying float dippingBedDistance; // Vertex distance to dipping bed plane\n";
				
				/* Agregue el código de la cama de inmersión para fragmentar la función principal del sombreador: */
				fragmentMain+="\
					/* Check fragment's dipping plane distance against dipping bed thickness: */\n\
					float w=fwidth(dippingBedDistance)*1.0;\n\
					if(dippingBedDistance<0.0)\n\
						baseColor=mix(baseColor,vec4(1.0,0.0,0.0,1.0),smoothstep(-dippingBedThickness*0.5-w,-dippingBedThickness*0.5+w,dippingBedDistance));\n\
					else\n\
						baseColor=mix(vec4(1.0,0.0,0.0,1.0),baseColor,smoothstep(dippingBedThickness*0.5-w,dippingBedThickness*0.5+w,dippingBedDistance));\n\
					\n";
				}
			}
		
		if(drawContourLines)// Importante
			{
			/* Declare the contour line function: */
			fragmentDeclarations+="\
				void addContourLines(in vec2,inout vec4);\n";
			
			/* Declara la función de la línea de contorno: */
			shaders.push_back(compileFragmentShader("SurfaceAddContourLines"));
			
			/* Llamar a la función de línea de contorno desde la función principal del fragmento shader: */
			fragmentMain+="\
				/* Modulate the base color by contour line color: */\n\
				addContourLines(gl_FragCoord.xy,baseColor);\n\
				\n";
			}
		
		if(illuminate)
			{
			/* Declara la función de iluminación: */
			fragmentDeclarations+="\
				void illuminate(inout vec4);\n";
			
			/* Compila el sombreador de iluminación: */
			shaders.push_back(compileFragmentShader("SurfaceIlluminate"));
			
			/* Función de iluminación de llamada de la función principal del fragmento shader: */
			fragmentMain+="\
				/* Apply illumination to the base color: */\n\
				illuminate(baseColor);\n\
				\n";
			}
		
		if(waterTable!=0&&dem==0)
			{
			/* Declarar las funciones de manejo de agua: */
			fragmentDeclarations+="\
				void addWaterColor(in vec2,inout vec4);\n\
				void addWaterColorAdvected(inout vec4);\n";
			
			/* Compile the water handling shader: */
			shaders.push_back(compileFragmentShader("SurfaceAddWaterColor"));
			
			/* Compilar el shader de manejo de agua: */
			if(advectWaterTexture)
				{
				fragmentMain+="\
					/* Modulate the base color with water color: */\n\
					addWaterColorAdvected(baseColor);\n\
					\n";
				}
			else
				{
				fragmentMain+="\
					/* Modulate the base color with water color: */\n\
					addWaterColor(gl_FragCoord.xy,baseColor);\n\
					\n";
				}
			}
		
		/* Termina la función principal del sombreador de fragmentos: */
		fragmentMain+="\
			/* Assign the final color to the fragment: */\n\
			gl_FragColor=baseColor;\n\
			}\n";
		
		/* Compila el fragmento shader: */
		shaders.push_back(glCompileFragmentShaderFromStrings(7,fragmentDeclarations.c_str(),"\t\t\n",fragmentUniforms.c_str(),"\t\t\n",fragmentVaryings.c_str(),"\t\t\n",fragmentMain.c_str()));
		
		/* Enlace el programa de sombreado: */
		result=glLinkShader(shaders);
		
		/* Suelte todos los sombreadores compilados: */
		for(std::vector<GLhandleARB>::iterator shIt=shaders.begin();shIt!=shaders.end();++shIt)
			glDeleteObjectARB(*shIt);
		
		/*******************************************************************
		Consulta las ubicaciones uniformes del programa de sombreado:
		*******************************************************************/
		
		GLint* ulPtr=uniformLocations;
		
		/* Consulta variables uniformes comunes: */
		*(ulPtr++)=glGetUniformLocationARB(result,"depthSampler");
		*(ulPtr++)=glGetUniformLocationARB(result,"depthProjection");
		if(dem!=0)
			{
			/* Consultar variables uniformes de DEM coincidentes: */
			*(ulPtr++)=glGetUniformLocationARB(result,"demTransform");
			*(ulPtr++)=glGetUniformLocationARB(result,"demSampler");
			*(ulPtr++)=glGetUniformLocationARB(result,"demDistScale");
			}
		else if(elevationColorMap!=0)
			{
			/* Variable de consulta de asignación de color variables uniformes: */
			*(ulPtr++)=glGetUniformLocationARB(result,"heightColorMapPlaneEq");
			*(ulPtr++)=glGetUniformLocationARB(result,"heightColorMapSampler");
			}
		if(drawContourLines)
			{
			*(ulPtr++)=glGetUniformLocationARB(result,"pixelCornerElevationSampler");
			*(ulPtr++)=glGetUniformLocationARB(result,"contourLineFactor");
			}
		if(drawDippingBed)
			{
			if(dippingBedFolded)
				*(ulPtr++)=glGetUniformLocationARB(result,"dbc");
			else
				*(ulPtr++)=glGetUniformLocationARB(result,"dippingBedPlaneEq");
			*(ulPtr++)=glGetUniformLocationARB(result,"dippingBedThickness");
			}
		if(illuminate)
			{
			/* Variables uniformes de iluminación de consulta: */
			*(ulPtr++)=glGetUniformLocationARB(result,"modelview");
			*(ulPtr++)=glGetUniformLocationARB(result,"tangentModelviewDepthProjection");
			}
		if(waterTable!=0&&dem==0)
			{
			/* Consulta de variables de manejo de agua uniforme: */
			*(ulPtr++)=glGetUniformLocationARB(result,"waterTransform");
			*(ulPtr++)=glGetUniformLocationARB(result,"bathymetrySampler");
			*(ulPtr++)=glGetUniformLocationARB(result,"quantitySampler");
			*(ulPtr++)=glGetUniformLocationARB(result,"waterCellSize");
			*(ulPtr++)=glGetUniformLocationARB(result,"waterOpacity");
			*(ulPtr++)=glGetUniformLocationARB(result,"waterAnimationTime");
			}
		*(ulPtr++)=glGetUniformLocationARB(result,"projectionModelviewDepthProjection");
		}
	catch(...)
		{
		/* Limpie y vuelva a tirar la excepción: */
		for(std::vector<GLhandleARB>::iterator shIt=shaders.begin();shIt!=shaders.end();++shIt)
			glDeleteObjectARB(*shIt);
		throw;
		}
	
	return result;
	}

void SurfaceRenderer::renderPixelCornerElevations(const int viewport[4],const PTransform& projectionModelview,GLContextData& contextData,SurfaceRenderer::DataItem* dataItem) const
	{
	/* Guarde el búfer de cuadros actualmente enlazado y borre el color: */
	GLint currentFrameBuffer;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT,&currentFrameBuffer);
	GLfloat currentClearColor[4];
	glGetFloatv(GL_COLOR_CLEAR_VALUE,currentClearColor);
	
	/* Compruebe si es necesario crear el búfer de cuadros de representación de líneas de contorno: */
	if(dataItem->contourLineFramebufferObject==0)
		{
		/* Inicialice el buffer de cuadros: */
		for(int i=0;i<2;++i)
			dataItem->contourLineFramebufferSize[i]=0;
		glGenFramebuffersEXT(1,&dataItem->contourLineFramebufferObject);
		glGenRenderbuffersEXT(1,&dataItem->contourLineDepthBufferObject);
		glGenTextures(1,&dataItem->contourLineColorTextureObject);
		}
	
	/* Enlazar el objeto de búfer de marco de representación de línea de contorno: */
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,dataItem->contourLineFramebufferObject);
	
	/* Compruebe si es necesario cambiar el tamaño del búfer de marco de la línea de contorno: */
	if(dataItem->contourLineFramebufferSize[0]!=(unsigned int)(viewport[2]+1)||dataItem->contourLineFramebufferSize[1]!=(unsigned int)(viewport[3]+1))
		{
		/* Recuerde si los búferes de representación aún deben estar adjuntos al búfer de cuadros: */
		bool mustAttachBuffers=dataItem->contourLineFramebufferSize[0]==0&&dataItem->contourLineFramebufferSize[1]==0;
		
		/* Actualice el tamaño del buffer de cuadros: */
		for(int i=0;i<2;++i)
			dataItem->contourLineFramebufferSize[i]=(unsigned int)(viewport[2+i]+1);
		
		/* Cambiar el tamaño del búfer de profundidad de representación de la línea de contorno topográfico: */
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,dataItem->contourLineDepthBufferObject);
		glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT,GL_DEPTH_COMPONENT,dataItem->contourLineFramebufferSize[0],dataItem->contourLineFramebufferSize[1]);
		glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,0);
		
		/* Cambiar el tamaño de la línea de contorno topográfico que representa la textura del color: */
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->contourLineColorTextureObject);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_S,GL_CLAMP);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_T,GL_CLAMP);
		glTexImage2D(GL_TEXTURE_RECTANGLE_ARB,0,GL_R32F,dataItem->contourLineFramebufferSize[0],dataItem->contourLineFramebufferSize[1],0,GL_LUMINANCE,GL_UNSIGNED_BYTE,0);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
		
		if(mustAttachBuffers)
			{
			glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT,GL_RENDERBUFFER_EXT,dataItem->contourLineDepthBufferObject);
			glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_RECTANGLE_ARB,dataItem->contourLineColorTextureObject,0);
			glDrawBuffer(GL_COLOR_ATTACHMENT0_EXT);
			glReadBuffer(GL_NONE);
			}
		}
	
	/* Amplíe la ventana gráfica para representar las esquinas de todos los píxeles: */
	glViewport(0,0,viewport[2]+1,viewport[3]+1);
	glClearColor(0.0f,0.0f,0.0f,1.0f);
	glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
	
	/* Desplace la matriz de proyección en medio píxel para representar las esquinas de los píxeles finales: */
	PTransform shiftedProjectionModelview=projectionModelview;
	PTransform::Matrix& spmm=shiftedProjectionModelview.getMatrix();
	Scalar xs=Scalar(viewport[2])/Scalar(viewport[2]+1);
	Scalar ys=Scalar(viewport[3])/Scalar(viewport[3]+1);
	for(int j=0;j<4;++j)
		{
		spmm(0,j)*=xs;
		spmm(1,j)*=ys;
		}
	
	/* Procese la elevación de la superficie en el búfer de fotogramas con desplazamiento de medio píxel: */
	depthImageRenderer->renderElevation(shiftedProjectionModelview,contextData);
	
	/* Restore the original viewport: */
	glViewport(viewport[0],viewport[1],viewport[2],viewport[3]);
	
	/* Restaure el color claro original y el enlace del búfer del marco: */
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,currentFrameBuffer);
	glClearColor(currentClearColor[0],currentClearColor[1],currentClearColor[2],currentClearColor[3]);
	}

SurfaceRenderer::SurfaceRenderer(const DepthImageRenderer* sDepthImageRenderer)
	:depthImageRenderer(sDepthImageRenderer),
	 drawContourLines(true),
	 contourLineFactor(1.0f),
	 elevationColorMap(0),
	 drawDippingBed(false),
	 dippingBedFolded(false),
	 dippingBedPlane(Plane::Vector(0,0,1),0.0f),
	 dippingBedThickness(1),
	 dem(0),
	 demDistScale(1.0f),
	 illuminate(false),
	 waterTable(0),
	 advectWaterTexture(false),
	 waterOpacity(2.0f),
	 surfaceSettingsVersion(1),
	 animationTime(0.0)
	{
	std::cout<<"16: SurfaceRenderer" << std::endl;
	/* Copia el tamaño de la imagen de profundidad: */
	for(int i=0;i<2;++i)
		depthImageSize[i]=depthImageRenderer->getDepthImageSize(i);
	
	/* Compruebe si la matriz de proyección de profundidad conserva la diestra: */
	const PTransform& depthProjection=depthImageRenderer->getDepthProjection();
	Point p1=depthProjection.transform(Point(0,0,0));
	Point p2=depthProjection.transform(Point(1,0,0));
	Point p3=depthProjection.transform(Point(0,1,0));
	Point p4=depthProjection.transform(Point(0,0,1));
	bool depthProjectionInverts=((p2-p1)^(p3-p1))*(p4-p1)<Scalar(0);
	
	/* Calcule la proyección de profundidad del plano tangente transpuesto: */
	tangentDepthProjection=Geometry::invert(depthProjection);
	if(depthProjectionInverts)
		tangentDepthProjection*=PTransform::scale(PTransform::Scale(-1,-1,-1));
	
	/* Supervise los archivos de origen del sombreador externo: */
	fileMonitor.addPath((std::string(CONFIG_SHADERDIR)+std::string("/SurfaceAddContourLines.fs")).c_str(),IO::FileMonitor::Modified,Misc::createFunctionCall(this,&SurfaceRenderer::shaderSourceFileChanged));
	fileMonitor.addPath((std::string(CONFIG_SHADERDIR)+std::string("/SurfaceIlluminate.fs")).c_str(),IO::FileMonitor::Modified,Misc::createFunctionCall(this,&SurfaceRenderer::shaderSourceFileChanged));
	fileMonitor.addPath((std::string(CONFIG_SHADERDIR)+std::string("/SurfaceAddWaterColor.fs")).c_str(),IO::FileMonitor::Modified,Misc::createFunctionCall(this,&SurfaceRenderer::shaderSourceFileChanged));
	fileMonitor.startPolling();
	}

void SurfaceRenderer::initContext(GLContextData& contextData) const
	{
	/* Cree un elemento de datos y agréguelo al contexto: */
	DataItem* dataItem=new DataItem;
	contextData.addDataItem(this,dataItem);
	
	/* Crear el mapa de altura render shader: */
	dataItem->heightMapShader=SurfaceRenderer::createSinglePassSurfaceShader(*contextData.getLightTracker(),dataItem->heightMapShaderUniforms);
	dataItem->surfaceSettingsVersion=surfaceSettingsVersion;
	dataItem->lightTrackerVersion=contextData.getLightTracker()->getVersion();
	
	/* Cree el sombreador de procesamiento del mapa de altura ambiente global: */
	dataItem->globalAmbientHeightMapShader=linkVertexAndFragmentShader("SurfaceGlobalAmbientHeightMapShader");
	dataItem->globalAmbientHeightMapShaderUniforms[0]=glGetUniformLocationARB(dataItem->globalAmbientHeightMapShader,"depthSampler");
	dataItem->globalAmbientHeightMapShaderUniforms[1]=glGetUniformLocationARB(dataItem->globalAmbientHeightMapShader,"depthProjection");
	dataItem->globalAmbientHeightMapShaderUniforms[2]=glGetUniformLocationARB(dataItem->globalAmbientHeightMapShader,"basePlane");
	dataItem->globalAmbientHeightMapShaderUniforms[3]=glGetUniformLocationARB(dataItem->globalAmbientHeightMapShader,"pixelCornerElevationSampler");
	dataItem->globalAmbientHeightMapShaderUniforms[4]=glGetUniformLocationARB(dataItem->globalAmbientHeightMapShader,"contourLineFactor");
	dataItem->globalAmbientHeightMapShaderUniforms[5]=glGetUniformLocationARB(dataItem->globalAmbientHeightMapShader,"heightColorMapSampler");
	dataItem->globalAmbientHeightMapShaderUniforms[6]=glGetUniformLocationARB(dataItem->globalAmbientHeightMapShader,"heightColorMapTransformation");
	dataItem->globalAmbientHeightMapShaderUniforms[7]=glGetUniformLocationARB(dataItem->globalAmbientHeightMapShader,"waterLevelSampler");
	dataItem->globalAmbientHeightMapShaderUniforms[8]=glGetUniformLocationARB(dataItem->globalAmbientHeightMapShader,"waterLevelTextureTransformation");
	dataItem->globalAmbientHeightMapShaderUniforms[9]=glGetUniformLocationARB(dataItem->globalAmbientHeightMapShader,"waterOpacity");
	
	/* Cree el mapa sombreado de altura iluminado render shader: */
	dataItem->shadowedIlluminatedHeightMapShader=linkVertexAndFragmentShader("SurfaceShadowedIlluminatedHeightMapShader");
	dataItem->shadowedIlluminatedHeightMapShaderUniforms[0]=glGetUniformLocationARB(dataItem->shadowedIlluminatedHeightMapShader,"depthSampler");
	dataItem->shadowedIlluminatedHeightMapShaderUniforms[1]=glGetUniformLocationARB(dataItem->shadowedIlluminatedHeightMapShader,"depthProjection");
	dataItem->shadowedIlluminatedHeightMapShaderUniforms[2]=glGetUniformLocationARB(dataItem->shadowedIlluminatedHeightMapShader,"tangentDepthProjection");
	dataItem->shadowedIlluminatedHeightMapShaderUniforms[3]=glGetUniformLocationARB(dataItem->shadowedIlluminatedHeightMapShader,"basePlane");
	dataItem->shadowedIlluminatedHeightMapShaderUniforms[4]=glGetUniformLocationARB(dataItem->shadowedIlluminatedHeightMapShader,"pixelCornerElevationSampler");
	dataItem->shadowedIlluminatedHeightMapShaderUniforms[5]=glGetUniformLocationARB(dataItem->shadowedIlluminatedHeightMapShader,"contourLineFactor");
	dataItem->shadowedIlluminatedHeightMapShaderUniforms[6]=glGetUniformLocationARB(dataItem->shadowedIlluminatedHeightMapShader,"heightColorMapSampler");
	dataItem->shadowedIlluminatedHeightMapShaderUniforms[7]=glGetUniformLocationARB(dataItem->shadowedIlluminatedHeightMapShader,"heightColorMapTransformation");
	dataItem->shadowedIlluminatedHeightMapShaderUniforms[8]=glGetUniformLocationARB(dataItem->shadowedIlluminatedHeightMapShader,"waterLevelSampler");
	dataItem->shadowedIlluminatedHeightMapShaderUniforms[9]=glGetUniformLocationARB(dataItem->shadowedIlluminatedHeightMapShader,"waterLevelTextureTransformation");
	dataItem->shadowedIlluminatedHeightMapShaderUniforms[10]=glGetUniformLocationARB(dataItem->shadowedIlluminatedHeightMapShader,"waterOpacity");
	dataItem->shadowedIlluminatedHeightMapShaderUniforms[11]=glGetUniformLocationARB(dataItem->shadowedIlluminatedHeightMapShader,"shadowTextureSampler");
	dataItem->shadowedIlluminatedHeightMapShaderUniforms[12]=glGetUniformLocationARB(dataItem->shadowedIlluminatedHeightMapShader,"shadowProjection");
	}

void SurfaceRenderer::setDrawContourLines(bool newDrawContourLines)
	{
	std::cout<<"16.1: SetDrawContourLines" << std::endl;
	drawContourLines=newDrawContourLines;
	++surfaceSettingsVersion;
	}

void SurfaceRenderer::setContourLineDistance(GLfloat newContourLineDistance)
	{
	std::cout<<"16.2: SetContourLineDistance" << std::endl;
	/* Establecer el nuevo factor de línea de contorno: */
	contourLineFactor=1.0f/newContourLineDistance;
	}

void SurfaceRenderer::setElevationColorMap(ElevationColorMap* newElevationColorMap)
	{
	std::cout<<"16.3: SetElevationColorMap" << std::endl;
	/* Compruebe si la configuración de este mapa de color de elevación invalida el sombreado: */
	if(dem==0 && ((newElevationColorMap != 0 && elevationColorMap == 0)||(newElevationColorMap == 0 && elevationColorMap != 0)))
		++surfaceSettingsVersion;
	
	/* Establecer el mapa de color de elevación: */
	elevationColorMap = newElevationColorMap;
	}

void SurfaceRenderer::setDrawDippingBed(bool newDrawDippingBed)
	{
	drawDippingBed=newDrawDippingBed;//algo de luz
	++surfaceSettingsVersion;
	}

void SurfaceRenderer::setDippingBedPlane(const SurfaceRenderer::Plane& newDippingBedPlane)
	{
	/* Establezca el modo de cama de inmersión para planar: */
	if(dippingBedFolded)
		{
		dippingBedFolded=false;
		++surfaceSettingsVersion;
		}
	
	/* Establecer la ecuación de plano de la cama de inmersión: */
	dippingBedPlane=newDippingBedPlane;
	}

void SurfaceRenderer::setDippingBedCoeffs(const GLfloat newDippingBedCoeffs[5])
	{
	/* Establezca el modo de cama de inmersión en plegado: */
	if(!dippingBedFolded)
		{
		dippingBedFolded=true;
		++surfaceSettingsVersion;
		}
	
	/* Establecer los coeficientes de la cama de inmersión: */
	for(int i=0;i<5;++i)
		dippingBedCoeffs[i]=newDippingBedCoeffs[i];
	}

void SurfaceRenderer::setDippingBedThickness(GLfloat newDippingBedThickness)
	{		
	dippingBedThickness=newDippingBedThickness;
	std::cout<<"Grozor?->"<<dippingBedThickness<<std::endl;
	}

void SurfaceRenderer::setDem(DEM* newDem)
	{
	/* Compruebe si la configuración de este DEM invalida el shader: */
	if((newDem!=0&&dem==0)||(newDem==0&&dem!=0))
		++surfaceSettingsVersion;
	
	/* Establecer el nuevo DEM: */
	dem=newDem;
	}

void SurfaceRenderer::setDemDistScale(GLfloat newDemDistScale)
	{
	std::cout<<"16.9: SetDemDistScale" << std::endl;
	demDistScale=newDemDistScale;
	}

void SurfaceRenderer::setIlluminate(bool newIlluminate)
	{
	std::cout<<"16.4: SetIlluminate" << std::endl;

	illuminate=newIlluminate;
	++surfaceSettingsVersion;
	}

void SurfaceRenderer::setWaterTable(WaterTable2* newWaterTable)
	{
	std::cout<<"16.6: SetWaterTable" << std::endl;
	waterTable=newWaterTable;
	++surfaceSettingsVersion;
	}

void SurfaceRenderer::setAdvectWaterTexture(bool newAdvectWaterTexture)
	{
	std::cout<<"16.7: SetAdvectWaterTexture" << std::endl;
	advectWaterTexture=false; // Nueva textura de agua Advect;
	++surfaceSettingsVersion;
	}

void SurfaceRenderer::setWaterOpacity(GLfloat newWaterOpacity)
	{
	std::cout<<"16.8: SetWaterOpacity" << std::endl;
	/* Establecer el nuevo factor de opacidad: */
	waterOpacity=newWaterOpacity;
	}

void SurfaceRenderer::setAnimationTime(double newAnimationTime)
	{
	/* Establecer el nuevo tiempo de animación: */
	animationTime=newAnimationTime;
	
	/* Encuesta el monitor de archivos: */
	fileMonitor.processEvents();
	}

void SurfaceRenderer::renderSinglePass(const int viewport[4],const PTransform& projection,const OGTransform& modelview,GLContextData& contextData) const
	{
	/* Obtener el elemento de datos: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Calcula las matrices requeridas: */
	PTransform projectionModelview=projection;
	projectionModelview*=modelview;
	
	/* Compruebe si la representación de la línea de contorno está habilitada: */
	if(drawContourLines)
		{
		/* Ejecute la primera pasada de representación para crear una textura de desplazamiento de medio píxel de elevaciones de superficie: */
		renderPixelCornerElevations(viewport,projectionModelview,contextData,dataItem);
		}
	else if(dataItem->contourLineFramebufferObject!=0)
		{
		/* Elimine el búfer de cuadros de representación de líneas de contorno: */
		glDeleteFramebuffersEXT(1,&dataItem->contourLineFramebufferObject);
		dataItem->contourLineFramebufferObject=0;
		glDeleteRenderbuffersEXT(1,&dataItem->contourLineDepthBufferObject);
		dataItem->contourLineDepthBufferObject=0;
		glDeleteTextures(1,&dataItem->contourLineColorTextureObject);
		dataItem->contourLineColorTextureObject=0;
		}
	
	/* Compruebe si el sombreador de superficie de un solo paso está desactualizado: */
	if(dataItem->surfaceSettingsVersion!=surfaceSettingsVersion||(illuminate&&dataItem->lightTrackerVersion!=contextData.getLightTracker()->getVersion()))
		{
		/* Reconstruir el shader: */
		try
			{
			GLhandleARB newShader=createSinglePassSurfaceShader(*contextData.getLightTracker(),dataItem->heightMapShaderUniforms);
			glDeleteObjectARB(dataItem->heightMapShader);
			dataItem->heightMapShader=newShader;
			}
		catch(const std::runtime_error& err)
			{
			Misc::formattedUserError("SurfaceRenderer::renderSinglePass: Caught exception %s while rebuilding surface shader",err.what());
			}
		
		/* Marque el shader como actualizado: */
		dataItem->surfaceSettingsVersion=surfaceSettingsVersion;
		dataItem->lightTrackerVersion=contextData.getLightTracker()->getVersion();
		}
	
	/* Enlazar el sombreador de superficie de un solo paso: */
	glUseProgramObjectARB(dataItem->heightMapShader);
	const GLint* ulPtr=dataItem->heightMapShaderUniforms;
	
	/* Enlazar la textura de la imagen de profundidad actual: */
	glActiveTextureARB(GL_TEXTURE0_ARB);
	depthImageRenderer->bindDepthTexture(contextData);
	glUniform1iARB(*(ulPtr++),0);
	
	/* Sube la matriz de proyección de profundidad: */
	depthImageRenderer->uploadDepthProjection(*(ulPtr++));
	
	if(dem!=0)
		{
		/* Sube la transformación DEM: */
		dem->uploadDemTransform(*(ulPtr++));
		
		/* Enlazar la textura DEM: */
		glActiveTextureARB(GL_TEXTURE1_ARB);
		dem->bindTexture(contextData);
		glUniform1iARB(*(ulPtr++),1);
		
		/* Sube el factor de escala de distancia DEM: */
		glUniform1fARB(*(ulPtr++),1.0f/(demDistScale*dem->getVerticalScale()));
		}
	else if(elevationColorMap!=0)
		{
		/* Sube la ecuación del plano de mapeo de textura: */
		elevationColorMap->uploadTexturePlane(*(ulPtr++));
		
		/* Enlazar la textura del mapa de color de altura: */
		glActiveTextureARB(GL_TEXTURE1_ARB);
		elevationColorMap->bindTexture(contextData);
		glUniform1iARB(*(ulPtr++),1);
		}
	
	if(drawContourLines)
		{
		/* Enlazar la textura de elevación de la esquina del píxel: */
		glActiveTextureARB(GL_TEXTURE2_ARB);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->contourLineColorTextureObject);
		glUniform1iARB(*(ulPtr++),2);
		
		/* Sube el factor de distancia de la línea de contorno: */
		glUniform1fARB(*(ulPtr++),contourLineFactor);
		}
	
	if(drawDippingBed)
		{
		if(dippingBedFolded)
			{
			/* Subir los coeficientes de la cama de inmersión: */
			glUniformARB<1>(*(ulPtr++),5,dippingBedCoeffs);
			}
		else
			{
			/* Sube la ecuación del plano de lecho de inmersión: */
			GLfloat planeEq[4];
			for(int i=0;i<3;++i)
				planeEq[i]=dippingBedPlane.getNormal()[i];
			planeEq[3]=-dippingBedPlane.getOffset();
			glUniformARB<4>(*(ulPtr++),1,planeEq);
			}
		
		/* Subir el espesor de la cama de inmersión: */
		glUniform1fARB(*(ulPtr++),dippingBedThickness);
		}
	
	if(illuminate)
		{
		/* Sube la matriz de modelview: */
		glUniformARB(*(ulPtr++),modelview);
		
		/* Calcule y cargue la matriz de proyección de profundidad de vista de modelo de plano tangente: */
		PTransform tangentModelviewDepthProjection=tangentDepthProjection;
		tangentModelviewDepthProjection*=Geometry::invert(modelview);
		const Scalar* tmdpPtr=tangentModelviewDepthProjection.getMatrix().getEntries();
		GLfloat matrix[16];
		GLfloat* mPtr=matrix;
		for(int i=0;i<16;++i,++tmdpPtr,++mPtr)
				*mPtr=GLfloat(*tmdpPtr);
		glUniformMatrix4fvARB(*(ulPtr++),1,GL_FALSE,matrix);
		}
	
	if(waterTable!=0&&dem==0)
		{
		/* Sube la matriz de coordenadas de la textura de la tabla de agua: */
		waterTable->uploadWaterTextureTransform(*(ulPtr++));
		
		/* Enlazar la textura de la batimetría: */
		glActiveTextureARB(GL_TEXTURE3_ARB);
		waterTable->bindBathymetryTexture(contextData);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
		glUniform1iARB(*(ulPtr++),3);
		
		/* Enlazar las cantidades de textura: */
		glActiveTextureARB(GL_TEXTURE4_ARB);
		waterTable->bindQuantityTexture(contextData);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
		glUniform1iARB(*(ulPtr++),4);
		
		/* Suba el tamaño de celda de la rejilla de agua para el cálculo de vector normal: */
		glUniformARB<2>(*(ulPtr++),1,waterTable->getCellSize());
		
		/* Sube el factor de opacidad del agua: */
		glUniform1fARB(*(ulPtr++),waterOpacity);
		
		/* Sube el tiempo de animación del agua: */
		glUniform1fARB(*(ulPtr++),GLfloat(animationTime));
		}
	
	/* Cargue la matriz combinada de proyección, vista de modelo y profundidad de proyección: */
	PTransform projectionModelviewDepthProjection=projectionModelview;
	projectionModelviewDepthProjection*=depthImageRenderer->getDepthProjection();
	glUniformARB(*(ulPtr++),projectionModelviewDepthProjection);
	
	/* Dibuja la superficie: */
	depthImageRenderer->renderSurfaceTemplate(contextData);
	
	/* Desenlazar todas las texturas y buffers: */
	if(waterTable!=0&&dem==0)
		{
		glActiveTextureARB(GL_TEXTURE4_ARB);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_S,GL_CLAMP);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_T,GL_CLAMP);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
		glActiveTextureARB(GL_TEXTURE3_ARB);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_S,GL_CLAMP);
		glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_T,GL_CLAMP);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
		}
	if(drawContourLines)
		{
		glActiveTextureARB(GL_TEXTURE2_ARB);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
		}
	if(dem!=0)
		{
		glActiveTextureARB(GL_TEXTURE1_ARB);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
		}
	else if(elevationColorMap!=0)
		{
		glActiveTextureARB(GL_TEXTURE1_ARB);
		glBindTexture(GL_TEXTURE_1D,0);
		}
	glActiveTextureARB(GL_TEXTURE0_ARB);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
	
	/* Desenlazar el mapa de altura shader: */
	glUseProgramObjectARB(0);
	}

#if 0

void SurfaceRenderer::renderGlobalAmbientHeightMap(GLuint heightColorMapTexture,GLContextData& contextData) const
	{
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Check if contour line rendering is enabled: */
	if(drawContourLines)
		{
		/* Run the first rendering pass to create a half-pixel offset texture of surface elevations: */
		glPrepareContourLines(contextData);
		}
	else if(dataItem->contourLineFramebufferObject!=0)
		{
		/* Delete the contour line rendering frame buffer: */
		glDeleteFramebuffersEXT(1,&dataItem->contourLineFramebufferObject);
		dataItem->contourLineFramebufferObject=0;
		glDeleteRenderbuffersEXT(1,&dataItem->contourLineDepthBufferObject);
		dataItem->contourLineDepthBufferObject=0;
		glDeleteTextures(1,&dataItem->contourLineColorTextureObject);
		dataItem->contourLineColorTextureObject=0;
		}
	
	/* Bind the global ambient height map shader: */
	glUseProgramObjectARB(dataItem->globalAmbientHeightMapShader);
	
	/* Bind the vertex and index buffers: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,dataItem->vertexBuffer);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,dataItem->indexBuffer);
	
	/* Set up the depth image texture: */
	if(!usePreboundDepthTexture)
		{
		glActiveTextureARB(GL_TEXTURE0_ARB);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->depthTexture);
		
		/* Check if the texture is outdated: */
		if(dataItem->depthTextureVersion!=depthImageVersion)
			{
			/* Upload the new depth texture: */
			glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB,0,0,0,size[0],size[1],GL_LUMINANCE,GL_FLOAT,depthImage.getBuffer());
			
			/* Mark the depth texture as current: */
			dataItem->depthTextureVersion=depthImageVersion;
			}
		}
	glUniform1iARB(dataItem->globalAmbientHeightMapShaderUniforms[0],0);
	
	/* Upload the depth projection matrix: */
	glUniformMatrix4fvARB(dataItem->globalAmbientHeightMapShaderUniforms[1],1,GL_FALSE,depthProjectionMatrix);
	
	/* Upload the base plane equation: */
	glUniformARB<4>(dataItem->globalAmbientHeightMapShaderUniforms[2],1,basePlaneEq);
	
	/* Bind the pixel corner elevation texture: */
	glActiveTextureARB(GL_TEXTURE1_ARB);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->contourLineColorTextureObject);
	glUniform1iARB(dataItem->globalAmbientHeightMapShaderUniforms[3],1);
	
	/* Upload the contour line distance factor: */
	glUniform1fARB(dataItem->globalAmbientHeightMapShaderUniforms[4],contourLineFactor);
	
	/* Bind the height color map texture: */
	glActiveTextureARB(GL_TEXTURE2_ARB);
	glBindTexture(GL_TEXTURE_1D,heightColorMapTexture);
	glUniform1iARB(dataItem->globalAmbientHeightMapShaderUniforms[5],2);
	
	/* Upload the height color map texture coordinate transformation: */
	glUniform2fARB(dataItem->globalAmbientHeightMapShaderUniforms[6],heightMapScale,heightMapOffset);
	
	if(waterTable!=0)
		{
		/* Bind the water level texture: */
		glActiveTextureARB(GL_TEXTURE3_ARB);
		//waterTable->bindWaterLevelTexture(contextData);
		glUniform1iARB(dataItem->globalAmbientHeightMapShaderUniforms[7],3);
		
		/* Upload the water table texture coordinate matrix: */
		glUniformMatrix4fvARB(dataItem->globalAmbientHeightMapShaderUniforms[8],1,GL_FALSE,waterTable->getWaterTextureMatrix());
		
		/* Upload the water opacity factor: */
		glUniform1fARB(dataItem->globalAmbientHeightMapShaderUniforms[9],waterOpacity);
		}
	
	/* Draw the surface: */
	typedef GLGeometry::Vertex<void,0,void,0,void,float,3> Vertex;
	GLVertexArrayParts::enable(Vertex::getPartsMask());
	glVertexPointer(static_cast<const Vertex*>(0));
	for(unsigned int y=1;y<size[1];++y)
		glDrawElements(GL_QUAD_STRIP,size[0]*2,GL_UNSIGNED_INT,static_cast<const GLuint*>(0)+(y-1)*size[0]*2);
	GLVertexArrayParts::disable(Vertex::getPartsMask());
	
	/* Unbind all textures and buffers: */
	if(waterTable!=0)
		{
		glActiveTextureARB(GL_TEXTURE3_ARB);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
		}
	glActiveTextureARB(GL_TEXTURE2_ARB);
	glBindTexture(GL_TEXTURE_1D,0);
	glActiveTextureARB(GL_TEXTURE1_ARB);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
	if(!usePreboundDepthTexture)
		{
		glActiveTextureARB(GL_TEXTURE0_ARB);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
		}
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,0);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);
	
	/* Unbind the global ambient height map shader: */
	glUseProgramObjectARB(0);
	}

void SurfaceRenderer::renderShadowedIlluminatedHeightMap(GLuint heightColorMapTexture,GLuint shadowTexture,const PTransform& shadowProjection,GLContextData& contextData) const
	{
	/* Get the data item: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Bind the shadowed illuminated height map shader: */
	glUseProgramObjectARB(dataItem->shadowedIlluminatedHeightMapShader);
	
	/* Bind the vertex and index buffers: */
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,dataItem->vertexBuffer);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,dataItem->indexBuffer);
	
	/* Set up the depth image texture: */
	if(!usePreboundDepthTexture)
		{
		glActiveTextureARB(GL_TEXTURE0_ARB);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->depthTexture);
		
		/* Check if the texture is outdated: */
		if(dataItem->depthTextureVersion!=depthImageVersion)
			{
			/* Upload the new depth texture: */
			glTexSubImage2D(GL_TEXTURE_RECTANGLE_ARB,0,0,0,size[0],size[1],GL_LUMINANCE,GL_FLOAT,depthImage.getBuffer());
			
			/* Mark the depth texture as current: */
			dataItem->depthTextureVersion=depthImageVersion;
			}
		}
	glUniform1iARB(dataItem->shadowedIlluminatedHeightMapShaderUniforms[0],0);
	
	/* Upload the depth projection matrix: */
	glUniformMatrix4fvARB(dataItem->shadowedIlluminatedHeightMapShaderUniforms[1],1,GL_FALSE,depthProjectionMatrix);
	
	/* Upload the tangent-plane depth projection matrix: */
	glUniformMatrix4fvARB(dataItem->shadowedIlluminatedHeightMapShaderUniforms[2],1,GL_FALSE,tangentDepthProjectionMatrix);
	
	/* Upload the base plane equation: */
	glUniformARB<4>(dataItem->shadowedIlluminatedHeightMapShaderUniforms[3],1,basePlaneEq);
	
	/* Bind the pixel corner elevation texture: */
	glActiveTextureARB(GL_TEXTURE1_ARB);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,dataItem->contourLineColorTextureObject);
	glUniform1iARB(dataItem->shadowedIlluminatedHeightMapShaderUniforms[4],1);
	
	/* Upload the contour line distance factor: */
	glUniform1fARB(dataItem->shadowedIlluminatedHeightMapShaderUniforms[5],contourLineFactor);
	
	/* Bind the height color map texture: */
	glActiveTextureARB(GL_TEXTURE2_ARB);
	glBindTexture(GL_TEXTURE_1D,heightColorMapTexture);
	glUniform1iARB(dataItem->shadowedIlluminatedHeightMapShaderUniforms[6],2);
	
	/* Upload the height color map texture coordinate transformation: */
	glUniform2fARB(dataItem->shadowedIlluminatedHeightMapShaderUniforms[7],heightMapScale,heightMapOffset);
	
	if(waterTable!=0)
		{
		/* Bind the water level texture: */
		glActiveTextureARB(GL_TEXTURE3_ARB);
		//waterTable->bindWaterLevelTexture(contextData);
		glUniform1iARB(dataItem->shadowedIlluminatedHeightMapShaderUniforms[8],3);
		
		/* Upload the water table texture coordinate matrix: */
		glUniformMatrix4fvARB(dataItem->shadowedIlluminatedHeightMapShaderUniforms[9],1,GL_FALSE,waterTable->getWaterTextureMatrix());
		
		/* Upload the water opacity factor: */
		glUniform1fARB(dataItem->shadowedIlluminatedHeightMapShaderUniforms[10],waterOpacity);
		}
	
	/* Bind the shadow texture: */
	glActiveTextureARB(GL_TEXTURE4_ARB);
	glBindTexture(GL_TEXTURE_2D,shadowTexture);
	glUniform1iARB(dataItem->shadowedIlluminatedHeightMapShaderUniforms[11],4);
	
	/* Upload the combined shadow viewport, shadow projection and modelview, and depth projection matrix: */
	PTransform spdp(1.0);
	spdp.getMatrix()(0,0)=0.5;
	spdp.getMatrix()(0,3)=0.5;
	spdp.getMatrix()(1,1)=0.5;
	spdp.getMatrix()(1,3)=0.5;
	spdp.getMatrix()(2,2)=0.5;
	spdp.getMatrix()(2,3)=0.5;
	spdp*=shadowProjection;
	spdp*=depthProjection;
	GLfloat spdpMatrix[16];
	GLfloat* spdpPtr=spdpMatrix;
	for(int j=0;j<4;++j)
		for(int i=0;i<4;++i,++spdpPtr)
			*spdpPtr=GLfloat(spdp.getMatrix()(i,j));
	glUniformMatrix4fvARB(dataItem->shadowedIlluminatedHeightMapShaderUniforms[12],1,GL_FALSE,spdpMatrix);
	
	/* Draw the surface: */
	typedef GLGeometry::Vertex<void,0,void,0,void,float,3> Vertex;
	GLVertexArrayParts::enable(Vertex::getPartsMask());
	glVertexPointer(static_cast<const Vertex*>(0));
	for(unsigned int y=1;y<size[1];++y)
		glDrawElements(GL_QUAD_STRIP,size[0]*2,GL_UNSIGNED_INT,static_cast<const GLuint*>(0)+(y-1)*size[0]*2);
	GLVertexArrayParts::disable(Vertex::getPartsMask());
	
	/* Unbind all textures and buffers: */
	if(waterTable!=0)
		{
		glActiveTextureARB(GL_TEXTURE3_ARB);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
		}
	glActiveTextureARB(GL_TEXTURE2_ARB);
	glBindTexture(GL_TEXTURE_1D,0);
	glActiveTextureARB(GL_TEXTURE1_ARB);
	glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
	if(!usePreboundDepthTexture)
		{
		glActiveTextureARB(GL_TEXTURE0_ARB);
		glBindTexture(GL_TEXTURE_RECTANGLE_ARB,0);
		}
	glBindBufferARB(GL_ARRAY_BUFFER_ARB,0);
	glBindBufferARB(GL_ELEMENT_ARRAY_BUFFER_ARB,0);
	
	/* Unbind the shadowed illuminated height map shader: */
	glUseProgramObjectARB(0);
	}

#endif
