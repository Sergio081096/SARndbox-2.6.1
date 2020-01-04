/***********************************************************************
Sandbox - Aplicación Vrui para conducir un sandbox de realidad aumentada.
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

#include "Sandbox.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <Misc/SizedTypes.h>
#include <Misc/SelfDestructPointer.h>
#include <Misc/FixedArray.h>
#include <Misc/FunctionCalls.h>
#include <Misc/FileNameExtensions.h>
#include <Misc/StandardValueCoders.h>
#include <Misc/ArrayValueCoders.h>
#include <Misc/ConfigurationFile.h>
#include <IO/File.h>
#include <IO/ValueSource.h>
#include <Cluster/OpenPipe.h>
#include <Math/Math.h>
#include <Math/Constants.h>
#include <Math/Interval.h>
#include <Math/MathValueCoders.h>
#include <Geometry/Point.h>
#include <Geometry/AffineCombiner.h>
#include <Geometry/HVector.h>
#include <Geometry/Plane.h>
#include <Geometry/LinearUnit.h>
#include <Geometry/GeometryValueCoders.h>
#include <Geometry/OutputOperators.h>
#include <GL/gl.h>
#include <GL/GLMaterialTemplates.h>
#include <GL/GLColorMap.h>
#include <GL/GLLightTracker.h>
#include <GL/Extensions/GLEXTFramebufferObject.h>
#include <GL/Extensions/GLARBTextureRectangle.h>
#include <GL/Extensions/GLARBTextureFloat.h>
#include <GL/Extensions/GLARBTextureRg.h>
#include <GL/Extensions/GLARBDepthTexture.h>
#include <GL/Extensions/GLARBShaderObjects.h>
#include <GL/Extensions/GLARBVertexShader.h>
#include <GL/Extensions/GLARBFragmentShader.h>
#include <GL/Extensions/GLARBMultitexture.h>
#include <GL/GLContextData.h>
#include <GL/GLGeometryWrappers.h>
#include <GL/GLTransformationWrappers.h>
#include <GLMotif/StyleSheet.h>
#include <GLMotif/WidgetManager.h>
#include <GLMotif/PopupMenu.h>
#include <GLMotif/Menu.h>
#include <GLMotif/PopupWindow.h>
#include <GLMotif/Margin.h>
#include <GLMotif/Label.h>
#include <GLMotif/TextField.h>
#include <Vrui/Vrui.h>
#include <Vrui/CoordinateManager.h>
#include <Vrui/Lightsource.h>
#include <Vrui/LightsourceManager.h>
#include <Vrui/Viewer.h>
#include <Vrui/ToolManager.h>
#include <Vrui/DisplayState.h>
#include <Vrui/OpenFile.h>
#include <Kinect/FileFrameSource.h>
#include <Kinect/MultiplexedFrameSource.h>
#include <Kinect/DirectFrameSource.h>
#include <Kinect/OpenDirectFrameSource.h>

#define SAVEDEPTH 0

#if SAVEDEPTH
#include <Images/RGBImage.h>
#include <Images/WriteImageFile.h>
#endif

#include "FrameFilter.h"
#include "DepthImageRenderer.h"
#include "ElevationColorMap.h"
#include "DEM.h"
#include "SurfaceRenderer.h"
#include "WaterTable2.h"
#include "HandExtractor.h"
#include "WaterRenderer.h"
#include "GlobalWaterTool.h"
#include "LocalWaterTool.h"
#include "DEMTool.h"
#include "BathymetrySaverTool.h"

#include "Config.h"

/**********************************
Methods of class Sandbox::DataItem:
**********************************/

Sandbox::DataItem::DataItem(void)
	:waterTableTime(0.0),
	 shadowFramebufferObject(0),shadowDepthTextureObject(0)
	{
	/* Compruebe si todas las extensiones requeridas son compatibles: */
	std::cout<<"DataItem"<<std::endl;	
	bool supported=GLEXTFramebufferObject::isSupported();
	supported=supported&&GLARBTextureRectangle::isSupported();
	supported=supported&&GLARBTextureFloat::isSupported();
	supported=supported&&GLARBTextureRg::isSupported();
	supported=supported&&GLARBDepthTexture::isSupported();
	supported=supported&&GLARBShaderObjects::isSupported();
	supported=supported&&GLARBVertexShader::isSupported();
	supported=supported&&GLARBFragmentShader::isSupported();
	supported=supported&&GLARBMultitexture::isSupported();
	if(!supported)
		Misc::throwStdErr("Sandbox: Not all required extensions are supported by local OpenGL");
	
	/* Inicialice todas las extensiones requeridas: */
	GLEXTFramebufferObject::initExtension();
	GLARBTextureRectangle::initExtension();
	GLARBTextureFloat::initExtension();
	GLARBTextureRg::initExtension();
	GLARBDepthTexture::initExtension();
	GLARBShaderObjects::initExtension();
	GLARBVertexShader::initExtension();
	GLARBFragmentShader::initExtension();
	GLARBMultitexture::initExtension();
	}

Sandbox::DataItem::~DataItem(void)
	{
	/* Eliminar todos los shaders, buffers, and texture objects: */
	std::cout<<"~DataItem"<<std::endl;
	glDeleteFramebuffersEXT(1,&shadowFramebufferObject);
	glDeleteTextures(1,&shadowDepthTextureObject);
	}

/****************************************
Methods of class Sandbox::RenderSettings:
****************************************/

Sandbox::RenderSettings::RenderSettings(bool line)
	:fixProjectorView(false),
	 projectorTransform(PTransform::identity),
	 projectorTransformValid(false),
	 hillshade(false),
	 surfaceMaterial(GLMaterial::Color(1.0f,1.0f,1.0f)),
	 useShadows(false),
	 elevationColorMap(0),
	 useContourLines(line), // Activar/Desactivar curvas de nivel
	 contourLineSpacing(0.75f),
	 renderWaterSurface(false),
	 waterOpacity(2.0f),
	 surfaceRenderer(0),
	 waterRenderer(0)
	{
	/* Cargue la transformación por defecto del proyector: */
	std::cout<<"1: Cargando Proyector-> "<< CONFIG_DEFAULTPROJECTIONMATRIXFILENAME <<std::endl;
	loadProjectorTransform(CONFIG_DEFAULTPROJECTIONMATRIXFILENAME);
	}

Sandbox::RenderSettings::RenderSettings(const Sandbox::RenderSettings& source) // Apuntador
	:fixProjectorView(source.fixProjectorView),
	 projectorTransform(source.projectorTransform),
	 projectorTransformValid(source.projectorTransformValid),
	 hillshade(source.hillshade),
	 surfaceMaterial(source.surfaceMaterial),
	 useShadows(source.useShadows),
	 elevationColorMap(source.elevationColorMap!=0?new ElevationColorMap(*source.elevationColorMap):0),
	 useContourLines(source.useContourLines),
	 contourLineSpacing(source.contourLineSpacing),
	 renderWaterSurface(source.renderWaterSurface),
	 waterOpacity(source.waterOpacity),
	 surfaceRenderer(0),
	 waterRenderer(0)
	{

		std::cout<<"3: Cargando Render del Apuntador"<<std::endl;	
	}//*/

Sandbox::RenderSettings::~RenderSettings(void)
	{
	delete surfaceRenderer;
	delete waterRenderer;
	delete elevationColorMap;
	}

void Sandbox::RenderSettings::loadProjectorTransform(const char* projectorTransformName)
	{
	std::string fullProjectorTransformName;
	try
		{
		/* Abra el archivo de transformación del proyector: */
		if(projectorTransformName[0]=='/')
			{
			/* Utilice el nombre absoluto del archivo directamente: */
			fullProjectorTransformName=projectorTransformName;
			}
		else
			{
			/* Arme un nombre de archivo relativo al directorio de archivos de configuración: */
			fullProjectorTransformName=CONFIG_CONFIGDIR;
			fullProjectorTransformName.push_back('/');
			fullProjectorTransformName.append(projectorTransformName);
			}
		std::cerr<<"2: Archivo Proyector-> " << fullProjectorTransformName << std::endl;
		IO::FilePtr projectorTransformFile=Vrui::openFile(fullProjectorTransformName.c_str(),IO::File::ReadOnly);
		projectorTransformFile->setEndianness(Misc::LittleEndian);
		
		/* Lea la matriz de transformación del proyector desde el archivo binario: */
		Misc::Float64 pt[16];
		projectorTransformFile->read(pt,16);
		projectorTransform=PTransform::fromRowMajor(pt);
		
		projectorTransformValid=true;
		}
	catch(const std::runtime_error& err)
		{
		/* Imprima un mensaje de error y desactive las proyecciones calibradas: */
		//std::cerr<<"No se puede cargar la transformación del proyector desde el archivo "<<fullProjectorTransformName<<" due to exception "<<err.what()<<std::endl;
		projectorTransformValid=false;
		}
	}

void Sandbox::RenderSettings::loadHeightMap(const char* heightMapName)
	{
	try
	{
		/* Cargue el mapa de color de elevación del nombre dado: */		
		ElevationColorMap* newElevationColorMap=new ElevationColorMap(heightMapName);		
		/* Eliminar el mapa de color de elevación anterior y asignar el nuevo: */
		delete elevationColorMap;
		elevationColorMap = newElevationColorMap;
		std::cout<<"5.4: Cargando Mapa-> "<< elevationColorMap <<std::endl;
	}
	catch(const std::runtime_error& err)
		{
			std::cerr<<"Ignorando el mapa de altura debido a una excepción "<<err.what()<<std::endl;
		}
	}

/************************
Methods of class Sandbox:
************************/

