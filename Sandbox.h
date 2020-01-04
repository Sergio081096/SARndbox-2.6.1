/***********************************************************************
Sandbox: Aplicación de Vrui para manejar un sandbox de realidad aumentada.
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

#ifndef SANDBOX_INCLUDED
#define SANDBOX_INCLUDED

#include <Threads/TripleBuffer.h>
#include <Geometry/Box.h>
#include <Geometry/Rotation.h>
#include <Geometry/OrthonormalTransformation.h>
#include <Geometry/ProjectiveTransformation.h>
#include <GL/gl.h>
#include "GL/glut.h"			// Archivo de cabecera para la libreria Glut
#include <GL/GLColorMap.h>
#include <GL/GLMaterial.h>
#include <GL/GLObject.h>
#include <GL/GLGeometryVertex.h>
#include <GLMotif/ToggleButton.h>
#include <GLMotif/TextFieldSlider.h>
#include <Vrui/Tool.h>
#include <Vrui/GenericToolFactory.h>
#include <Vrui/TransparentObject.h>
#include <Vrui/Application.h>
#include <Kinect/FrameBuffer.h>
#include <Kinect/FrameSource.h>

#include "Types.h"

/* Declaraciones de reenvío: */
namespace Misc {
template <class ParameterParam>
class FunctionCall;
}
class GLContextData;
namespace GLMotif {
class PopupMenu;
class PopupWindow;
class TextField;
}
namespace Vrui {
class Lightsource;
}
namespace Kinect {
class Camera;
}
class FrameFilter;
class DepthImageRenderer;
class ElevationColorMap;
class DEM;
class SurfaceRenderer;
class WaterTable2;
class HandExtractor;
typedef Misc::FunctionCall<GLContextData&> AddWaterFunction;
class WaterRenderer;

