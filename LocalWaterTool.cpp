/***********************************************************************
LocalWaterTool - Clase de herramienta para agregar o eliminar agua 
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

#include "LocalWaterTool.h"

#include <Misc/FunctionCalls.h>
#include <GL/gl.h>
#include <GL/Extensions/GLARBVertexProgram.h>
#include <GL/GLGeometryWrappers.h>
#include <GL/GLTransformationWrappers.h>
#include <Vrui/Vrui.h>
#include <Vrui/ToolManager.h>
#include <Vrui/DisplayState.h>

#include "WaterTable2.h"
#include "Sandbox.h"

/***************************************
Static elements of class LocalWaterTool:
***************************************/

LocalWaterToolFactory* LocalWaterTool::factory=0;

/*******************************
Methods of class LocalWaterTool:
*******************************/

LocalWaterToolFactory* LocalWaterTool::initClass(Vrui::ToolManager& toolManager)
	{
	/* Crea la fábrica de herramientas: */
	factory=new LocalWaterToolFactory("LocalWaterTool","Manage Water Locally",0,toolManager);
	
	/* Configure el diseño de entrada de la clase de herramienta: */
	factory->setNumButtons(2);
	factory->setButtonFunction(0,"Rain");
	factory->setButtonFunction(1,"Dry");
	
	/* Registro y retorno de clase */
	toolManager.addClass(factory,Vrui::ToolManager::defaultToolFactoryDestructor);
	return factory;
	}

LocalWaterTool::LocalWaterTool(const Vrui::ToolFactory* factory,const Vrui::ToolInputAssignment& inputAssignment)
	:Vrui::Tool(factory,inputAssignment),
	 addWaterFunction(0),
	 adding(0.0f)
	{
	}

LocalWaterTool::~LocalWaterTool(void)
	{
	}

void LocalWaterTool::initialize(void)
	{
	/* Registre una función de representación con la capa freática: */
	if(application->waterTable!=0)
		{
		addWaterFunction=Misc::createFunctionCall(this,&LocalWaterTool::addWater);
		application->waterTable->addRenderFunction(addWaterFunction);
		}
	}

void LocalWaterTool::deinitialize(void)
	{
	/* Anule el registro de la función de procesamiento de la capa freática: */
	if(application->waterTable!=0)
		application->waterTable->removeRenderFunction(addWaterFunction);
	delete addWaterFunction;
	addWaterFunction=0;
	}

const Vrui::ToolFactory* LocalWaterTool::getFactory(void) const
	{
	return factory;
	}

void LocalWaterTool::buttonCallback(int buttonSlotIndex,Vrui::InputDevice::ButtonCallbackData* cbData)
	{
	GLfloat waterAmount=application->rainStrength;
	if(!cbData->newButtonState)
		waterAmount=-waterAmount;
	if(buttonSlotIndex==1)
		waterAmount=-waterAmount;
	adding+=waterAmount;
	}

void LocalWaterTool::initContext(GLContextData& contextData) const
	{
	/* Inicialice las extensiones OpenGL requeridas: */
	GLARBVertexProgram::initExtension();
	}