void Sandbox::rawDepthFrameDispatcher(const Kinect::FrameBuffer& frameBuffer)
	{
	/* Pase el cuadro recibido al filtro de cuadro y al extractor manual: */
	std::cout<<"123: rawDepthFrameDispatcher" <<std::endl;
	if(frameFilter!=0 && !pauseUpdates)
		frameFilter->receiveRawFrame(frameBuffer);
	if(handExtractor!=0)
		handExtractor->receiveRawFrame(frameBuffer);
	}

void Sandbox::receiveFilteredFrame(const Kinect::FrameBuffer& frameBuffer)
	{
	/* Coloque el nuevo marco en el búfer de entrada de marco: */
	std::cout<<"receiveFilteredFrame"<<std::endl;
	filteredFrames.postNewValue(frameBuffer);
	
	/* Despierta el hilo de primer plano: */
	Vrui::requestUpdate();
	}

void Sandbox::toggleDEM(DEM* dem)
	{
	/* Compruebe si este es el DEM activo: */
	std::cout<<"toggleDEM"<<std::endl;
	if(activeDem==dem)
		{
		/* Desactivar el DEM actualmente activo: */
		activeDem=0;
		}
	else
		{
		/* Activar este DEM: */
		activeDem=dem;
		}
	
	/* Habilite la coincidencia de DEM en todos los renderizadores de superficie que usan una matriz de proyector fija,
	   es decir, en todos los espacios de arena físicos: */
	for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
		if(rsIt->fixProjectorView)
			rsIt->surfaceRenderer->setDem(activeDem);
	}

void Sandbox::addWater(GLContextData& contextData) const
	{
	/* Compruebe si la lista de objetos de lluvia más reciente no está vacía: */
	std::cout<<"addWater"<<std::endl;
	if(handExtractor!=0 && !handExtractor->getLockedExtractedHands().empty())
		{
		/* Renderiza todos los objetos de lluvia al nivel de la mesa: */
		glPushAttrib(GL_ENABLE_BIT);
		glDisable(GL_CULL_FACE);
		
		/* Cree un marco de coordenadas locales para renderizar discos de lluvia: */
		Vector z=waterTable->getBaseTransform().inverseTransform(Vector(0,0,1));
		Vector x=Geometry::normal(z);
		Vector y=Geometry::cross(z,x);
		x.normalize();
		y.normalize();
		
		glVertexAttrib1fARB(1,rainStrength/waterSpeed);
		for(HandExtractor::HandList::const_iterator hIt=handExtractor->getLockedExtractedHands().begin();hIt!=handExtractor->getLockedExtractedHands().end();++hIt)
		{
			/* Renderiza un disco de lluvia aproximando una mano: */
			glBegin(GL_POLYGON);
			for(int i=0;i<32;++i)
				{
				Scalar angle=Scalar(2)*Math::Constants<Scalar>::pi*Scalar(i)/Scalar(32);
				glVertex(hIt->center+x*(Math::cos(angle)*hIt->radius*0.75)+y*(Math::sin(angle)*hIt->radius*0.75));
				}
			glEnd();
		}
		
		glPopAttrib();
		}
	}

void Sandbox::pauseUpdatesCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData)
	{
		//std::cout<<"pause"<<std::endl;
		//renderSettings.back().useContourLines=false;
		pauseUpdates=cbData->set;
	}

void Sandbox::pauseLineCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData)
	{
		//std::cout<<"pause"<<std::endl;
		//renderSettings.back().useContourLines=false;
		pauseLine=cbData->set;
	}

void Sandbox::showWaterControlDialogCallback(Misc::CallbackData* cbData)
	{
		//std::cout<<"Control"<<std::endl;
		Vrui::popupPrimaryWidget(waterControlDialog);
	}

void Sandbox::waterSpeedSliderCallback(GLMotif::TextFieldSlider::ValueChangedCallbackData* cbData)
	{
		waterSpeed=cbData->value;
	}

void Sandbox::waterMaxStepsSliderCallback(GLMotif::TextFieldSlider::ValueChangedCallbackData* cbData)
	{
		waterMaxSteps=int(Math::floor(cbData->value+0.5));
	}

void Sandbox::waterAttenuationSliderCallback(GLMotif::TextFieldSlider::ValueChangedCallbackData* cbData)
	{
		waterTable->setAttenuation(GLfloat(1.0-cbData->value));
	}

GLMotif::PopupMenu* Sandbox::createMainMenu(void)
	{
	/* Cree un shell emergente para mantener el menú principal: */
	std::cout<<"MainMenu"<<std::endl;
	GLMotif::PopupMenu* mainMenuPopup=new GLMotif::PopupMenu("MainMenuPopup",Vrui::getWidgetManager());
	mainMenuPopup->setTitle("AR Sandbox");
	//Sandbox::glutKeyboardFunc (  'w', 1, 1  );
	
	/* Create the main menu principal: */
	GLMotif::Menu* mainMenu=new GLMotif::Menu("MainMenu",mainMenuPopup,false);
	
	/* Crea un botón para pausar las actualizaciones de topografía: */
	pauseUpdatesLine=new GLMotif::ToggleButton("PauseUpdatesLine",mainMenu,"Pause Line");
	pauseUpdatesLine->setToggle(false);
	pauseUpdatesLine->getValueChangedCallbacks().add(this,&Sandbox::pauseLineCallback);

	pauseUpdatesToggle=new GLMotif::ToggleButton("PauseUpdatesToggle",mainMenu,"Pause Topography");	
	pauseUpdatesToggle->setToggle(false);
	pauseUpdatesToggle->getValueChangedCallbacks().add(this,&Sandbox::pauseUpdatesCallback);

	

	//pauseUpdatesToggle->getValueChangedCallbacks().add(this,&Sandbox::pauseUpdatesCallback);
	
	if(waterTable!=0)
		{
		/* Crea un botón para mostrar el diálogo de control de agua: */
		GLMotif::Button* showWaterControlDialogButton=new GLMotif::Button("ShowWaterControlDialogButton",mainMenu,"Show Water Simulation Control");
		showWaterControlDialogButton->getSelectCallbacks().add(this,&Sandbox::showWaterControlDialogCallback);
		}
	
	/* Finish building the main menu: */
	mainMenu->manageChild();
	
	return mainMenuPopup;
	}

GLMotif::PopupWindow* Sandbox::createWaterControlDialog(void)
	{
	std::cout<<"createWaterControlDialog"<<std::endl;
	const GLMotif::StyleSheet& ss=*Vrui::getWidgetManager()->getStyleSheet();
	
	/* Create a popup window shell: */
	GLMotif::PopupWindow* waterControlDialogPopup=new GLMotif::PopupWindow("WaterControlDialogPopup",Vrui::getWidgetManager(),"Water Simulation Control");
	waterControlDialogPopup->setCloseButton(true);
	waterControlDialogPopup->setResizableFlags(true,false);
	waterControlDialogPopup->popDownOnClose();
	
	GLMotif::RowColumn* waterControlDialog=new GLMotif::RowColumn("WaterControlDialog",waterControlDialogPopup,false);
	waterControlDialog->setOrientation(GLMotif::RowColumn::VERTICAL);
	waterControlDialog->setPacking(GLMotif::RowColumn::PACK_TIGHT);
	waterControlDialog->setNumMinorWidgets(2);
	
	new GLMotif::Label("WaterSpeedLabel",waterControlDialog,"Speed");
	
	waterSpeedSlider=new GLMotif::TextFieldSlider("WaterSpeedSlider",waterControlDialog,8,ss.fontHeight*10.0f);
	waterSpeedSlider->getTextField()->setFieldWidth(7);
	waterSpeedSlider->getTextField()->setPrecision(4);
	waterSpeedSlider->getTextField()->setFloatFormat(GLMotif::TextField::SMART);
	waterSpeedSlider->setSliderMapping(GLMotif::TextFieldSlider::EXP10);
	waterSpeedSlider->setValueRange(0.001,10.0,0.05);
	waterSpeedSlider->getSlider()->addNotch(0.0f);
	waterSpeedSlider->setValue(waterSpeed);
	waterSpeedSlider->getValueChangedCallbacks().add(this,&Sandbox::waterSpeedSliderCallback);
	
	new GLMotif::Label("WaterMaxStepsLabel",waterControlDialog,"Max Steps");
	
	waterMaxStepsSlider=new GLMotif::TextFieldSlider("WaterMaxStepsSlider",waterControlDialog,8,ss.fontHeight*10.0f);
	waterMaxStepsSlider->getTextField()->setFieldWidth(7);
	waterMaxStepsSlider->getTextField()->setPrecision(0);
	waterMaxStepsSlider->getTextField()->setFloatFormat(GLMotif::TextField::FIXED);
	waterMaxStepsSlider->setSliderMapping(GLMotif::TextFieldSlider::LINEAR);
	waterMaxStepsSlider->setValueType(GLMotif::TextFieldSlider::UINT);
	waterMaxStepsSlider->setValueRange(0,200,1);
	waterMaxStepsSlider->setValue(waterMaxSteps);
	waterMaxStepsSlider->getValueChangedCallbacks().add(this,&Sandbox::waterMaxStepsSliderCallback);
	
	new GLMotif::Label("FrameRateLabel",waterControlDialog,"Frame Rate");
	
	GLMotif::Margin* frameRateMargin=new GLMotif::Margin("FrameRateMargin",waterControlDialog,false);
	frameRateMargin->setAlignment(GLMotif::Alignment::LEFT);
	
	frameRateTextField=new GLMotif::TextField("FrameRateTextField",frameRateMargin,8);
	frameRateTextField->setFieldWidth(7);
	frameRateTextField->setPrecision(2);
	frameRateTextField->setFloatFormat(GLMotif::TextField::FIXED);
	frameRateTextField->setValue(0.0);
	
	frameRateMargin->manageChild();
	
	new GLMotif::Label("WaterAttenuationLabel",waterControlDialog,"Attenuation");
	
	waterAttenuationSlider=new GLMotif::TextFieldSlider("WaterAttenuationSlider",waterControlDialog,8,ss.fontHeight*10.0f);
	waterAttenuationSlider->getTextField()->setFieldWidth(7);
	waterAttenuationSlider->getTextField()->setPrecision(5);
	waterAttenuationSlider->getTextField()->setFloatFormat(GLMotif::TextField::SMART);
	waterAttenuationSlider->setSliderMapping(GLMotif::TextFieldSlider::EXP10);
	waterAttenuationSlider->setValueRange(0.001,1.0,0.01);
	waterAttenuationSlider->getSlider()->addNotch(Math::log10(1.0-double(waterTable->getAttenuation())));
	waterAttenuationSlider->setValue(1.0-double(waterTable->getAttenuation()));
	waterAttenuationSlider->getValueChangedCallbacks().add(this,&Sandbox::waterAttenuationSliderCallback);
	
	waterControlDialog->manageChild();
	
	return waterControlDialogPopup;
	}