class Sandbox:public Vrui::Application,public GLObject
	{
	/* Clases integradas: */
	private:
	typedef Geometry::Box<Scalar,3> Box; // Tipo para cuadros delimitadores
	typedef Geometry::OrthonormalTransformation<Scalar,3> ONTransform; // Tipo para transformaciones de cuerpo rígido
	typedef Kinect::FrameSource::DepthCorrection::PixelCorrection PixelDepthCorrection; // Escriba para factores de corrección de profundidad por píxel
	
	struct DataItem:public GLObject::DataItem
		{
		/* Elementos: */
		public:
		double waterTableTime; // Marca de tiempo de simulación de la capa freática en este contexto OpenGL
		GLsizei shadowBufferSize[2]; // Tamaño del búfer del marco de representación de sombras
		GLuint shadowFramebufferObject; // Objeto de búfer de marco para representar mapas de sombra
		GLuint shadowDepthTextureObject; // Textura de profundidad para el búfer del marco de renderizado de sombras
		
		/* Constructores y destructores: */
		DataItem(void);
		virtual ~DataItem(void);
		};
	
	struct RenderSettings // Estructura para mantener la configuración de representación por ventana
		{
		/* Elementos: */
		public:
		bool fixProjectorView; // Marque si desea permitir la navegación del punto de vista o siempre renderizar desde el punto de vista del proyector
		PTransform projectorTransform; // La matriz de transformación del proyector calibrada para renderizado de proyección fija
		bool projectorTransformValid; // Marque si la transformación del proyector es válida
		bool hillshade; // Marque si se debe usar sombreado de colina de realidad aumentada
		GLMaterial surfaceMaterial; // Propiedades del material para renderizar la superficie en modo sombreado
		bool useShadows; // Marque si se deben usar sombras en el sombreado de colina de realidad aumentada
		ElevationColorMap* elevationColorMap; // Puntero a un mapa de color de elevación
		bool useContourLines; // Marque si dibujar líneas de contorno de elevación
		GLfloat contourLineSpacing; // Espaciado entre líneas de contorno adyacentes en cm
		bool renderWaterSurface; // Marque si se debe representar la superficie del agua como una superficie geométrica
		GLfloat waterOpacity; // Factor de opacidad del agua cuando se procesa como textura
		SurfaceRenderer* surfaceRenderer; // Objeto de representación de superficie para esta ventana
		WaterRenderer* waterRenderer; // Un renderizador para representar la superficie del agua como geometría
		
		/* Constructores y destructores: */
		RenderSettings(bool line); // Crea configuraciones de representación predeterminadas
		RenderSettings(const RenderSettings& source); // Copia constructor
		~RenderSettings(void); // Destruye la configuración de representación
		
		/* Metodos: */
		void loadProjectorTransform(const char* projectorTransformName); // Carga una transformación de proyector desde el archivo dado
		void loadHeightMap(const char* heightMapName); // Carga el mapa de altura seleccionado
		};
	
	friend class GlobalWaterTool;
	friend class LocalWaterTool;
	friend class DEMTool;
	
	/* Elementos: */
	private:
	Kinect::FrameSource* camera; // El dispositivo de cámara Kinect
	unsigned int frameSize[2]; // Ancho y alto de los marcos de profundidad de la cámara.
	PixelDepthCorrection* pixelDepthCorrection; // Buffer de coeficientes de corrección de profundidad por píxel
	Kinect::FrameSource::IntrinsicParameters cameraIps; // Parámetros intrínsecos de la cámara Kinect.
	FrameFilter* frameFilter; // Procesamiento de objetos para filtrar fotogramas de profundidad sin procesar de la cámara Kinect
	bool pauseUpdates; // Pausa las actualizaciones de la topografía.
	bool pauseLine;
	Threads::TripleBuffer<Kinect::FrameBuffer> filteredFrames; // Triple buffer para marcos de profundidad filtrada entrantes
	DepthImageRenderer* depthImageRenderer; // Objeto que gestiona la imagen de profundidad filtrada actual
	ONTransform boxTransform; // Transformación del espacio de la cámara al espacio del plano base (x a lo largo del eje de caja de arena larga, z hacia arriba)
	Scalar boxSize; // Radio de la esfera alrededor del área de sandbox
	Box bbox; // Cuadro delimitador alrededor de todas las superficies potenciales
	WaterTable2* waterTable; // Objeto de simulación de flujo de agua
	double waterSpeed; // Velocidad relativa de la simulación del flujo de agua.
	unsigned int waterMaxSteps; // Número máximo de pasos de simulación de agua por cuadro
	GLfloat rainStrength; // Cantidad de agua depositada por herramientas y objetos de lluvia en cada paso de simulación de agua
	HandExtractor* handExtractor; // Objeto para detectar manos extendidas sobre la superficie de arena para hacer que llueva
	const AddWaterFunction* addWaterFunction; // Función de procesamiento registrada con la capa freática
	bool addWaterFunctionRegistered; // Marcar si la función de adición de agua está registrada actualmente en la capa freática
	std::vector<RenderSettings> renderSettings; // Lista de configuraciones de representación por ventana
	Vrui::Lightsource* sun; // Una fuente de luz fija externa
	DEM* activeDem; // El DEM actualmente activo
	GLMotif::PopupMenu* mainMenu;
	GLMotif::ToggleButton* pauseUpdatesToggle;
	GLMotif::ToggleButton* pauseUpdatesLine;
	GLMotif::PopupWindow* waterControlDialog;
	GLMotif::TextFieldSlider* waterSpeedSlider;
	GLMotif::TextFieldSlider* waterMaxStepsSlider;
	GLMotif::TextField* frameRateTextField;
	GLMotif::TextFieldSlider* waterAttenuationSlider;
	int controlPipeFd; // Descriptor de archivo de una tubería con nombre opcional para enviar comandos de control a un AR Sandbox en ejecución
	
	/* Métodos privados:s */
	void rawDepthFrameDispatcher(const Kinect::FrameBuffer& frameBuffer); // Devolución de llamada que recibe fotogramas de profundidad sin procesar de la cámara Kinect; los reenvía al filtro de marco y a los objetos de lluvia
	void receiveFilteredFrame(const Kinect::FrameBuffer& frameBuffer); // Devolución de llamada que recibe marcos de profundidad filtrados del objeto de filtro
	void toggleDEM(DEM* dem); // Establece o alterna el DEM actualmente activo
	void addWater(GLContextData& contextData) const; // Función para renderizar geometría que agrega agua a la capa freática
	void pauseUpdatesCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData);
	void pauseLineCallback(GLMotif::ToggleButton::ValueChangedCallbackData* cbData);
	void showWaterControlDialogCallback(Misc::CallbackData* cbData);
	void waterSpeedSliderCallback(GLMotif::TextFieldSlider::ValueChangedCallbackData* cbData);
	void waterMaxStepsSliderCallback(GLMotif::TextFieldSlider::ValueChangedCallbackData* cbData);
	void waterAttenuationSliderCallback(GLMotif::TextFieldSlider::ValueChangedCallbackData* cbData);
	GLMotif::PopupMenu* createMainMenu(void);
	GLMotif::PopupWindow* createWaterControlDialog(void);

	void glutKeyboardFunc( unsigned char key, int x, int y );	
	void keyboard( unsigned char key, int x, int y );
	void glutPostRedisplay(void);
	
	/* Constructores y destructores: */
	public:
	Sandbox(int& argc,char**& argv);
	virtual ~Sandbox(void);
	
	/* Métodos de Vrui::Aplicación: */
	virtual void toolDestructionCallback(Vrui::ToolManager::ToolDestructionCallbackData* cbData);
	virtual void frame(void);
	virtual void display(GLContextData& contextData) const;
	virtual void resetNavigation(void);
	virtual void eventCallback(EventID eventId,Vrui::InputDevice::ButtonCallbackData* cbData);
	
	/* Métodos de GLObject: */
	virtual void initContext(GLContextData& contextData) const;
	};

#endif
