/***********************************************************************
BathymetrySaverTool: Herramienta para guardar la cuadrícula de batimetría 
actual de un entorno limitado de realidad aumentada en un archivo o socket de red.
Copyright (c) 2016 Oliver Kreylos

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

#ifndef BATHYMETRYSAVERTOOL_INCLUDED
#define BATHYMETRYSAVERTOOL_INCLUDED

#include <string>
#include <GL/gl.h>
#include <Vrui/Tool.h>
#include <Vrui/Application.h>

#include "Types.h"

/* Declaraciones de reenvío: */
class WaterTable2;
class Sandbox;
class BathymetrySaverTool;

class BathymetrySaverToolFactory:public Vrui::ToolFactory
	{
	friend class BathymetrySaverTool;
	
	/* Clases integradas: */
	private:
	struct Configuration // Estructura que contiene configuraciones de herramientas
		{
		/* Elementos: */
		public:
		std::string saveFileName; // Nombre del archivo en el que guardar la cuadrícula de batimetría
		bool postUpdate; // Marque si desea publicar un mensaje de actualización en un servidor web después de guardar la cuadrícula de batimetría
		std::string postUpdateHostName; // Nombre del servidor web al que enviar mensajes de actualización
		int postUpdatePort; // Número de puerto TCP del servidor web al que enviar mensajes de actualización
		std::string postUpdatePage; // Nombre de la página en el servidor web en la que se publican los mensajes de actualización
		std::string postUpdateMessage; // El mensaje a enviar al servidor web
		double gridScale; // Factor de escala general para aplicar a las redes en la exportación
		
		/* Constructores y destructores: */
		Configuration(void); // Crea la configuración predeterminada
		/* Metodos: */
		void read(const Misc::ConfigurationFileSection& cfs); // Anula la configuración de la sección del archivo de configuración
		void write(Misc::ConfigurationFileSection& cfs) const; // Escribe la configuración en la sección del archivo de configuración
		};
	
	/* Elementos: */
	private:
	Configuration configuration; // Configuración predeterminada para todas las herramientas.
	WaterTable2* waterTable; // Puntero al objeto de la mesa de agua desde el cual solicitar rejillas de batimetría
	GLsizei gridSize[2]; // Ancho y alto de la rejilla de batimetría de la capa freática
	GLfloat cellSize[2]; // Ancho y alto de cada celda de nivel freático
	
	/* Constructores y destructores: */
	public:
	BathymetrySaverToolFactory(WaterTable2* sWaterTable,Vrui::ToolManager& toolManager);
	virtual ~BathymetrySaverToolFactory(void);
	
	/* Métodos de Vrui :: ToolFactory: */
	virtual const char* getName(void) const;
	virtual const char* getButtonFunction(int buttonSlotIndex) const;
	virtual Vrui::Tool* createTool(const Vrui::ToolInputAssignment& inputAssignment) const;
	virtual void destroyTool(Vrui::Tool* tool) const;
	};

class BathymetrySaverTool:public Vrui::Tool,public Vrui::Application::Tool<Sandbox>
	{
	friend class BathymetrySaverToolFactory;
	
	/* Elementos: */
	private:
	static BathymetrySaverToolFactory* factory; // Puntero al objeto de fábrica para esta clase
	BathymetrySaverToolFactory::Configuration configuration; // Configuracion de esta herramienta
	GLfloat* bathymetryBuffer; // Búfer de batimetría
	bool requestPending; // Marque si esta herramienta tiene una solicitud pendiente para recuperar una cuadrícula de batimetría
	
	/* Métodos privados: */
	void writeDEMFile(void) const; // Escribe la cuadrícula de batimetría en un archivo en formato USGS DEM
	void postUpdate(void) const; // Envía un mensaje de actualización a un servidor web
	
	/* Constructores y destructores: */
	public:
	static BathymetrySaverToolFactory* initClass(WaterTable2* sWaterTable,Vrui::ToolManager& toolManager);
	BathymetrySaverTool(const Vrui::ToolFactory* factory,const Vrui::ToolInputAssignment& inputAssignment);
	virtual ~BathymetrySaverTool(void);
	
	/* Metodos de la clase Vrui::Tool: */
	virtual void configure(const Misc::ConfigurationFileSection& configFileSection);
	virtual void storeState(Misc::ConfigurationFileSection& configFileSection) const;
	virtual const Vrui::ToolFactory* getFactory(void) const;
	virtual void buttonCallback(int buttonSlotIndex,Vrui::InputDevice::ButtonCallbackData* cbData);
	virtual void frame(void);
	};

#endif