namespace {

/****************
Helper functions:
****************/

void printUsage(void)
	{
	std::cout<<"Usage: SARndbox [option 1] ... [option n]"<<std::endl;
	std::cout<<"  Options:"<<std::endl;
	std::cout<<"  -h"<<std::endl;
	std::cout<<"     Imprime este mensaje de ayuda"<<std::endl;
	std::cout<<"  -c <índice de cámara>"<<std::endl;
	std::cout<<"     Selecciona la cámara 3D local del índice dado (0: primera cámara"<<std::endl;
	std::cout<<"     en el USB bus)"<<std::endl;
	std::cout<<"     Default: 0"<<std::endl;
	std::cout<<"  -f <frame file name prefix>"<<std::endl;
	std::cout<<"     Reads a pre-recorded 3D video stream from a pair of color/depth"<<std::endl;
	std::cout<<"     files of the given file name prefix"<<std::endl;
	std::cout<<"  -s <scale factor>"<<std::endl;
	std::cout<<"     Scale factor from real sandbox to simulated terrain"<<std::endl;
	std::cout<<"     Default: 100.0 (1:100 scale, 1cm in sandbox is 1m in terrain"<<std::endl;
	std::cout<<"  -slf <sandbox layout file name>"<<std::endl;
	std::cout<<"     Loads the sandbox layout file of the given name"<<std::endl;
	std::cout<<"     Default: "<<CONFIG_CONFIGDIR<<'/'<<CONFIG_DEFAULTBOXLAYOUTFILENAME<<std::endl;
	std::cout<<"  -er <min elevation> <max elevation>"<<std::endl;
	std::cout<<"     Sets the range of valid sand surface elevations relative to the"<<std::endl;
	std::cout<<"     ground plane in cm"<<std::endl;
	std::cout<<"     Default: Range of elevation color map"<<std::endl;
	std::cout<<"  -hmp <x> <y> <z> <offset>"<<std::endl;
	std::cout<<"     Sets an explicit base plane equation to use for height color mapping"<<std::endl;
	std::cout<<"  -nas <num averaging slots>"<<std::endl;
	std::cout<<"     Sets the number of averaging slots in the frame filter; latency is"<<std::endl;
	std::cout<<"     <num averaging slots> * 1/30 s"<<std::endl;
	std::cout<<"     Default: 30"<<std::endl;
	std::cout<<"  -sp <min num samples> <max variance>"<<std::endl;
	std::cout<<"     Sets the frame filter parameters minimum number of valid samples"<<std::endl;
	std::cout<<"     and maximum sample variance before convergence"<<std::endl;
	std::cout<<"     Default: 10 2"<<std::endl;
	std::cout<<"  -he <hysteresis envelope>"<<std::endl;
	std::cout<<"     Sets the size of the hysteresis envelope used for jitter removal"<<std::endl;
	std::cout<<"     Default: 0.1"<<std::endl;
	std::cout<<"  -wts <water grid width> <water grid height>"<<std::endl;
	std::cout<<"     Sets the width and height of the water flow simulation grid"<<std::endl;
	std::cout<<"     Default: 640 480"<<std::endl;
	std::cout<<"  -ws <water speed> <water max steps>"<<std::endl;
	std::cout<<"     Sets the relative speed of the water simulation and the maximum"<<std::endl;
	std::cout<<"     number of simulation steps per frame"<<std::endl;
	std::cout<<"     Default: 1.0 30"<<std::endl;
	std::cout<<"  -rer <min rain elevation> <max rain elevation>"<<std::endl;
	std::cout<<"     Sets the elevation range of the rain cloud level relative to the"<<std::endl;
	std::cout<<"     ground plane in cm"<<std::endl;
	std::cout<<"     Default: Above range of elevation color map"<<std::endl;
	std::cout<<"  -rs <rain strength>"<<std::endl;
	std::cout<<"     Sets the strength of global or local rainfall in cm/s"<<std::endl;
	std::cout<<"     Default: 0.25"<<std::endl;
	std::cout<<"  -evr <evaporation rate>"<<std::endl;
	std::cout<<"     Water evaporation rate in cm/s"<<std::endl;
	std::cout<<"     Default: 0.0"<<std::endl;
	std::cout<<"  -dds <DEM distance scale>"<<std::endl;
	std::cout<<"     DEM matching distance scale factor in cm"<<std::endl;
	std::cout<<"     Default: 1.0"<<std::endl;
	std::cout<<"  -wi <window index>"<<std::endl;
	std::cout<<"     Sets the zero-based index of the display window to which the"<<std::endl;
	std::cout<<"     following rendering settings are applied"<<std::endl;
	std::cout<<"     Default: 0"<<std::endl;
	std::cout<<"  -fpv [projector transform file name]"<<std::endl;
	std::cout<<"     Fixes the navigation transformation so that Kinect camera and"<<std::endl;
	std::cout<<"     projector are aligned, as defined by the projector transform file"<<std::endl;
	std::cout<<"     of the given name"<<std::endl;
	std::cout<<"     Default projector transform file name: "<<CONFIG_CONFIGDIR<<'/'<<CONFIG_DEFAULTPROJECTIONMATRIXFILENAME<<std::endl;
	std::cout<<"  -nhs"<<std::endl;
	std::cout<<"     Disables hill shading"<<std::endl;
	std::cout<<"  -uhs"<<std::endl;
	std::cout<<"     Enables hill shading"<<std::endl;
	std::cout<<"  -ns"<<std::endl;
	std::cout<<"     Disables shadows"<<std::endl;
	std::cout<<"  -us"<<std::endl;
	std::cout<<"     Enables shadows"<<std::endl;
	std::cout<<"  -nhm"<<std::endl;
	std::cout<<"     Disables elevation color mapping"<<std::endl;
	std::cout<<"  -uhm [nombre del archivo de mapa de color de elevación]"<<std::endl;
	std::cout<<"     Habilita la asignación de color de elevación y carga el mapa de color de elevación desde el archivo"<<std::endl;
	std::cout<<"     del nombre dado"<<std::endl;
	std::cout<<"     Default elevation color  map file name: "<<CONFIG_CONFIGDIR<<'/'<<CONFIG_DEFAULTHEIGHTCOLORMAPFILENAME<<std::endl;
	std::cout<<"  -ncl"<<std::endl;
	std::cout<<"     Disables topographic contour lines"<<std::endl;
	std::cout<<"  -ucl [contour line spacing]"<<std::endl;
	std::cout<<"     Enables topographic contour lines and sets the elevation distance between"<<std::endl;
	std::cout<<"     adjacent contour lines to the given value in cm"<<std::endl;
	std::cout<<"     Default contour line spacing: 0.75"<<std::endl;
	std::cout<<"  -rws"<<std::endl;
	std::cout<<"     Renders water surface as geometric surface"<<std::endl;
	std::cout<<"  -rwt"<<std::endl;
	std::cout<<"     Renders water surface as texture"<<std::endl;
	std::cout<<"  -wo <water opacity>"<<std::endl;
	std::cout<<"     Sets the water depth at which water appears opaque in cm"<<std::endl;
	std::cout<<"     Default: 2.0"<<std::endl;
	std::cout<<"  -cp <control pipe name>"<<std::endl;
	std::cout<<"     Sets the name of a named POSIX pipe from which to read control commands"<<std::endl;
	}

}

