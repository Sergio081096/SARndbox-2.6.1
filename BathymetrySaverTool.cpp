/***********************************************************************
BathymetrySaverTool: herramienta para guardar la cuadrícula de batimetría 
actual de una caja de arena de realidad aumentada en un archivo o socket 
de red.
Copyright (c) 2016-2018 Oliver Kreylos

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

#include "BathymetrySaverTool.h"

#include <stdexcept>
#include <iomanip>
#include <Misc/PrintInteger.h>
#include <Misc/ThrowStdErr.h>
#include <Misc/MessageLogger.h>
#include <Misc/StandardValueCoders.h>
#include <Misc/ConfigurationFile.h>
#include <IO/ValueSource.h>
#include <IO/OStream.h>
#include <Comm/TCPPipe.h>
#include <Math/Math.h>
#include <Vrui/OpenFile.h>

#include "WaterTable2.h"
#include "Sandbox.h"

/**********************************************************
Methods of class BathymetrySaverToolFactory::Configuration:
**********************************************************/

BathymetrySaverToolFactory::Configuration::Configuration(void)
	:saveFileName("BathymetrySaverTool.dem"),
	 postUpdate(false),postUpdatePort(80),postUpdatePage(""),
	 postUpdateMessage("app.GenerateTileCache();"),
	 gridScale(1.0)
	{
	}

void BathymetrySaverToolFactory::Configuration::read(const Misc::ConfigurationFileSection& cfs)
	{
	saveFileName=cfs.retrieveString("./saveFileName",saveFileName);
	postUpdate=cfs.retrieveValue<bool>("./postUpdate",postUpdate);
	postUpdateHostName=cfs.retrieveString("./postUpdateHostName",postUpdateHostName);
	postUpdatePort=cfs.retrieveValue<int>("./postUpdatePort",postUpdatePort);
	postUpdatePage=cfs.retrieveString("./postUpdatePage",postUpdatePage);
	postUpdateMessage=cfs.retrieveString("./postUpdateMessage",postUpdateMessage);
	gridScale=cfs.retrieveValue<double>("./gridScale",gridScale);
	}

void BathymetrySaverToolFactory::Configuration::write(Misc::ConfigurationFileSection& cfs) const
	{
	cfs.storeString("./saveFileName",saveFileName);
	cfs.storeValue<bool>("./postUpdate",postUpdate);
	cfs.storeString("./postUpdateHostName",postUpdateHostName);
	cfs.storeValue<int>("./postUpdatePort",postUpdatePort);
	cfs.storeString("./postUpdatePage",postUpdatePage);
	cfs.storeString("./postUpdateMessage",postUpdateMessage);
	cfs.storeValue<double>("./gridScale",gridScale);
	}

/*******************************************
Methods of class BathymetrySaverToolFactory:
*******************************************/

BathymetrySaverToolFactory::BathymetrySaverToolFactory(WaterTable2* sWaterTable,Vrui::ToolManager& toolManager)
	:ToolFactory("BathymetrySaverTool",toolManager),
	 waterTable(sWaterTable)
	{
	/* Recuperar la cuadrícula de batimetría y tamaños de celda: */
	for(int i=0;i<2;++i)
		{
		gridSize[i]=waterTable->getBathymetrySize(i);
		cellSize[i]=waterTable->getCellSize()[i];
		}
	
	/* Inicializar diseño de herramienta: */
	layout.setNumButtons(1);
	
	#if 0
	/* Insertar clase en la jerarquía de clases: */
	ToolFactory* toolFactory=toolManager.loadClass("Tool");
	toolFactory->addChildClass(this);
	addParentClass(toolFactory);
	#endif
	
	/* Cargar ajustes de clase: */
	Misc::ConfigurationFileSection cfs=toolManager.getToolClassSection(getClassName());
	configuration.read(cfs);
	
	/* Establecer el puntero de fábrica de la clase de herramienta: */
	BathymetrySaverTool::factory=this;
	}

