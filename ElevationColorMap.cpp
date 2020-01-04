/***********************************************************************
ElevationColorMap - Clase para representar mapas de color de elevación
para mapas topográficos.
Copyright (c) 2014-2016 Oliver Kreylos

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

#include "ElevationColorMap.h"

#include <string>
#include <Misc/ThrowStdErr.h>
#include <Misc/FileNameExtensions.h>
#include <IO/ValueSource.h>
#include <GL/gl.h>
#include <GL/GLContextData.h>
#include <GL/Extensions/GLARBShaderObjects.h>
#include <Vrui/OpenFile.h>
#include <iostream>

#include "Types.h"
#include "DepthImageRenderer.h"

#include "Config.h"

/**********************************
Methods of class ElevationColorMap:
**********************************/

ElevationColorMap::ElevationColorMap(const char* heightMapName)
	{
	/* Cargue el mapa de altura dado: */
	std::cout<<"5: ElevationColorMap " << std::endl;
	ElevationColorMap::load(heightMapName);
	}

void ElevationColorMap::initContext(GLContextData& contextData) const
	{
	/* Inicialice las extensiones OpenGL requeridas: */
	GLARBShaderObjects::initExtension();
	
	/* Cree el elemento de datos y asócielo con este objeto: */
	DataItem* dataItem=new DataItem;
	contextData.addDataItem(this,dataItem);
	}

void ElevationColorMap::load(const char* heightMapName)
	{
	/* Abra el archivo de mapa de altura: */
	std::string fullHeightMapName;
	if(heightMapName[0]=='/')
	{
		/* Utilice el nombre de archivo absoluto directamente: */
		fullHeightMapName=heightMapName;
	}
	else
	{
		/* Arme un nombre de archivo relativo al directorio de archivos de configuración: */
		fullHeightMapName=CONFIG_CONFIGDIR;
		fullHeightMapName.push_back('/');
		fullHeightMapName.append(heightMapName);
	}
	std::cout<<"5.1: LoadMap-> "<< fullHeightMapName << std::endl;
	/* Abra el archivo de mapa de altura: */
	IO::ValueSource heightMapSource(Vrui::openFile(fullHeightMapName.c_str()));
	
	/* Cargue el mapa de color de altura: */
	std::vector<Color> heightMapColors;
	std::vector<GLdouble> heightMapKeys;
	if(Misc::hasCaseExtension(heightMapName,".cpt"))
	{
		std::cout<<"5.2: MapName " << std::endl;
		heightMapSource.setPunctuation("\n");
		heightMapSource.skipWs();
		int line=1;
		std::cout<<"5.3: Colores " << std::endl;
		while(!heightMapSource.eof())
		{
			/* Lea el siguiente valor clave del mapa de color: */
			heightMapKeys.push_back(GLdouble(heightMapSource.readNumber()));
			
			/* Lea el siguiente valor clave del mapa de color: */
			Color color;
			for(int i=0;i<3;++i)
			{
				color[i]=Color::Scalar(heightMapSource.readNumber()/255.0);
			}
			color[3]=Color::Scalar(1);
			heightMapColors.push_back(color);
			
			if(!heightMapSource.isLiteral('\n'))
				Misc::throwStdErr("ElevationColorMap: Color map format error in line %d of file %s",line,fullHeightMapName.c_str());
			++line;
		}
	}
	else
	{
		heightMapSource.setPunctuation(",\n");
		heightMapSource.skipWs();
		int line=1;
		while(!heightMapSource.eof())
		{
			/* Lea el siguiente valor clave del mapa de color: */
			heightMapKeys.push_back(GLdouble(heightMapSource.readNumber()));
			if(!heightMapSource.isLiteral(','))
				Misc::throwStdErr("ElevationColorMap: Color map format error in line %d of file %s",line,fullHeightMapName.c_str());
			
			/* Lea el siguiente valor clave del mapa de color: */
			Color color;
			for(int i=0;i<3;++i)
				color[i]=Color::Scalar(heightMapSource.readNumber());
			color[3]=Color::Scalar(1);
			heightMapColors.push_back(color);
			
			if(!heightMapSource.isLiteral('\n'))
				Misc::throwStdErr("ElevationColorMap: Color map format error in line %d of file %s",line,fullHeightMapName.c_str());
			++line;
		}
	}
	
	/* Crear el mapa de color: */
	//std::cout<<"5.4.5: size-> "<< heightMapKeys.size()<< " MapColors->"<< &heightMapKeys[0] << std::endl;

	setColors(heightMapKeys.size(),&heightMapColors[0],&heightMapKeys[0],256);
	
	/* Invalide el objeto de textura de mapa de color: */
	++textureVersion;
	}

void ElevationColorMap::calcTexturePlane(const Plane& basePlane)
	{
	std::cout<<"15.1: calcTexturePlane " << std::endl;
	/* Escala y desplaza la ecuación del plano base del espacio de la cámara: */
	const Plane::Vector& bpn=basePlane.getNormal();
	Scalar bpo=basePlane.getOffset();
	Scalar hms=Scalar(getNumEntries()-1)/((getScalarRangeMax()-getScalarRangeMin())*Scalar(getNumEntries()));
	Scalar hmo=Scalar(0.5)/Scalar(getNumEntries())-hms*getScalarRangeMin();
	for(int i=0;i<3;++i)
		texturePlaneEq[i]=GLfloat(bpn[i]*hms);
	texturePlaneEq[3]=GLfloat(-bpo*hms+hmo);
	}

void ElevationColorMap::calcTexturePlane(const DepthImageRenderer* depthImageRenderer)
	{
	std::cout<<"15: calcTexturePlane " << std::endl;
	/* Calcule el plano de textura basándose en el plano base del procesador de imágenes de profundidad dado: */
	calcTexturePlane(depthImageRenderer->getBasePlane());
	}

void ElevationColorMap::bindTexture(GLContextData& contextData) const
	{
	/* Recuperar el elemento de datos: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Enlazar el objeto de textura: */
	glBindTexture(GL_TEXTURE_1D,dataItem->textureObjectId);
	
	/* Compruebe si la textura del mapa de color está desactualizada: */
	if(dataItem->textureObjectVersion!=textureVersion)
		{
		/* Sube las entradas del mapa de color como textura 1D: */
		glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
		glTexParameteri(GL_TEXTURE_1D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
		glTexImage1D(GL_TEXTURE_1D,0,GL_RGB8,getNumEntries(),0,GL_RGBA,GL_FLOAT,getColors());
		
		dataItem->textureObjectVersion=textureVersion;
		}
	}

void ElevationColorMap::uploadTexturePlane(GLint location) const
	{
	/* Sube la ecuación del plano de mapeo de textura: */
	glUniformARB<4>(location,1,texturePlaneEq);
	}