Sandbox::Sandbox(int& argc,char**& argv):Vrui::Application(argc,argv),
	 camera(0),
	 pixelDepthCorrection(0),
	 frameFilter(0),
	 pauseUpdates(false),
	 pauseLine(true),
	 depthImageRenderer(0),
	 waterTable(0),
	 handExtractor(0),
	 addWaterFunction(0),
	 addWaterFunctionRegistered(false),
	 sun(0),
	 activeDem(0),
	 mainMenu(0),
	 pauseUpdatesToggle(0),
	 pauseUpdatesLine(0),
	 waterControlDialog(0),
	 waterSpeedSlider(0),
	 waterMaxStepsSlider(0),
	 frameRateTextField(0),
	 waterAttenuationSlider(0),
	 controlPipeFd(-1)
	{
	/* Lea los parámetros de configuración predeterminados del sandbox: */
	std::cout<<"Main " << std::endl;
	std::string sandboxConfigFileName=CONFIG_CONFIGDIR;
	sandboxConfigFileName.push_back('/');
	sandboxConfigFileName.append(CONFIG_DEFAULTCONFIGFILENAME);
	std::cout<<"Obtener la ruta de SARndbox.cfg-> "<<sandboxConfigFileName<<std::endl;
	Misc::ConfigurationFile sandboxConfigFile(sandboxConfigFileName.c_str());
	Misc::ConfigurationFileSection cfg=sandboxConfigFile.getSection("/SARndbox");
	unsigned int cameraIndex=cfg.retrieveValue<int>("./cameraIndex",0);
	std::string cameraConfiguration=cfg.retrieveString("./cameraConfiguration","Camera");
	double scale=cfg.retrieveValue<double>("./scaleFactor",100.0);
	std::string sandboxLayoutFileName=CONFIG_CONFIGDIR;
	sandboxLayoutFileName.push_back('/');
	sandboxLayoutFileName.append(CONFIG_DEFAULTBOXLAYOUTFILENAME);
	std::cout<<"Obtener la ruta de BoxLayout.txt-> "<<sandboxLayoutFileName<<std::endl;
	sandboxLayoutFileName=cfg.retrieveString("./sandboxLayoutFileName",sandboxLayoutFileName);
	Math::Interval<double> elevationRange=cfg.retrieveValue<Math::Interval<double> >("./elevationRange",Math::Interval<double>(-1000.0,1000.0));
	bool haveHeightMapPlane=cfg.hasTag("./heightMapPlane");
	Plane heightMapPlane;
	if(haveHeightMapPlane)
	{
		heightMapPlane=cfg.retrieveValue<Plane>("./heightMapPlane");//No entra
	}
	unsigned int numAveragingSlots=cfg.retrieveValue<unsigned int>("./numAveragingSlots",30);
	unsigned int minNumSamples=cfg.retrieveValue<unsigned int>("./minNumSamples",10);
	unsigned int maxVariance=cfg.retrieveValue<unsigned int>("./maxVariance",2);
	float hysteresis=cfg.retrieveValue<float>("./hysteresis",0.1f);
	Misc::FixedArray<unsigned int,2> wtSize;
	wtSize[0]=640;
	wtSize[1]=480;
	wtSize=cfg.retrieveValue<Misc::FixedArray<unsigned int,2> >("./waterTableSize",wtSize);
	waterSpeed=cfg.retrieveValue<double>("./waterSpeed",1.0);
	waterMaxSteps=cfg.retrieveValue<unsigned int>("./waterMaxSteps",30U);
	Math::Interval<double> rainElevationRange=cfg.retrieveValue<Math::Interval<double> >("./rainElevationRange",Math::Interval<double>(-1000.0,1000.0));
	rainStrength=cfg.retrieveValue<GLfloat>("./rainStrength",0.25f);
	double evaporationRate=cfg.retrieveValue<double>("./evaporationRate",0.0);
	float demDistScale=cfg.retrieveValue<float>("./demDistScale",1.0f);
	std::string controlPipeName=cfg.retrieveString("./controlPipeName","");
	
	/* Procesar los parámetros de la línea de comando: */
	bool printHelp=false;
	const char* frameFilePrefix=0;
	const char* kinectServerName=0;
	int windowIndex=0;
	bool line = true;//Curvas de nivel
	//std::cin >> line;
	renderSettings.push_back(RenderSettings(line));
	for(int i=1;i<argc;++i)
		{
		if(argv[i][0]=='-')
			{
			if(strcasecmp(argv[i]+1,"h")==0)
				printHelp=true;
			else if(strcasecmp(argv[i]+1,"c")==0)
				{
				++i;
				cameraIndex=atoi(argv[i]);
				}
			else if(strcasecmp(argv[i]+1,"f")==0)
				{
				++i;
				frameFilePrefix=argv[i];
				}
			else if(strcasecmp(argv[i]+1,"p")==0)
				{
				++i;
				kinectServerName=argv[i];
				}
			else if(strcasecmp(argv[i]+1,"s")==0)
				{
				++i;
				scale=atof(argv[i]);
				}
			else if(strcasecmp(argv[i]+1,"slf")==0)
				{
				++i;
				sandboxLayoutFileName=argv[i];
				}
			else if(strcasecmp(argv[i]+1,"er")==0)
				{
				++i;
				double elevationMin=atof(argv[i]);
				++i;
				double elevationMax=atof(argv[i]);
				elevationRange=Math::Interval<double>(elevationMin,elevationMax);
				}
			else if(strcasecmp(argv[i]+1,"hmp")==0)
				{
				/* Leer los coeficientes del plano de mapeo de altura: */
				haveHeightMapPlane=true;
				double hmp[4];
				for(int j=0;j<4;++j)
					{
					++i;
					hmp[j]=atof(argv[i]);
					}
				heightMapPlane=Plane(Plane::Vector(hmp),hmp[3]);
				heightMapPlane.normalize();
				}
			else if(strcasecmp(argv[i]+1,"nas")==0)
				{
				++i;
				numAveragingSlots=atoi(argv[i]);
				}
			else if(strcasecmp(argv[i]+1,"sp")==0)
				{
				++i;
				minNumSamples=atoi(argv[i]);
				++i;
				maxVariance=atoi(argv[i]);
				}
			else if(strcasecmp(argv[i]+1,"he")==0)
				{
				++i;
				hysteresis=float(atof(argv[i]));
				}
			else if(strcasecmp(argv[i]+1,"wts")==0)
				{
				for(int j=0;j<2;++j)
					{
					++i;
					wtSize[j]=(unsigned int)(atoi(argv[i]));
					}
				}
			else if(strcasecmp(argv[i]+1,"ws")==0)
				{
				++i;
				waterSpeed=atof(argv[i]);
				++i;
				waterMaxSteps=atoi(argv[i]);
				}
			else if(strcasecmp(argv[i]+1,"rer")==0)
				{
				++i;
				double rainElevationMin=atof(argv[i]);
				++i;
				double rainElevationMax=atof(argv[i]);
				rainElevationRange=Math::Interval<double>(rainElevationMin,rainElevationMax);
				}
			else if(strcasecmp(argv[i]+1,"rs")==0)
				{
				++i;
				rainStrength=GLfloat(atof(argv[i]));
				}
			else if(strcasecmp(argv[i]+1,"evr")==0)
				{
				++i;
				evaporationRate=atof(argv[i]);
				}
			else if(strcasecmp(argv[i]+1,"dds")==0)
				{
				++i;
				demDistScale=float(atof(argv[i]));
				}
			else if(strcasecmp(argv[i]+1,"wi")==0)
				{
				++i;
				windowIndex=atoi(argv[i]);
				
				/* Amplíe la lista de ajustes de procesamiento si se selecciona un índice más allá del final: */
				while(int(renderSettings.size())<=windowIndex)
					renderSettings.push_back(renderSettings.back());
				
				/* Deshabilite la vista del proyector fijo en la nueva configuración de render: */
				renderSettings.back().fixProjectorView=false;
				}
			else if(strcasecmp(argv[i]+1,"fpv")==0)
				{
				renderSettings.back().fixProjectorView=true;
				//renderSettings.back().useContourLines=true;
				std::cout<<"6: Cambiar el estado de-> fixProjectorView " <<std::endl;
				if(i+1<argc&&argv[i+1][0]!='-')
					{
					/* Cargue el archivo de transformación del proyector especificado en el siguiente argumento: */
					++i;
					renderSettings.back().loadProjectorTransform(argv[i]);
					}
				}
			else if(strcasecmp(argv[i]+1,"nhs")==0)
				renderSettings.back().hillshade=false;
			else if(strcasecmp(argv[i]+1,"uhs")==0)
				renderSettings.back().hillshade=true;
			else if(strcasecmp(argv[i]+1,"ns")==0)
				renderSettings.back().useShadows=false;
			else if(strcasecmp(argv[i]+1,"us")==0)
				renderSettings.back().useShadows=true;
			else if(strcasecmp(argv[i]+1,"nhm")==0)
				{
				delete renderSettings.back().elevationColorMap;
				renderSettings.back().elevationColorMap=0;
				}
			else if(strcasecmp(argv[i]+1,"uhm")==0)
				{
				if(i+1<argc&&argv[i+1][0]!='-')
					{
					/* Cargue el archivo de mapa de color de altura especificado en el siguiente argumento: */
					++i;
					renderSettings.back().loadHeightMap(argv[i]);
					}
				else
					{
					/* Cargue el mapa de color de altura predeterminado: */
					std::cout<<"4: Cargar Color-> "<<CONFIG_DEFAULTHEIGHTCOLORMAPFILENAME<<std::endl;
					renderSettings.back().loadHeightMap(CONFIG_DEFAULTHEIGHTCOLORMAPFILENAME);					
					}
				}
			else if(strcasecmp(argv[i]+1,"ncl")==0)
				renderSettings.back().useContourLines=false;
			else if(strcasecmp(argv[i]+1,"ucl")==0)
				{
				renderSettings.back().useContourLines=true;
				if(i+1<argc&&argv[i+1][0]!='-')
					{
					/* Lea el espaciado de la línea de contorno: */
					++i;
					renderSettings.back().contourLineSpacing=GLfloat(atof(argv[i]));
					}
				}
			else if(strcasecmp(argv[i]+1,"rws")==0)
				renderSettings.back().renderWaterSurface=true;
			else if(strcasecmp(argv[i]+1,"rwt")==0)
				renderSettings.back().renderWaterSurface=false;
			else if(strcasecmp(argv[i]+1,"wo")==0)
				{
				++i;
				renderSettings.back().waterOpacity=GLfloat(atof(argv[i]));
				}
			else if(strcasecmp(argv[i]+1,"cp")==0)
				{
				++i;
				controlPipeName=argv[i];
				}
			else
				std::cerr<<"Ignorar comando de línea no reconocido "<<argv[i]<<std::endl;
			}
		}
	//Nuevo
	/* Print usage help if requested: */
	if(printHelp)
		printUsage();

	std::cout<<"7: Inicio " << std::endl;	
	if(frameFilePrefix!=0)
		{
		/* Abra los archivos de video 3D pregrabados seleccionados: */
		std::string colorFileName=frameFilePrefix;
		colorFileName.append(".color");
		std::string depthFileName=frameFilePrefix;
		depthFileName.append(".depth");
		camera=new Kinect::FileFrameSource(Vrui::openFile(colorFileName.c_str()),Vrui::openFile(depthFileName.c_str()));
		}
	else if(kinectServerName!=0)
		{
		/* Dividir el nombre del servidor en nombre de host y puerto: */
		const char* colonPtr=0;
		for(const char* snPtr=kinectServerName;*snPtr!='\0';++snPtr)
			if(*snPtr==':')
				colonPtr=snPtr;
		std::string hostName;
		int port;
		if(colonPtr!=0)
			{
			/* Extraer el nombre de host y el puerto: */
			hostName=std::string(kinectServerName,colonPtr);
			port=atoi(colonPtr+1);
			}
		else
			{
			/* Utilice el nombre de host completo y el puerto predeterminado: */
			hostName=kinectServerName;
			port=26000;
			}
		
		/* Abra una fuente de trama multiplexada para el nombre de host y número de puerto del servidor dado: */
		Kinect::MultiplexedFrameSource* source=Kinect::MultiplexedFrameSource::create(Cluster::openTCPPipe(Vrui::getClusterMultiplexer(),hostName.c_str(),port));
		
		/* Utilice la primera secuencia de componentes del servidor como dispositivo de cámara: */
		camera=source->getStream(0);
		}
	else///Continua
		{		
		/* Abra el dispositivo de cámara 3D del índice seleccionado: */
		Kinect::DirectFrameSource* realCamera=Kinect::openDirectFrameSource(cameraIndex);//kinect
		Misc::ConfigurationFileSection cameraConfigurationSection=cfg.getSection(cameraConfiguration.c_str());
		realCamera->configure(cameraConfigurationSection);
		camera=realCamera;// 0x21cb030
		}	
	for(int i=0;i<2;++i)
	{
		frameSize[i]=camera->getActualFrameSize(Kinect::FrameSource::DEPTH)[i];//camera of size 640 x 480
	}
	
	/* Obtenga los parámetros de corrección de profundidad por píxel de la cámara y evalúelos en la cuadrícula de píxeles del marco de profundidad: */
	Kinect::FrameSource::DepthCorrection* depthCorrection=camera->getDepthCorrectionParameters();	
	if(depthCorrection!=0)//Este 0x21c5180
		{
		pixelDepthCorrection=depthCorrection->getPixelCorrection(frameSize);// 0x1235390
		delete depthCorrection;
		}
	else
		{
		/* Crear parámetros de corrección de profundidad por píxel ficticios: */
		pixelDepthCorrection=new PixelDepthCorrection[frameSize[1]*frameSize[0]];
		PixelDepthCorrection* pdcPtr=pixelDepthCorrection;
		for(unsigned int y=0;y<frameSize[1];++y)
			for(unsigned int x=0;x<frameSize[0];++x,++pdcPtr)
				{
				pdcPtr->scale=1.0f;
				pdcPtr->offset=0.0f;
				}
		}
	
	/* Consigue los parámetros intrínsecos de la cámara: */
	cameraIps=camera->getIntrinsicParameters();
	//std::cout<<"8: Camera-> "<< cameraIps << std::endl;	
	
	/* Lea el archivo de diseño de sandbox: */
	Geometry::Plane<double,3> basePlane;
	Geometry::Point<double,3> basePlaneCorners[4];
	{
	IO::ValueSource layoutSource(Vrui::openFile(sandboxLayoutFileName.c_str()));
	layoutSource.skipWs();
	
	/* Lee la ecuación del plano base: */
	std::string s=layoutSource.readLine();
	basePlane=Misc::ValueCoder<Geometry::Plane<double,3> >::decode(s.c_str(),s.c_str()+s.length());
	basePlane.normalize();
	std::cout<<"8: Plano Base-> "<< basePlane << std::endl;	
	
	/* Lee las esquinas del cuadrilátero base y proyectalas en el plano base: */
	for(int i=0;i<4;++i)
		{
		layoutSource.skipWs();
		s=layoutSource.readLine();
		basePlaneCorners[i]=basePlane.project(Misc::ValueCoder<Geometry::Point<double,3> >::decode(s.c_str(),s.c_str()+s.length()));
		std::cout<<"Componentes del Plano-> "<< basePlaneCorners[i] << std::endl;	
		}
	}
	
	/* Limitar el rango de elevación válida a la intersección de las extensiones de todos los mapas de color altura: */
	for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
		if(rsIt->elevationColorMap!=0)
			{
			Math::Interval<double> mapRange(rsIt->elevationColorMap->getScalarRangeMin(),rsIt->elevationColorMap->getScalarRangeMax());
			elevationRange.intersectInterval(mapRange);
			}
	
	/* Escala todos los tamaños por el factor de escala dado: */
	double sf=scale/100.0; // Factor de escala desde cm hasta unidades finales.
	for(int i=0;i<3;++i)
		for(int j=0;j<4;++j)
			cameraIps.depthProjection.getMatrix()(i,j)*=sf;
	basePlane = Geometry::Plane<double,3>(basePlane.getNormal(),basePlane.getOffset()*sf);
	
	for(int i=0;i<4;++i)
	{
		for(int j=0;j<3;++j)
		{
			basePlaneCorners[i][j]*=sf;
		}
	}
		
			
	if(elevationRange!=Math::Interval<double>::full)
	{
		elevationRange*=sf;// 1
	}

	if(rainElevationRange!=Math::Interval<double>::full)
	{
		rainElevationRange*=sf;// 1
	}
	for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
		{
		if(rsIt->elevationColorMap!=0)
			rsIt->elevationColorMap->setScalarRange(rsIt->elevationColorMap->getScalarRangeMin()*sf,rsIt->elevationColorMap->getScalarRangeMax()*sf);
		rsIt->contourLineSpacing*=sf;
		rsIt->waterOpacity/=sf;
		for(int i=0;i<4;++i)
			rsIt->projectorTransform.getMatrix()(i,3)*=sf;
		}
	rainStrength*=sf;
	evaporationRate*=sf;
	demDistScale*=sf;
	
	/* Crear el objeto de filtro de marco: */
	frameFilter=new FrameFilter(frameSize,numAveragingSlots,pixelDepthCorrection,cameraIps.depthProjection,basePlane);
	frameFilter->setValidElevationInterval(cameraIps.depthProjection,basePlane,elevationRange.getMin(),elevationRange.getMax());
	frameFilter->setStableParameters(minNumSamples,maxVariance);// 10, 2
	frameFilter->setHysteresis(hysteresis);// 0.1f
	frameFilter->setSpatialFilter(true);
	frameFilter->setOutputFrameFunction(Misc::createFunctionCall(this,&Sandbox::receiveFilteredFrame));
	
	if(waterSpeed>0.0)
		{
		std::cout<<"10: Velocidad del agua-> "<< waterSpeed << std::endl;	
		/* Crear el objeto extractor de mano: */
		handExtractor=new HandExtractor(frameSize,pixelDepthCorrection,cameraIps.depthProjection);
		}
	
	/* Iniciar la transmisión de cuadros de profundidad: */
	camera->startStreaming(0,Misc::createFunctionCall(this,&Sandbox::rawDepthFrameDispatcher));/// Inicializar pero no lanzar
	
	/* Crea el renderizador de imágenes en profundidad: */
	depthImageRenderer=new DepthImageRenderer(frameSize);
	depthImageRenderer->setIntrinsics(cameraIps);
	depthImageRenderer->setBasePlane(basePlane);
	
	{
	/* Calcule la transformación del espacio de la cámara al espacio de la caja de arena: */
	ONTransform::Vector z=basePlane.getNormal();
	ONTransform::Vector x=(basePlaneCorners[1]-basePlaneCorners[0])+(basePlaneCorners[3]-basePlaneCorners[2]);
	ONTransform::Vector y=z^x;
	boxTransform=ONTransform::rotate(Geometry::invert(ONTransform::Rotation::fromBaseVectors(x,y)));
	ONTransform::Point center=Geometry::mid(Geometry::mid(basePlaneCorners[0],basePlaneCorners[1]),Geometry::mid(basePlaneCorners[2],basePlaneCorners[3]));
	boxTransform*=ONTransform::translateToOriginFrom(center);
	
	/* Calcula el tamaño del área de la caja de arena: */
	boxSize=Geometry::dist(center,basePlaneCorners[0]);
	for(int i=1;i<4;++i)
		boxSize=Math::max(boxSize,Geometry::dist(center,basePlaneCorners[i]));
	}
	
	/* Calcule un cuadro delimitador alrededor de todas las superficies potenciales: */
	bbox=Box::empty;
	for(int i=0;i<4;++i)
		{
		bbox.addPoint(basePlaneCorners[i]+basePlane.getNormal()*elevationRange.getMin());
		bbox.addPoint(basePlaneCorners[i]+basePlane.getNormal()*elevationRange.getMax());
		}
	
	if(waterSpeed>0.0)
		{
		/* Inicializar la simulación de flujo de agua: */
		waterTable=new WaterTable2(wtSize[0],wtSize[1],depthImageRenderer,basePlaneCorners);
		waterTable->setElevationRange(elevationRange.getMin(),rainElevationRange.getMax());
		waterTable->setWaterDeposit(evaporationRate);
		
		/* Registrar una función de render con la tabla de agua: */
		addWaterFunction=Misc::createFunctionCall(this,&Sandbox::addWater);
		waterTable->addRenderFunction(addWaterFunction);
		addWaterFunctionRegistered=true;
		}
	
	/* Inicialice todos los renderizadores de superficie: */
	for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
		{
		/* Calcule el plano de mapeo de textura para el mapa de altura de este renderizador: */
		//std::cout<<"15: calcTexturePlane?" << std::endl;
		if(rsIt->elevationColorMap!=0)
			{
			if(haveHeightMapPlane)
				rsIt->elevationColorMap->calcTexturePlane(heightMapPlane);
			else
				rsIt->elevationColorMap->calcTexturePlane(depthImageRenderer);
			}
		
		/* Inicialice el renderizador de superficie: */
		rsIt->surfaceRenderer=new SurfaceRenderer(depthImageRenderer);
		rsIt->surfaceRenderer->setDrawContourLines(rsIt->useContourLines);
		rsIt->surfaceRenderer->setContourLineDistance(rsIt->contourLineSpacing);
		rsIt->surfaceRenderer->setElevationColorMap(rsIt->elevationColorMap);
		rsIt->surfaceRenderer->setIlluminate(rsIt->hillshade);
		if(waterTable!=0)
			{
			if(rsIt->renderWaterSurface)
				{
				/* Crear un renderizador de agua: */
				std::cout<<"17: WaterRenderer" << std::endl;
				rsIt->waterRenderer=new WaterRenderer(waterTable);
				}
			else
				{
				std::cout<<"16.5: WaterRenderer"<<std::endl;
				rsIt->surfaceRenderer->setWaterTable(waterTable);
				rsIt->surfaceRenderer->setAdvectWaterTexture(true);
				rsIt->surfaceRenderer->setWaterOpacity(rsIt->waterOpacity);
				}
			}
		rsIt->surfaceRenderer->setDemDistScale(demDistScale);
		}
	
	#if 0
	/* Create a fixed-position light source: */
	sun=Vrui::getLightsourceManager()->createLightsource(true);
	for(int i=0;i<Vrui::getNumViewers();++i)
		Vrui::getViewer(i)->setHeadlightState(false);
	sun->enable();
	sun->getLight().position=GLLight::Position(1,0,1,0);
	#endif
	
	/* Create the GUI: */
	mainMenu=createMainMenu();
	Vrui::setMainMenu(mainMenu);
	if(waterTable!=0)
		waterControlDialog=createWaterControlDialog();
	
	/* Inicialice las clases de herramientas personalizadas: */
	GlobalWaterTool::initClass(*Vrui::getToolManager());
	LocalWaterTool::initClass(*Vrui::getToolManager());
	DEMTool::initClass(*Vrui::getToolManager());
	if(waterTable!=0)
		BathymetrySaverTool::initClass(waterTable,*Vrui::getToolManager());
	addEventTool("Pause Topography",0,0);
	
	if(!controlPipeName.empty())
		{
		/* Abra el conducto de control en modo de no bloqueo: */
		controlPipeFd=open(controlPipeName.c_str(),O_RDONLY|O_NONBLOCK);
		if(controlPipeFd<0)
			std::cerr<<"Unable to open control pipe "<<controlPipeName<<"; ignoring"<<std::endl;
		}
	//Sandbox::glutKeyboardFunc (  'l', 1, 1  );
	/* Inhibir el protector de pantalla: */
	Vrui::inhibitScreenSaver();
	
	/* Establezca la unidad lineal para admitir la escala adecuada: */
	Vrui::getCoordinateManager()->setUnit(Geometry::LinearUnit(Geometry::LinearUnit::METER,scale/100.0));
	std::cout<<"Fin" << std::endl;
	}