BathymetrySaverToolFactory::~BathymetrySaverToolFactory(void)
	{
	/* Restablecer el puntero de fábrica de la clase de herramienta: */
	BathymetrySaverTool::factory=0;
	}

const char* BathymetrySaverToolFactory::getName(void) const
	{
	return "Save Bathymetry";
	}

const char* BathymetrySaverToolFactory::getButtonFunction(int) const
	{
	return "Save Bathymetry";
	}

Vrui::Tool* BathymetrySaverToolFactory::createTool(const Vrui::ToolInputAssignment& inputAssignment) const
	{
	return new BathymetrySaverTool(this,inputAssignment);
	}

void BathymetrySaverToolFactory::destroyTool(Vrui::Tool* tool) const
	{
	delete tool;
	}

/********************************************
Static elements of class BathymetrySaverTool:
********************************************/

BathymetrySaverToolFactory* BathymetrySaverTool::factory=0;

/************************************
Methods of class BathymetrySaverTool:
************************************/

namespace {

/****************
Helper functions:
****************/

std::ostream& printInt2(std::ostream& os,int value)
	{
	os<<std::setw(6)<<value;
	return os;
	}

std::ostream& printFloat4(std::ostream& os,double value)
	{
	if(value!=0.0)
		{
		/* Divide el valor en mantisa y exponente: */
		int exponent=int(Math::floor(Math::log10(Math::abs(value))));
		double mantissa=value/Math::pow(10.0,double(exponent));
		
		/* Escribe el numero: */
		std::ios::fmtflags oldFlags=os.flags(std::ios::showpoint|std::ios::dec|std::ios::fixed|std::ios::right);
		char oldFill=os.fill(' ');
		std::streamsize oldPrecision=os.precision(5);
		os<<std::setw(7)<<mantissa<<'e';
		os.setf(std::ios::showpos);
		os.fill('0');
		os<<std::internal<<std::setw(4)<<exponent;
		os.flags(oldFlags);
		os.fill(oldFill);
		os.precision(oldPrecision);
		}
	else
		os<<"0.00000e+000";
	
	return os;
	}

std::ostream& printFloat8(std::ostream& os,double value)
	{
	if(value!=0.0)
		{
		/* Divide el valor en mantisa y exponente: */
		int exponent=int(Math::floor(Math::log10(Math::abs(value))));
		double mantissa=value/Math::pow(10.0,double(exponent));
		
		/* Escribe el numero: */
		std::ios::fmtflags oldFlags=os.flags(std::ios::showpoint|std::ios::dec|std::ios::fixed|std::ios::right);
		char oldFill=os.fill(' ');
		std::streamsize oldPrecision=os.precision(15);
		os<<std::setw(19)<<mantissa<<'D';
		os.setf(std::ios::showpos);
		os.fill('0');
		os<<std::internal<<std::setw(4)<<exponent;
		os.flags(oldFlags);
		os.fill(oldFill);
		os.precision(oldPrecision);
		}
	else
		os<<"  0.000000000000000D+000";
	
	return os;
	}

}

