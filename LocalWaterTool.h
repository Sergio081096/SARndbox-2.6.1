/***********************************************************************
LocalWaterTool: Clase de herramienta para agregar o eliminar agua 
localmente de un entorno limitado de realidad aumentada.
Copyright (c) 2012-2013 Oliver Kreylos

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

#ifndef LOCALWATERTOOL_INCLUDED
#define LOCALWATERTOOL_INCLUDED

#include <GL/gl.h>
#include <GL/GLObject.h>
#include <Vrui/Tool.h>
#include <Vrui/GenericToolFactory.h>
#include <Vrui/TransparentObject.h>
#include <Vrui/Application.h>

/* Declaraciones de reenvío: */
namespace Misc {
template <class ParameterParam>
class FunctionCall;
}
class GLContextData;
typedef Misc::FunctionCall<GLContextData&> AddWaterFunction;
class Sandbox;
class LocalWaterTool;
typedef Vrui::GenericToolFactory<LocalWaterTool> LocalWaterToolFactory;

class LocalWaterTool:public Vrui::Tool,public Vrui::Application::Tool<Sandbox>,public GLObject,public Vrui::TransparentObject
	{
	friend class Vrui::GenericToolFactory<LocalWaterTool>;
	
	/* Elementos: */
	private:
	static LocalWaterToolFactory* factory; // Puntero al objeto de fábrica para esta clase
	
	const AddWaterFunction* addWaterFunction; // Función de procesamiento registrada con la capa freática
	GLfloat adding; // Cantidad de datos agregados o eliminados de la capa freática
	
	/* Constructores y destructores: */
	public:
	static LocalWaterToolFactory* initClass(Vrui::ToolManager& toolManager);
	LocalWaterTool(const Vrui::ToolFactory* factory,const Vrui::ToolInputAssignment& inputAssignment);
	virtual ~LocalWaterTool(void);
	
	/* Métodos de la clase Vrui::Herramienta: */
	virtual void initialize(void);
	virtual void deinitialize(void);
	virtual const Vrui::ToolFactory* getFactory(void) const;
	virtual void buttonCallback(int buttonSlotIndex,Vrui::InputDevice::ButtonCallbackData* cbData);
	
	/* Métodos de la clase GLObject: */
	virtual void initContext(GLContextData& contextData) const;
	
	/* Métodos de la clase Vrui::TransparentObject: */
	virtual void glRenderActionTransparent(GLContextData& contextData) const;
	
	/* Nuevos métodos: */
	void addWater(GLContextData& contextData) const; // Función para renderizar geometría que agrega agua a la capa freática
	};

#endif