Sandbox::~Sandbox(void)
	{
	/* Detener los marcos de profundidad de transmisión: */
	std::cout<<"~Sandbox"<<std::endl;
	camera->stopStreaming();
	delete camera;
	delete frameFilter;
	
	/* Eliminar objetos de ayuda: */
	delete waterTable;
	delete depthImageRenderer;
	delete handExtractor;
	delete addWaterFunction;
	delete[] pixelDepthCorrection;
	
	delete mainMenu;
	delete waterControlDialog;
	
	close(controlPipeFd);
	}

void Sandbox::toolDestructionCallback(Vrui::ToolManager::ToolDestructionCallbackData* cbData)
	{
	/* Compruebe si la herramienta destruida es la herramienta activa de DEM: */
	std::cout<<"Destruction"<<std::endl;
	if(activeDem==dynamic_cast<DEM*>(cbData->tool))
		{
		/* Desactive la herramienta activa de DEM: */
		activeDem=0;
		}
	}

namespace {

/****************
Helper functions:
****************/

std::vector<std::string> tokenizeLine(const char*& buffer)
	{
		std::cout<<"Algo1"<<std::endl;
	std::vector<std::string> result;
	
	/* Omita los espacios en blanco iniciales pero no el final de línea: */
	const char* bPtr=buffer;
	while(*bPtr!='\0'&&*bPtr!='\n'&&isspace(*bPtr))
		++bPtr;
	
	/* Extraiga tokens separados por espacios en blanco hasta que se encuentre una nueva línea o un final de cadena:` */
	while(*bPtr!='\0'&&*bPtr!='\n')
		{
		/* Recuerda el inicio del token actual: */
		const char* tokenStart=bPtr;
		
		/* Encuentra el final del token actual: */
		while(*bPtr!='\0'&&!isspace(*bPtr))
			++bPtr;
		
		/* Extraer el token: */
		result.push_back(std::string(tokenStart,bPtr));
		
		/* Saltar espacios en blanco pero no al final de la línea: */
		while(*bPtr!='\0'&&*bPtr!='\n'&&isspace(*bPtr))
			++bPtr;
		}
	
	/* Saltar final de línea: */
	if(*bPtr=='\n')
		++bPtr;
	
	/* Establezca el inicio de la siguiente línea y devuelva la lista de fichas: */
	buffer=bPtr;
	return result;
	}

bool isToken(const std::string& token,const char* pattern)
	{
	return strcasecmp(token.c_str(),pattern)==0;
	}

}