void BathymetrySaverTool::writeDEMFile(void) const
	{
	/* Abra el archivo de salida como un std::ostream: */
	IO::OStream demFile(Vrui::openFile(configuration.saveFileName.c_str(),IO::File::WriteOnly));
	
	/* Escribe el nombre de batimetría: */
	static const char* fileHeader="Augmented Reality Sandbox bathymetry grid";
	demFile<<fileHeader;
	for(size_t i=strlen(fileHeader);i<144;++i)
		demFile<<' ';
	
	/* Escribe la primera parte del encabezado: */
	printInt2(demFile,1); // Código de nivel DEM (DEM-1)
	printInt2(demFile,1); // Patrón de elevación (regular)
	printInt2(demFile,1); // Código del sistema de referencia planimétrico (UTM)
	printInt2(demFile,10); // Zona del sistema de referencia planimétrica (norte de California)
	
	/* Escribir parámetros de proyección de mapas ficticios, porque UTM: */
	for(int i=0;i<15;++i)
		printFloat8(demFile,0.0);
	
	/* Escribir unidades de medida: */
	printInt2(demFile,2); // Unidad horizontal en metros
	printInt2(demFile,2); // Unidad vertical en metros
	
	/* Recuperar el factor de escala de la cuadrícula: */
	double gs=configuration.gridScale;
	
	/* Escriba el polígono de cobertura DEM: */
	printInt2(demFile,4); // El polígono es cuadrángulo
	
	/* Huevo de Pascua: todos los DEM exportados se centran en Davis, CA: */
	static const double gridCenter[2]={609959.0, 4268028.0};
	double west=gridCenter[0]-double(factory->gridSize[0]-1)*double(factory->cellSize[0])*gs*0.5;
	double east=gridCenter[0]+double(factory->gridSize[0]-1)*double(factory->cellSize[0])*gs*0.5;
	double north=gridCenter[1]+double(factory->gridSize[1]-1)*double(factory->cellSize[1])*gs*0.5;
	double south=gridCenter[1]-double(factory->gridSize[1]-1)*double(factory->cellSize[1])*gs*0.5;
	
	/* Recorra el polígono en el sentido de las agujas del reloj, comenzando en la esquina suroeste: */
	printFloat8(demFile,west);
	printFloat8(demFile,south);
	printFloat8(demFile,west);
	printFloat8(demFile,north);
	printFloat8(demFile,east);
	printFloat8(demFile,north);
	printFloat8(demFile,east);
	printFloat8(demFile,south);
	
	/* Calcule y escriba el rango de elevación de la cuadrícula: */
	GLfloat elevMin,elevMax;
	elevMin=elevMax=bathymetryBuffer[0];
	const GLfloat* bbPtr=bathymetryBuffer+1;
	for(GLsizei count=factory->gridSize[1]*factory->gridSize[0]-1;count>0;--count,++bbPtr)
		{
		if(elevMin>*bbPtr)
			elevMin=*bbPtr;
		if(elevMax<*bbPtr)
			elevMax=*bbPtr;
		}
	
	// DEBUGGING
	std::cout<<elevMin<<", "<<elevMax<<std::endl;
	
	elevMin*=gs;
	elevMax*=gs;
	printFloat8(demFile,elevMin);
	printFloat8(demFile,elevMax);
	
	/* Calcule el desplazamiento y la escala de cuantificación de elevación: */
	double elevationBase=0.0; // double(elevMin+elevMax)*0.5;
	double zScale=1000.0; // Cuantización a milímetros por defecto
	double elevRange=Math::max(Math::abs(elevMax-elevationBase),Math::abs(elevMin-elevationBase));
	if(elevRange!=0.0)
		{
		/* Calcule un factor de escala de potencia de diez para escalar el rango real del terreno de -9999 a 9999: */
		zScale=Math::pow(10.0,Math::floor(Math::log10(9999.0/elevRange)));
		}
	
	// DEBUGGING
	// std::cout<<elevationBase<<", "<<zScale<<std::endl;
	// std::cout<<(elevMax-elevationBase)*zScale<<", "<<(elevMin-elevationBase)*zScale<<std::endl;
	
	/* Escribe el ángulo de rotación de la cuadrícula: */
	printFloat8(demFile,0.0);
	
	/* Escribe el código de precisión: */
	printInt2(demFile,0); // Precisión desconocida
	
	/* Escriba las escalas de cuadrícula con total precisión. Por especificación, solo se admiten valores enteros: */
	printFloat4(demFile,factory->cellSize[0]*gs);
	printFloat4(demFile,factory->cellSize[1]*gs);
	printFloat4(demFile,1.0/zScale);
	
	/* Escriba el número de filas y columnas en la cuadrícula: */
	printInt2(demFile,1); // Número de filas especificadas en cada perfil de cuadrícula
	printInt2(demFile,factory->gridSize[0]); // Número de columnas
	
	/* Calcule el tamaño total del archivo escrito hasta el momento: */
	size_t fileSize=864U;
	
	/* Escribe todas las columnas de la cuadrícula: */
	for(GLsizei column=0;column<factory->gridSize[0];++column)
		{
		/* Rellene el tamaño del archivo actual a un múltiplo de 1024: */
		size_t paddedSize=(fileSize+1023U)&~size_t(1023U);
		for(;fileSize<paddedSize;++fileSize)
			demFile<<' ';
		
		/* Escribe el encabezado del perfil: */
		printInt2(demFile,1); // Índice de fila inicial basado en 1 de este perfil
		printInt2(demFile,column+1); // Índice de columna basado en 1 de este perfil
		printInt2(demFile,factory->gridSize[1]); // Número de filas en el perfil
		printInt2(demFile,1); // Número de columnas en el perfil.
		printFloat8(demFile,west+double(column)*double(factory->cellSize[0])*gs); // Este de la primera publicación de elevación en la columna
		printFloat8(demFile,south); // Norte de la primera elevación en la columna.
		printFloat8(demFile,elevationBase); // Elevación local del dato
		
		/* Calcule y escriba el rango de elevación del perfil: */
		const GLfloat* pPtr=bathymetryBuffer+column;
		GLfloat elevMin,elevMax;
		elevMin=elevMax=*pPtr;
		pPtr+=factory->gridSize[0];
		for(GLsizei count=factory->gridSize[1]-1;count>0;--count,pPtr+=factory->gridSize[0])
			{
			if(elevMin>*pPtr)
				elevMin=*pPtr;
			if(elevMax<*pPtr)
				elevMax=*pPtr;
			}
		printFloat8(demFile,elevMin*gs);
		printFloat8(demFile,elevMax*gs);
		
		/* Actualiza el tamaño del archivo: */
		fileSize+=6*4+24*5;
		
		/* Cuantizar y escribir las publicaciones de elevación del perfil: */
		pPtr=bathymetryBuffer+column;
		for(GLsizei count=factory->gridSize[1];count>0;--count,pPtr+=factory->gridSize[0])
			{
			/* Compruebe si queda suficiente espacio en el registro actual de 1024 caracteres: */
			size_t paddedSize=(fileSize+1023U)&~size_t(1023U);
			if(paddedSize-fileSize<10U) // Los últimos cuatro caracteres de cada registro deben estar en blanco
				{
				/* Lleva el registro */
				for(;fileSize<paddedSize;++fileSize)
					demFile<<' ';
				}
			
			/* Cuantizar y escribir la publicación: */
			double scaled=(double(*pPtr)*gs-elevationBase)*zScale;
			printInt2(demFile,int(Math::floor(scaled+0.5)));
			fileSize+=6;
			}
		}
	
	/* Rellene el tamaño del archivo actual a un múltiplo de 1024: */
	size_t paddedSize=(fileSize+1023U)&~size_t(1023U);
	for(;fileSize<paddedSize;++fileSize)
		demFile<<' ';
	
	/* Escribe un dummy "C" record: */
	for(int i=0;i<10;++i)
		printInt2(demFile,0);
	fileSize+=6*10;
	
	/* Rellene el tamaño total del archivo a un múltiplo de 1024: */
	paddedSize=(fileSize+1023U)&~size_t(1023U);
	for(;fileSize<paddedSize;++fileSize)
		demFile<<' ';
	}

