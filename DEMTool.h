/***********************************************************************
DEMTool: Clase de herramienta para cargar un modelo de elevación digital 
en un entorno limitado de realidad aumentada para colorear la superficie 
de arena según la distancia al DEM.
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

#ifndef DEMTOOL_INCLUDED
#define DEMTOOL_INCLUDED

#include <string>
#include <GL/gl.h>
#include <GL/GLObject.h>
#include <GLMotif/FileSelectionHelper.h>
#include <Vrui/Tool.h>
#include <Vrui/Application.h>

#include "Types.h"
#include "DEM.h"

/* Declaraciones de reenvío: */
class Sandbox;
class DEMTool;

class DEMToolFactory:public Vrui::ToolFactory
	{
	friend class DEMTool;
	
	/* Elementos: */
	private:
	GLMotif::FileSelectionHelper demSelectionHelper; // Objeto auxiliar para cargar DEM desde archivos
	
	/* Constructores y destructores: */
	public:
	DEMToolFactory(Vrui::ToolManager& toolManager);
	virtual ~DEMToolFactory(void);
	
	/* Métodos de Vrui::ToolFactory: */
	virtual const char* getName(void) const;
	virtual const char* getButtonFunction(int buttonSlotIndex) const;
	virtual Vrui::Tool* createTool(const Vrui::ToolInputAssignment& inputAssignment) const;
	virtual void destroyTool(Vrui::Tool* tool) const;
	};

class DEMTool:public DEM,public Vrui::Tool,public Vrui::Application::Tool<Sandbox>
	{
	friend class DEMToolFactory;
	
	/* Elementos: */
	private:
	static DEMToolFactory* factory; // Puntero al objeto de fábrica para esta clase
	std::string demFileName; // Nombre del archivo DEM para cargar
	bool haveDemTransform; // Marcar si la sección del archivo de configuración de la herramienta especificó una transformación DEM
	OGTransform demTransform; // La transformación para aplicar a la DEM
	Scalar demVerticalShift; // Desplazamiento vertical adicional para aplicar a DEM en unidades de coordenadas de sandbox
	Scalar demVerticalScale; // La exageración vertical para aplicar al DEM
	
	/* Métodos privados: */
	void loadDEMFile(const char* demFileName); // Carga un DEM desde un archivo
	void loadDEMFileCallback(GLMotif::FileSelectionDialog::OKCallbackData* cbData); // Se llama cuando el usuario selecciona un archivo DEM para cargar
	
	/* Constructores y destructores: */
	public:
	static DEMToolFactory* initClass(Vrui::ToolManager& toolManager);
	DEMTool(const Vrui::ToolFactory* factory,const Vrui::ToolInputAssignment& inputAssignment);
	virtual ~DEMTool(void);
	
	/* Métodos de la clase Vrui::Tool: */
	virtual void configure(const Misc::ConfigurationFileSection& configFileSection);
	virtual void initialize(void);
	virtual const Vrui::ToolFactory* getFactory(void) const;
	virtual void buttonCallback(int buttonSlotIndex,Vrui::InputDevice::ButtonCallbackData* cbData);
	};

#endif