void Sandbox::glutKeyboardFunc ( unsigned char key, int x, int y ) 
{
	Sandbox::keyboard( key, x, y );
}

void Sandbox::frame(void)
	{
	/* Compruebe si el marco filtrado se ha actualizado: */
	if(filteredFrames.lockNewValue())
		{
		/* Actualice la imagen de profundidad : */
		depthImageRenderer->setDepthImage(filteredFrames.getLockedValue());
		}
	
	if(handExtractor!=0)
		{
		/* Bloquea la lista de manos extraída más reciente: */
		handExtractor->lockNewExtractedHands();
		
		#if 0
		
		/* Register/unregister the rain rendering function based on whether hands have been detected: */
		bool registerWaterFunction=!handExtractor->getLockedExtractedHands().empty();
		if(addWaterFunctionRegistered!=registerWaterFunction)
			{
			if(registerWaterFunction)
				waterTable->addRenderFunction(addWaterFunction);
			else
				waterTable->removeRenderFunction(addWaterFunction);
			addWaterFunctionRegistered=registerWaterFunction;
			}
		
		#endif
		}
	std::cout<<"Algo"<<std::endl;
	
	/* Actualizar todos los renderizadores de superficie: */
	for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
		rsIt->surfaceRenderer->setAnimationTime(Vrui::getApplicationTime());
	
	/* Compruebe si hay un comando de control: */
	if(controlPipeFd>=0)
		{
		/* Intente leer una parte de los datos (fallará con EAGAIN si no hay datos debido al acceso sin bloqueo): */
		char commandBuffer[1024];
		ssize_t readResult=read(controlPipeFd,commandBuffer,sizeof(commandBuffer)-1);
		if(readResult>0)
			{
			commandBuffer[readResult]='\0';
			
			/* Extraer comandos línea por línea: */
			const char* cPtr=commandBuffer;
			while(*cPtr!='\0')
				{
				/* Divide la línea actual en tokens y salta las líneas vacías: */
				std::vector<std::string> tokens=tokenizeLine(cPtr);
				if(tokens.empty())
					continue;
				
				/* Analizar el comando: */
				if(isToken(tokens[0],"waterSpeed"))
					{
					if(tokens.size()==2)
						{
						waterSpeed=atof(tokens[1].c_str());
						if(waterSpeedSlider!=0)
							waterSpeedSlider->setValue(waterSpeed);
						}
					else
						std::cerr<<"Número incorrecto de argumentos para el comando waterSpeed control pipe"<<std::endl;
					}
				else if(isToken(tokens[0],"waterMaxSteps"))
					{
					if(tokens.size()==2)
						{
						waterMaxSteps=atoi(tokens[1].c_str());
						if(waterMaxStepsSlider!=0)
							waterMaxStepsSlider->setValue(waterMaxSteps);
						}
					else
						std::cerr<<"Wrong number of arguments for waterMaxSteps control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"waterAttenuation"))
					{
					if(tokens.size()==2)
						{
						double attenuation=atof(tokens[1].c_str());
						if(waterTable!=0)
							waterTable->setAttenuation(GLfloat(1.0-attenuation));
						if(waterAttenuationSlider!=0)
							waterAttenuationSlider->setValue(attenuation);
						}
					else
						std::cerr<<"Wrong number of arguments for waterAttenuation control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"colorMap"))
					{
					if(tokens.size()==2)
						{
						try
							{
							/* Actualizar todos los mapas de altura: */
							for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
								if(rsIt->elevationColorMap!=0)
									rsIt->elevationColorMap->load(tokens[1].c_str());
							}
						catch(const std::runtime_error& err)
							{
							std::cerr<<"No se puede leer el mapa de color de altura "<<tokens[1]<<" due to exception "<<err.what()<<std::endl;
							}
						}
					else
						std::cerr<<"Wrong number of arguments for colorMap control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"heightMapPlane"))
					{
					if(tokens.size()==5)
						{
						/* Lee la ecuación del plano del mapa de altura: */
						double hmp[4];
						for(int i=0;i<4;++i)
							hmp[i]=atof(tokens[1+i].c_str());
						Plane heightMapPlane=Plane(Plane::Vector(hmp),hmp[3]);
						heightMapPlane.normalize();
						
						/* Anule los planos de asignación de altura de todos los mapas de color de elevación: */
						for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
							if(rsIt->elevationColorMap!=0)
								rsIt->elevationColorMap->calcTexturePlane(heightMapPlane);
						}
					else
						std::cerr<<"Wrong number of arguments for heightMapPlane control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"dippingBed"))
					{
					if(tokens.size()==2&&isToken(tokens[1],"off"))
						{
						/* Desactive la representación de lecho de inmersión en todos los renderizadores de superficie: */
						for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
							rsIt->surfaceRenderer->setDrawDippingBed(false);
						}
					else if(tokens.size()==5)
						{
						/* Lea la ecuación del plano de lecho de inmersión: */
						GLfloat dbp[4];
						for(int i=0;i<4;++i)
							dbp[i]=GLfloat(atof(tokens[1+i].c_str()));
						SurfaceRenderer::Plane dippingBedPlane=SurfaceRenderer::Plane(SurfaceRenderer::Plane::Vector(dbp),dbp[3]);
						dippingBedPlane.normalize();
						
						/* Habilite la representación de lecho de inmersión y establezca la ecuación del plano
						   de lecho de inmersión en todos los renderizadores de superficie: */
						for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
							{
							rsIt->surfaceRenderer->setDrawDippingBed(true);
							rsIt->surfaceRenderer->setDippingBedPlane(dippingBedPlane);
							}
						}
					else
						std::cerr<<"Wrong number of arguments for dippingBed control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"foldedDippingBed"))
					{
					if(tokens.size()==6)
						{
						/* Lea los coeficientes del lecho de inmersión: */
						GLfloat dbc[5];
						for(int i=0;i<5;++i)
							dbc[i]=GLfloat(atof(tokens[1+i].c_str()));
						
						/* Habilite la representación de lecho de inmersión y establezca los coeficientes
						   de lecho de inmersión en todos los renderizadores de superficie: */
						for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
							{
							rsIt->surfaceRenderer->setDrawDippingBed(true);
							rsIt->surfaceRenderer->setDippingBedCoeffs(dbc);
							}
						}
					else
						std::cerr<<"Wrong number of arguments for foldedDippingBed control pipe command"<<std::endl;
					}
				else if(isToken(tokens[0],"dippingBedThickness"))
					{
					if(tokens.size()==2)
						{
						/* Lea el espesor de la cama de inmersión: */
						float dippingBedThickness=float(atof(tokens[1].c_str()));
						
						/* Establezca el grosor del lecho de inmersión en todos los renderizadores de superficie: */
						for(std::vector<RenderSettings>::iterator rsIt=renderSettings.begin();rsIt!=renderSettings.end();++rsIt)
							rsIt->surfaceRenderer->setDippingBedThickness(dippingBedThickness);
						}
					else
						std::cerr<<"Wrong number of arguments for dippingBedThickness control pipe command"<<std::endl;
					}
				else
					std::cerr<<"Unrecognized control pipe command "<<tokens[0]<<std::endl;
				}
			}
		}
	
	if(frameRateTextField!=0&&Vrui::getWidgetManager()->isVisible(waterControlDialog))
		{
		/* Actualizar la pantalla de velocidad de fotogramas: */
		frameRateTextField->setValue(1.0/Vrui::getCurrentFrameTime());
		}
	
	if(pauseUpdates)
		Vrui::scheduleUpdate(Vrui::getApplicationTime()+1.0/30.0);
	}

void Sandbox::keyboard ( unsigned char key, int x, int y )  // Create Keyboard Function
{
	switch ( key ) {
		case 'w':   //Movimientos de camara
		case 'W':
			std::cerr<<"Unrecognized control pipe command "<<std::endl;
			break;
		case 's':
		case 'S':
			//camaraZ -=0.5f;
			break;
		case 'a':
		case 'A':
			//camaraX -= 0.5f;
			break;
		case 'd':
		case 'D':
			//camaraX += 0.5f;
			break;

		case 'i':		//Movimientos de Luz
		case 'I':
			
			break;
		case 'k':
		case 'K':
			
			break;

		case 'l':   //Activamos/desactivamos luz
		case 'L':
			break;
		case 27:        // Cuando Esc es presionado...
			exit ( 0 );   // Salimos del programa
			break;        
		default:        // Cualquier otra
			break;
  }
	//glutPostRedisplay();
}


void Sandbox::display(GLContextData& contextData) const
	{	
	/* Obtener el elemento de datos: */
	DataItem* dataItem=contextData.retrieveDataItem<DataItem>(this);
	
	/* Obtenga los ajustes de renderizado para esta ventana: */
	const Vrui::DisplayState& ds=Vrui::getDisplayState(contextData);
	const Vrui::VRWindow* window=ds.window;
	int windowIndex;
	for(windowIndex=0;windowIndex<Vrui::getNumWindows()&&window!=Vrui::getWindow(windowIndex);++windowIndex)
		;
	const RenderSettings& rs=windowIndex<int(renderSettings.size())?renderSettings[windowIndex]:renderSettings.back();
	
	/* Compruebe si el estado de simulación de agua necesita actualizarse: */
	if(waterTable!=0&&dataItem->waterTableTime!=Vrui::getApplicationTime())
		{
		/* Avtualizar agua table's bathymetry grid: */
		waterTable->updateBathymetry(contextData);
		
		/* Ejecutar el paso principal de simulaciones de flujo de agua: */
		GLfloat totalTimeStep=GLfloat(Vrui::getFrameTime()*waterSpeed);
		unsigned int numSteps=0;
		while(numSteps<waterMaxSteps-1U&&totalTimeStep>1.0e-8f)
			{
			/* Ejecutar con un paso de tiempo autodeterminado para mantener la estabilidad: */
			waterTable->setMaxStepSize(totalTimeStep);
			GLfloat timeStep=waterTable->runSimulationStep(false,contextData);
			totalTimeStep-=timeStep;
			++numSteps;
			}
		#if 0
		if(totalTimeStep>1.0e-8f)
			{
			std::cout<<'.'<<std::flush;
			/* Forzar el paso final para evitar la ralentización de la simulación: */
			waterTable->setMaxStepSize(totalTimeStep);
			GLfloat timeStep=waterTable->runSimulationStep(true,contextData);
			totalTimeStep-=timeStep;
			++numSteps;
			}
		#else
		if(totalTimeStep>1.0e-8f)
			std::cout<<"Ran out of tiempo by "<<totalTimeStep<<std::endl;
		#endif
		
		/* Marque el estado de simulación de agua como actualizado para este cuadro: */
		dataItem->waterTableTime=Vrui::getApplicationTime();
		}
	
	/* Calcula la matriz de proyección: */
	PTransform projection=ds.projection;
	if(rs.fixProjectorView&&rs.projectorTransformValid)
		{
		/* Utilice la transformación del proyector en su lugar: */
		projection=rs.projectorTransform;
		
		/* Multiplica con la transformación de la vista del modelo inverso 
		   para que los rayos sigan funcionando como siempre: */
		projection*=Geometry::invert(ds.modelviewNavigational);
		}
	
	if(rs.hillshade)
		{
		/* Establecer el material de la superficie: */
		glMaterial(GLMaterialEnums::FRONT,rs.surfaceMaterial);
		}
	
	#if 0
	if(rs.hillshade&&rs.useShadows)
		{
		/* Configurar el estado de OpenGL: */
		glPushAttrib(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_ENABLE_BIT|GL_POLYGON_BIT);
		
		GLLightTracker& lt=*contextData.getLightTracker();
		
		/* Guarde el buffer de marco y la ventana gráfica actualmente enlazados: */
		GLint currentFrameBuffer;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT,&currentFrameBuffer);
		GLint currentViewport[4];
		glGetIntegerv(GL_VIEWPORT,currentViewport);
		
		/*******************************************************************
		First rendering pass: Global ambient illumination only
		*******************************************************************/
		
		/* Dibuja la malla de la superficie: */
		surfaceRenderer->glRenderGlobalAmbientHeightMap(dataItem->heightColorMapObject,contextData);
		
		/*******************************************************************
		Second rendering pass: Add local illumination for every light source
		*******************************************************************/
		
		/* Habilitar la representación aditiva: */
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE,GL_ONE);
		glDepthFunc(GL_LEQUAL);
		glDepthMask(GL_FALSE);
		
		for(int lightSourceIndex=0;lightSourceIndex<lt.getMaxNumLights();++lightSourceIndex)
			if(lt.getLightState(lightSourceIndex).isEnabled())
				{
				/***************************************************************
				First step: Render to the light source's shadow map
				***************************************************************/
				
				/* Configure el estado de OpenGL para renderizar en el mapa de sombras: */
				glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,dataItem->shadowFramebufferObject);
				glViewport(0,0,dataItem->shadowBufferSize[0],dataItem->shadowBufferSize[1]);
				glDepthMask(GL_TRUE);
				glClear(GL_DEPTH_BUFFER_BIT);
				glCullFace(GL_FRONT);
				
				/*************************************************************
				Calculate the shadow projection matrix:
				*************************************************************/
				
				/* Obtener la posición de la fuente de luz en el espacio ocular: */
				Geometry::HVector<float,3> lightPosEc;
				glGetLightfv(GL_LIGHT0+lightSourceIndex,GL_POSITION,lightPosEc.getComponents());
				
				/* Transforma la posición de la fuente de luz al espacio de la cámara: */
				Vrui::ONTransform::HVector lightPosCc=Vrui::getDisplayState(contextData).modelviewNavigational.inverseTransform(Vrui::ONTransform::HVector(lightPosEc));
				
				/* Calcule el vector de dirección desde el centro del cuadro delimitador hasta la fuente de luz: */
				Point bboxCenter=Geometry::mid(bbox.min,bbox.max);
				Vrui::Vector lightDirCc=Vrui::Vector(lightPosCc.getComponents())-Vrui::Vector(bboxCenter.getComponents())*lightPosCc[3];
				
				/* Cree una transformación que alinee la dirección de la luz con el eje z positivo: */
				Vrui::ONTransform shadowModelview=Vrui::ONTransform::rotate(Vrui::Rotation::rotateFromTo(lightDirCc,Vrui::Vector(0,0,1)));
				shadowModelview*=Vrui::ONTransform::translateToOriginFrom(bboxCenter);
				
				/* Cree una matriz de proyección, basada en si la luz es posicional o direccional: */
				PTransform shadowProjection(0.0);
				if(lightPosEc[3]!=0.0f)
					{
					/* Modifique la transformación de la vista del modelo de modo que la fuente de luz esté en el origen: */
					shadowModelview.leftMultiply(Vrui::ONTransform::translate(Vrui::Vector(0,0,-lightDirCc.mag())));
					
					/***********************************************************
					Create a perspective projection:
					***********************************************************/
					
					/* Calcule el cuadro delimitador de perspectiva del cuadro delimitador de superficie en el espacio ocular: */
					Box pBox=Box::empty;
					for(int i=0;i<8;++i)
						{
						Point bc=shadowModelview.transform(bbox.getVertex(i));
						pBox.addPoint(Point(-bc[0]/bc[2],-bc[1]/bc[2],-bc[2]));
						}
					
					/* Sube la matriz del tronco: */
					double l=pBox.min[0]*pBox.min[2];
					double r=pBox.max[0]*pBox.min[2];
					double b=pBox.min[1]*pBox.min[2];
					double t=pBox.max[1]*pBox.min[2];
					double n=pBox.min[2];
					double f=pBox.max[2];
					shadowProjection.getMatrix()(0,0)=2.0*n/(r-l);
					shadowProjection.getMatrix()(0,2)=(r+l)/(r-l);
					shadowProjection.getMatrix()(1,1)=2.0*n/(t-b);
					shadowProjection.getMatrix()(1,2)=(t+b)/(t-b);
					shadowProjection.getMatrix()(2,2)=-(f+n)/(f-n);
					shadowProjection.getMatrix()(2,3)=-2.0*f*n/(f-n);
					shadowProjection.getMatrix()(3,2)=-1.0;
					}
				else
					{
					/***********************************************************
					Create a perspective projection:
					***********************************************************/
					
					/* Transforma el cuadro delimitador con la transformación modelview: */
					Box bboxEc=bbox;
					bboxEc.transform(shadowModelview);
					
					/* Subir la matriz orto: */
					double l=bboxEc.min[0];
					double r=bboxEc.max[0];
					double b=bboxEc.min[1];
					double t=bboxEc.max[1];
					double n=-bboxEc.max[2];
					double f=-bboxEc.min[2];
					shadowProjection.getMatrix()(0,0)=2.0/(r-l);
					shadowProjection.getMatrix()(0,3)=-(r+l)/(r-l);
					shadowProjection.getMatrix()(1,1)=2.0/(t-b);
					shadowProjection.getMatrix()(1,3)=-(t+b)/(t-b);
					shadowProjection.getMatrix()(2,2)=-2.0/(f-n);
					shadowProjection.getMatrix()(2,3)=-(f+n)/(f-n);
					shadowProjection.getMatrix()(3,3)=1.0;
					}
				
				/* Multiplique la matriz de la vista de modelo de sombra en la matriz de proyección de sombra: */
				shadowProjection*=shadowModelview;
				
				/* Dibuja la superficie en el buffer de sombra: */
				surfaceRenderer->glRenderDepthOnly(shadowProjection,contextData);
				
				/* Restablecer el estado de OpenGL: */
				glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,currentFrameBuffer);
				glViewport(currentViewport[0],currentViewport[1],currentViewport[2],currentViewport[3]);
				glCullFace(GL_BACK);
				glDepthMask(GL_FALSE);
				
				#if SAVEDEPTH
				/* Guardar la imagen de profundidad: */
				{
				glBindTexture(GL_TEXTURE_2D,dataItem->shadowDepthTextureObject);
				GLfloat* depthTextureImage=new GLfloat[dataItem->shadowBufferSize[1]*dataItem->shadowBufferSize[0]];
				glGetTexImage(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT,GL_FLOAT,depthTextureImage);
				glBindTexture(GL_TEXTURE_2D,0);
				Images::RGBImage dti(dataItem->shadowBufferSize[0],dataItem->shadowBufferSize[1]);
				GLfloat* dtiPtr=depthTextureImage;
				Images::RGBImage::Color* ciPtr=dti.modifyPixels();
				for(int y=0;y<dataItem->shadowBufferSize[1];++y)
					for(int x=0;x<dataItem->shadowBufferSize[0];++x,++dtiPtr,++ciPtr)
						{
						GLColor<GLfloat,3> tc(*dtiPtr,*dtiPtr,*dtiPtr);
						*ciPtr=tc;
						}
				delete[] depthTextureImage;
				Images::writeImageFile(dti,"DepthImage.png");
				}
				#endif
				
				/* Dibuja la superficie usando la textura de la sombra: */
				rs.surfaceRenderer->glRenderShadowedIlluminatedHeightMap(dataItem->heightColorMapObject,dataItem->shadowDepthTextureObject,shadowProjection,contextData);
				}
		
		/* Restablecer el estado de OpenGL: */
		glPopAttrib();
		}
	else
	#endif
		{
		/* Render la superficie en una sola pasada: */
		rs.surfaceRenderer->renderSinglePass(ds.viewport,projection,ds.modelviewNavigational,contextData);
		}
	
	if(rs.waterRenderer!=0)
		{
		/* Dibuje la superficie del agua: */
		glMaterialAmbientAndDiffuse(GLMaterialEnums::FRONT,GLColor<GLfloat,4>(0.0f,0.5f,0.8f));
		glMaterialSpecular(GLMaterialEnums::FRONT,GLColor<GLfloat,4>(1.0f,1.0f,1.0f));
		glMaterialShininess(GLMaterialEnums::FRONT,64.0f);
		rs.waterRenderer->render(projection,ds.modelviewNavigational,contextData);
		}
	}

void Sandbox::resetNavigation(void)
	{
		std::cout<<"Algo2"<<std::endl;
	/* Construya una transformación de navegación para centrar el área de la caja de arena en la pantalla, 
	   de cara al espectador, con el eje largo de la caja de arena hacia la derecha: */
	Vrui::NavTransform nav=Vrui::NavTransform::translateFromOriginTo(Vrui::getDisplayCenter());
	nav*=Vrui::NavTransform::scale(Vrui::getDisplaySize()/boxSize);
	Vrui::Vector y=Vrui::getUpDirection();
	Vrui::Vector z=Vrui::getForwardDirection();
	Vrui::Vector x=z^y;
	nav*=Vrui::NavTransform::rotate(Vrui::Rotation::fromBaseVectors(x,y));
	nav*=boxTransform;
	Vrui::setNavigationTransformation(nav);
	}

void Sandbox::eventCallback(Vrui::Application::EventID eventId,Vrui::InputDevice::ButtonCallbackData* cbData)
	{
	}

void Sandbox::initContext(GLContextData& contextData) const
	{
	/* Cree un elemento de datos y agréguelo al contexto: */
	DataItem* dataItem=new DataItem;
	contextData.addDataItem(this,dataItem);
	
	{
	/* Guarda el búfer de cuadros actualmente enlazado: */
	GLint currentFrameBuffer;
	glGetIntegerv(GL_FRAMEBUFFER_BINDING_EXT,&currentFrameBuffer);
	
	/* Establecer el tamaño predeterminado del búfer de sombra: */
	dataItem->shadowBufferSize[0]=1024;
	dataItem->shadowBufferSize[1]=1024;
	
	/* Generar el buffer de marco de representación sombra: */
	glGenFramebuffersEXT(1,&dataItem->shadowFramebufferObject);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,dataItem->shadowFramebufferObject);
	
	/* Genera una textura de profundidad para la representación de sombras: */
	glGenTextures(1,&dataItem->shadowDepthTextureObject);
	glBindTexture(GL_TEXTURE_2D,dataItem->shadowDepthTextureObject);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_MODE_ARB,GL_COMPARE_R_TO_TEXTURE);
	glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_COMPARE_FUNC_ARB,GL_LEQUAL);
	glTexParameteri(GL_TEXTURE_2D,GL_DEPTH_TEXTURE_MODE_ARB,GL_INTENSITY);
	glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT24_ARB,dataItem->shadowBufferSize[0],dataItem->shadowBufferSize[1],0,GL_DEPTH_COMPONENT,GL_UNSIGNED_BYTE,0);
	glBindTexture(GL_TEXTURE_2D,0);
	
	/* Adjuntar la textura de profundidad al objeto buffer de cuadro: */
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT,GL_TEXTURE_2D,dataItem->shadowDepthTextureObject,0);
	glDrawBuffer(GL_NONE);
	glReadBuffer(GL_NONE);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,currentFrameBuffer);
	} 
	}

VRUI_APPLICATION_RUN(Sandbox)
