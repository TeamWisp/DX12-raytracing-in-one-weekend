#include <game.h>
#include <window.h>
#include <application.h>

#include <DirectXMath.h>

#include <d3dx12.h>

Game::Game(Application & arg_app, const std::wstring & arg_name, int arg_width, int arg_height, bool arg_v_sync) :
	app_(&arg_app),
	name_(arg_name),
	width_(arg_width),
	height_(arg_height),
	v_sync_(arg_v_sync)
{
}

Game::~Game()
{ }

bool Game::Initialize()
{
	// Check for DirectX Math library support.
	if (!DirectX::XMVerifyCPUSupport())
	{
		MessageBoxA(NULL, "Failed to verify DirectX Math library support.", "Error", MB_OK | MB_ICONERROR);
		return false;
	}

	window_ = app_->CreateRenderWindow(name_, width_, height_, v_sync_);
	window_->RegisterCallbacks(shared_from_this());
	window_->Show();

	return true;
}

void Game::Destroy()
{
	app_->DestroyWindow(window_);
	window_.reset();
}

int Game::GetClientWidth()
{
	return width_;
}

int Game::GetClientHeight()
{
	return height_;
}

void Game::OnUpdate(UpdateEventArgs & arg_e)
{ }

void Game::OnRender(RenderEventArgs & arg_e)
{ }

void Game::OnKeyPressed(KeyEventArgs & arg_e)
{ }

void Game::OnKeyReleased(KeyEventArgs & arg_e)
{ }

void Game::OnMouseMoved(MouseMotionEventArgs & arg_e)
{ }

void Game::OnMouseButtonPressed(MouseButtonEventArgs & arg_e)
{ }

void Game::OnMouseButtonReleased(MouseButtonEventArgs & arg_e)
{ }

void Game::OnMouseWheel(MouseWheelEventArgs & arg_e)
{ }

void Game::OnResize(ResizeEventArgs & arg_e)
{ }

void Game::OnWindowDestroy()
{ }
