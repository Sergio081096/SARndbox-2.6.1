/***********************************************************************
GlobalWaterTool: Clase de herramienta para agregar o eliminar agua de 
una caja de arena de realidad aumentada a nivel mundial.
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

#include "GlobalWaterTool.h"

#include <Vrui/ToolManager.h>

#include "WaterTable2.h"
#include "Sandbox.h"
#include <iostream>

/****************************************
Static elements of class GlobalWaterTool:
****************************************/

GlobalWaterToolFactory* GlobalWaterTool::factory=0;

/********************************
Methods of class GlobalWaterTool:
********************************/

GlobalWaterToolFactory* GlobalWaterTool::initClass(Vrui::ToolManager& toolManager)
	{
	/* Crear la fabrica de herramientas: */
	factory=new GlobalWaterToolFactory("GlobalWaterTool","Manage Water",0,toolManager);
	
	/* Configure el diseño de entrada de la clase de herramienta: */
	factory->setNumButtons(2);
	factory->setButtonFunction(0,"Rain");
	factory->setButtonFunction(1,"Dry");
	
	/* Registro y retorno de la clase: */
	toolManager.addClass(factory,Vrui::ToolManager::defaultToolFactoryDestructor);
	return factory;
	}

GlobalWaterTool::GlobalWaterTool(const Vrui::ToolFactory* factory,const Vrui::ToolInputAssignment& inputAssignment)
	:Vrui::Tool(factory,inputAssignment)
	{
	}

GlobalWaterTool::~GlobalWaterTool(void)
	{
	}

const Vrui::ToolFactory* GlobalWaterTool::getFactory(void) const
	{
	return factory;
	}

void GlobalWaterTool::buttonCallback(int buttonSlotIndex,Vrui::InputDevice::ButtonCallbackData* cbData)
{
	/* Calcule la cantidad de agua para agregar o eliminar: */
	float waterAmount;
	if(cbData->newButtonState) // El botón se acaba de presionar
	{
		/* Agregar una cantidad fija de agua/segundo: */
		waterAmount=application->waterSpeed>0.0?application->rainStrength/application->waterSpeed:0.0f;
		
		/* Invertir la cantidad de agua para el botón de drenaje: */
		if(buttonSlotIndex==1)
		{
			waterAmount=-waterAmount;
		}
		if(buttonSlotIndex != 1)
			application->waterCallback(false);
		
	
		//std::cout<<"Boton 1->"<< buttonSlotIndex<<std::endl;	
		/* Recuerde la cantidad para el evento de lanzamiento de botón: */
		waterAmounts[buttonSlotIndex]=waterAmount;
	}
	else // Botón acaba de ser lanzado
	{
		/* Invertir la cantidad de agua agregada cuando se presionó el botón: */
		//std::cout<<"Boton 2->"<< buttonSlotIndex<<std::endl;	
		waterAmount=-waterAmounts[buttonSlotIndex];
	}
	
	/* Agregue la cantidad de agua a la mesa de agua: */
	application->waterTable->setWaterDeposit(application->waterTable->getWaterDeposit()+waterAmount);
}