void LocalWaterTool::glRenderActionTransparent(GLContextData& contextData) const
	{
	glPushAttrib(GL_ENABLE_BIT|GL_POLYGON_BIT);
	
	/* Ir a coordenadas de navegación: */
	glPushMatrix();
	glLoadMatrix(Vrui::getDisplayState(contextData).modelviewNavigational);
	
	/* Obtenga la posición y el tamaño actual del disco de lluvia en las coordenadas de la cámara: */
	Vrui::Point rainPos=Vrui::getInverseNavigationTransformation().transform(getButtonDevicePosition(0));
	Vrui::Scalar rainRadius=Vrui::getPointPickDistance()*Vrui::Scalar(3);
	
	/* Construye el cilindro de lluvia: */
	Vrui::Vector z=application->waterTable->getBaseTransform().inverseTransform(Vrui::Vector(0,0,1));
	Vrui::Vector x=Geometry::normal(z);
	Vrui::Vector y=Geometry::cross(z,x);
	x.normalize();
	y.normalize();
	
	/* Establecer el material de los cilindros de lluvia: */
	GLfloat diffuseCol[4]={0.0f,0.0f,1.0f,0.333f};
	glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,diffuseCol);
	
	/* Renderice las caras posteriores del cilindro de lluvia: */
	glCullFace(GL_FRONT);
	glBegin(GL_QUAD_STRIP);
	for(int i=0;i<=32;++i)
		{
		Vrui::Scalar angle=Vrui::Scalar(2)*Math::Constants<Vrui::Scalar>::pi*Vrui::Scalar(i)/Vrui::Scalar(32);
		glNormal(x*Math::cos(angle)+y*Math::sin(angle));
		glVertex(rainPos+x*(Math::cos(angle)*rainRadius)+y*(Math::sin(angle)*rainRadius));
		glVertex(rainPos+x*(Math::cos(angle)*rainRadius)+y*(Math::sin(angle)*rainRadius)-z*Vrui::Scalar(50));
		}
	glEnd();
	
	/* Renderiza las caras frontales del cilindro de lluvia: */
	glCullFace(GL_BACK);
	glBegin(GL_QUAD_STRIP);
	for(int i=0;i<=32;++i)
		{
		Vrui::Scalar angle=Vrui::Scalar(2)*Math::Constants<Vrui::Scalar>::pi*Vrui::Scalar(i)/Vrui::Scalar(32);
		glNormal(x*Math::cos(angle)+y*Math::sin(angle));
		glVertex(rainPos+x*(Math::cos(angle)*rainRadius)+y*(Math::sin(angle)*rainRadius));
		glVertex(rainPos+x*(Math::cos(angle)*rainRadius)+y*(Math::sin(angle)*rainRadius)-z*Vrui::Scalar(50));
		}
	glEnd();
	glBegin(GL_POLYGON);
	glNormal(z);
	for(int i=0;i<32;++i)
		{
		Vrui::Scalar angle=Vrui::Scalar(2)*Math::Constants<Vrui::Scalar>::pi*Vrui::Scalar(i)/Vrui::Scalar(32);
		glVertex(rainPos+x*(Math::cos(angle)*rainRadius)+y*(Math::sin(angle)*rainRadius));
		}
	glEnd();
	
	glPopMatrix();
	glPopAttrib();
	}

void LocalWaterTool::addWater(GLContextData& contextData) const
	{
	if(adding!=0.0f)
		{
		glPushAttrib(GL_ENABLE_BIT);
		glDisable(GL_CULL_FACE);
		
		/* Obtenga la posición y el tamaño actual del disco de lluvia en las coordenadas de la cámara: */
		Vrui::Point rainPos=Vrui::getInverseNavigationTransformation().transform(getButtonDevicePosition(0));
		Vrui::Scalar rainRadius=Vrui::getPointPickDistance()*Vrui::Scalar(3);
		
		/* Render el disco de lluvia: */
		Vrui::Vector z=application->waterTable->getBaseTransform().inverseTransform(Vrui::Vector(0,0,1));
		Vrui::Vector x=Geometry::normal(z);
		Vrui::Vector y=Geometry::cross(z,x);
		x*=rainRadius/Geometry::mag(x);
		y*=rainRadius/Geometry::mag(y);
		
		glVertexAttrib1fARB(1,adding/application->waterSpeed);
		glBegin(GL_POLYGON);
		for(int i=0;i<32;++i)
			{
			Vrui::Scalar angle=Vrui::Scalar(2)*Math::Constants<Vrui::Scalar>::pi*Vrui::Scalar(i)/Vrui::Scalar(32);
			glVertex(rainPos+x*Math::cos(angle)+y*Math::sin(angle));
			}
		glEnd();
		
		glPopAttrib();
		}
	}