void BathymetrySaverTool::postUpdate(void) const
	{
	/* Conéctese al servidor HTTP: */
	Comm::NetPipePtr pipe=new Comm::TCPPipe(configuration.postUpdateHostName.c_str(),configuration.postUpdatePort);
	
	/* Ensamble la solicitud PUT: */
	std::string request;
	request.append("PUT");
	request.push_back(' ');
	request.push_back('/');
	request.append(configuration.postUpdatePage.c_str());
	request.push_back(' ');
	request.append("HTTP/1.1\r\n");
	
	request.append("Host: ");
	request.append(configuration.postUpdateHostName.c_str());
	request.push_back(':');
	char portString[6];
	request.append(Misc::print(configuration.postUpdatePort,portString+5));
	request.append("\r\n");
	
	request.append("Accept: */*\r\n");
	
	request.append("Content-Length: ");
	char contentLengthString[6];
	request.append(Misc::print(configuration.postUpdateMessage.size(),contentLengthString+5));
	request.append("\r\n");
	
	request.append("Content-Type: application/x-www-form-urlencoded\r\n");
	
	/* Termina el encabezado de solicitud: */
	request.append("\r\n");
	
	/* Ensamble el contenido de la solicitud PUT: */
	request.append(configuration.postUpdateMessage);
	
	/* Envíe la solicitud PUT: */
	pipe->writeRaw(request.data(),request.size());
	pipe->flush();
	
	/* Analiza el encabezado de respuesta: */
	bool replyChunked=false;
	bool replySized=false;
	size_t replySize=0;
	{
	/* Adjunte una fuente de valor al conducto para analizar la respuesta del servidor: */
	IO::ValueSource reply(pipe);
	reply.setPunctuation("()<>@,;:\\/[]?={}\r");
	reply.setQuotes("\"");
	reply.skipWs();
	
	/* Lea la línea de estado: */
	if(!reply.isLiteral("HTTP")||!reply.isLiteral('/'))
		Misc::throwStdErr("Not an HTTP reply!");
	reply.skipString();
	unsigned int statusCode=reply.readUnsignedInteger();
	if(statusCode!=200)
		Misc::throwStdErr("HTTP error %d: %s",statusCode,reply.readLine().c_str());
	reply.readLine();
	reply.skipWs();
	
	/* Analizar las opciones de respuesta hasta la primera línea vacía: */
	while(!reply.eof()&&reply.peekc()!='\r')
		{
		/* Lea la etiqueta de opción: */
		std::string option=reply.readString();
		if(reply.isLiteral(':'))
			{
			/* Manejar el valor de la opción: */
			if(option=="Transfer-Encoding")
				{
				/* Analice la lista de codificaciones de transferencia separadas por comas: */
				while(true)
					{
					std::string coding=reply.readString();
					if(coding=="chunked")
						replyChunked=true;
					else
						{
						/* Saltar la extensión de transferencia: */
						while(reply.isLiteral(';'))
							{
							reply.skipString();
							if(!reply.isLiteral('='))
								Misc::throwStdErr("Malformed HTTP reply header");
							reply.skipString();
							}
						}
					if(reply.eof()||reply.peekc()!=',')
						break;
					while(!reply.eof()&&reply.peekc()==',')
						reply.readChar();
					}
				}
			else if(option=="Content-Length")
				{
				replySized=true;
				replySize=reply.readUnsignedInteger();
				}
			}
		
		/* Salta el resto de la línea: */
		reply.skipLine();
		reply.skipWs();
		}
	
	/* Lea el par CR / LF: */
	if(reply.getChar()!='\r'||reply.getChar()!='\n')
		Misc::throwStdErr("Malformed HTTP reply header");
	}
	
	/* Imprima la entidad de respuesta: */
	if(replyChunked)
		{
		// std::cout<<"Chunked reply body!"<<std::endl<<std::endl;
		
		/* Lea todos los fragmentos hasta el fragmento final: */
		while(true)
			{
			/* Lea el siguiente tamaño de trozo: */
			size_t chunkSize=0;
			int digit;
			while(true)
				{
				digit=pipe->getChar();
				if(digit>='0'&&digit<='9')
					chunkSize=(chunkSize<<4)+(digit-'0');
				else if(digit>='a'&&digit<='f')
					chunkSize=(chunkSize<<4)+(digit-'a'+10);
				else if(digit>='A'&&digit<='F')
					chunkSize=(chunkSize<<4)+(digit-'A'+10);
				else
					break;
				}
			while(digit!='\r')
				digit=pipe->getChar();
			if(pipe->getChar()!='\n')
				Misc::throwStdErr("Malformed HTTP chunk header");
			
			if(chunkSize==0)
				break;
			
			/* Lee el trozo: */
			char buffer[256];
			while(chunkSize>0)
				{
				size_t readSize=chunkSize;
				if(readSize>sizeof(buffer))
					readSize=sizeof(buffer);
				pipe->read(buffer,readSize);
				// buffer[readSize]='\0';
				// std::cout<<buffer;
				chunkSize-=readSize;
				}
			
			/* Lea el pie de página del fragmento: */
			if(pipe->getChar()!='\r'||pipe->getChar()!='\n')
				Misc::throwStdErr("Malformed HTTP chunk footer");
			}
		
		/* Saltar el remolque del cuerpo: */
		while(pipe->getChar()!='\r')
			{
			/* Saltarse la línea: */
			while(pipe->getChar()!='\r')
				;
			if(pipe->getChar()!='\n')
				Misc::throwStdErr("Malformed HTTP body trailer");
			}
		if(pipe->getChar()!='\n')
			Misc::throwStdErr("Malformed HTTP body trailer");
		}
	else if(replySized)
		{
		/* Lea el tamaño de respuesta fijo: */
		char buffer[256];
		while(replySize>0)
			{
			size_t readSize=replySize;
			if(readSize>sizeof(buffer))
				readSize=sizeof(buffer);
			pipe->read(buffer,readSize);
			// buffer[readSize]='\0';
			// std::cout<<buffer;
			replySize-=readSize;
			}
		}
	else
		{
		/* Lea hasta el final del archivo:: */
		char buffer[256];
		while(!pipe->eof())
			{
			pipe->readUpTo(buffer,sizeof(buffer));
			// buffer[bufSize]='\0';
			// std::cout<<buffer;
			}
		}
	// std::cout<<std::endl;
	}

