#pragma once

/**
*   @brief The Game class is the abstract base class for DirecX 12 demos.
*/
#pragma once

#include <events.h>

#include <memory> // for std::enable_shared_from_this
#include <string> // for std::wstring

class Window;
class Application;

class Game : public std::enable_shared_from_this<Game>
{
public:
	/**
	* Create the DirectX demo using the specified window dimensions.
	*/
	Game(Application &arg_app, const std::wstring& arg_name, int arg_width, int arg_height, bool arg_v_sync);
	virtual ~Game();

	/**
	*  Initialize the DirectX Runtime.
	*/
	virtual bool Initialize();

	/**
	*  Load content required for the demo.
	*/
	virtual bool LoadContent() = 0;

	/**
	*  Unload demo specific content that was loaded in LoadContent.
	*/
	virtual void UnloadContent() = 0;

	/**
	* Destroy any resource that are used by the game.
	*/
	virtual void Destroy();

	int GetClientWidth();
	int GetClientHeight();

protected:
	friend class Window;

	/**
	*  Update the game logic.
	*/
	virtual void OnUpdate(UpdateEventArgs& arg_e);

	/**
	*  Render stuff.
	*/
	virtual void OnRender(RenderEventArgs& arg_e);

	/**
	* Invoked by the registered window when a key is pressed
	* while the window has focus.
	*/
	virtual void OnKeyPressed(KeyEventArgs& arg_e);

	/**
	* Invoked when a key on the keyboard is released.
	*/
	virtual void OnKeyReleased(KeyEventArgs& arg_e);

	/**
	* Invoked when the mouse is moved over the registered window.
	*/
	virtual void OnMouseMoved(MouseMotionEventArgs& arg_e);

	/**
	* Invoked when a mouse button is pressed over the registered window.
	*/
	virtual void OnMouseButtonPressed(MouseButtonEventArgs& arg_e);

	/**
	* Invoked when a mouse button is released over the registered window.
	*/
	virtual void OnMouseButtonReleased(MouseButtonEventArgs& arg_e);

	/**
	* Invoked when the mouse wheel is scrolled while the registered window has focus.
	*/
	virtual void OnMouseWheel(MouseWheelEventArgs& arg_e);

	/**
	* Invoked when the attached window is resized.
	*/
	virtual void OnResize(ResizeEventArgs& arg_e);

	/**
	* Invoked when the registered window instance is destroyed.
	*/
	virtual void OnWindowDestroy();

	std::shared_ptr<Window> window_;

	Application *app_;

private:
	std::wstring name_;
	int width_;
	int height_;
	bool v_sync_;

};