BathymetrySaverToolFactory* BathymetrySaverTool::initClass(WaterTable2* sWaterTable,Vrui::ToolManager& toolManager)
	{
	/* Crea la fábrica de herramientas: */
	factory=new BathymetrySaverToolFactory(sWaterTable,toolManager);
	
	/* Regístrese y devuelva la clase: */
	toolManager.addClass(factory,Vrui::ToolManager::defaultToolFactoryDestructor);
	return factory;
	}

BathymetrySaverTool::BathymetrySaverTool(const Vrui::ToolFactory* factory,const Vrui::ToolInputAssignment& inputAssignment)
	:Vrui::Tool(factory,inputAssignment),
	 configuration(BathymetrySaverTool::factory->configuration),
	 bathymetryBuffer(new GLfloat[BathymetrySaverTool::factory->gridSize[1]*BathymetrySaverTool::factory->gridSize[0]]),
	 requestPending(false)
	{
	}

BathymetrySaverTool::~BathymetrySaverTool(void)
	{
	delete[] bathymetryBuffer;
	}

void BathymetrySaverTool::configure(const Misc::ConfigurationFileSection& configFileSection)
	{
	/* Reemplace los datos de configuración privada de la sección del archivo de configuración dado: */
	configuration.read(configFileSection);
	}

void BathymetrySaverTool::storeState(Misc::ConfigurationFileSection& configFileSection) const
	{
	/* Escribir datos de configuración privada en la sección del archivo de configuración dada: */
	configuration.write(configFileSection);
	}

const Vrui::ToolFactory* BathymetrySaverTool::getFactory(void) const
	{
	return factory;
	}

void BathymetrySaverTool::buttonCallback(int buttonSlotIndex,Vrui::InputDevice::ButtonCallbackData* cbData)
	{
	if(cbData->newButtonState)
		{
		/* Solicite una rejilla de batimetría de la capa freática: */
		requestPending=factory->waterTable->requestBathymetry(bathymetryBuffer);
		}
	}

void BathymetrySaverTool::frame(void)
	{
	if(requestPending&&factory->waterTable->haveBathymetry())
		{
		try
			{
			/* Exportar la red de batimetría: */
			writeDEMFile();
			
			if(configuration.postUpdate)
				{
				/* Envíe un mensaje de actualización al servidor web configurado: */
				postUpdate();
				}
			}
		catch(const std::runtime_error& err)
			{
			Misc::formattedUserError("Save Bathymetry: Unable to save bathymetry due to exception \"%s\"",err.what());
			}
		
		requestPending=false;
		}
	}
