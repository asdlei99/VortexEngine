#include "pch.h"

#include "Engine.h"

#include "Resources/ResCache.h"
#include "Performance/ProfilingManager.h"
#include "SettingsManager.h"
#include <Actors/ActorFactory.h>
#include <Components/ComponentFactory.h>
#include <Graphics/Rendering/RenderManager.h>
#include <Graphics/Window/WindowInfo.h>
#include <Graphics/Window/WindowsWindow.h>
#include <Level/Level.h>
#include <Input/InputDevice.h>

#include "Application.h"

#include <Components/Camera.h>
#include <Components/MeshRenderer.h>
#include <Components/LightComponents/SpotLight.h>
#include <Components/LightComponents/PointLight.h>
#include <Components/LightComponents/DirectionalLight.h>

#include <3rd Party/imgui/imgui.h>
#include <3rd Party/imgui/imgui_impl_dx11.h>
#include <Graphics/Rendering/GraphicsDevice.h>

namespace Core
{
	Engine::Engine() : _isPaused(true), _isRunning(false)
	{
	}


	Engine::~Engine()
	{
	}

	void Engine::Run(std::shared_ptr<Application> application)
	{
		_pApplication = application;

		auto result = Initialize();
		if (!result)
		{
			MessageBox(nullptr, "Failed to initialize engine", "Exit", MB_OK | MB_ICONERROR);
			return;
		}

		RunMainLoop();

		Shutdown();
	}

	bool Engine::Initialize()
	{
		_pLogger.reset(DBG_NEW Logger());
		_pProfilingManager.reset(DBG_NEW ProfilingManager);

		_pEventManager.reset(DBG_NEW EventManager(true));
		HandleRegistering(true);

		_pSettingsManager.reset(DBG_NEW SettingsManager);
		_pSettingsManager->Initialize("Settings.xml");

		_pResourceCache.reset(DBG_NEW ResCache);
		_pResourceCache->RegisterDefaultLoaders();

		_pActorFactory.reset(DBG_NEW ActorFactory());
		RegisterComponents();

		_pRenderManager.reset(DBG_NEW RenderManager());
		_pWindow.reset(DBG_NEW WindowsWindow());

		WindowInfo windowInfo;
		SettingsManager::GetPtr()->GetSetting("Window", "X", windowInfo.X);
		SettingsManager::GetPtr()->GetSetting("Window", "Y", windowInfo.Y);
		SettingsManager::GetPtr()->GetSetting("Window", "Width", windowInfo.Width);
		SettingsManager::GetPtr()->GetSetting("Window", "Height", windowInfo.Heigt);
		SettingsManager::GetPtr()->GetSetting("Window", "Title", windowInfo.Title);

		auto result = _pWindow->Initialize(windowInfo);
		if (!result)
			return false;

		result = _pRenderManager->Initialize(_pWindow->GetHandle());
		if (!result)
			return false;

		// IMGUI Initialization
		auto graphicsDevice = GraphicsDevice::GetPtr();
		ImGui_ImplDX11_Init(_pWindow->GetHandle(), graphicsDevice->GetDevice().Get(), graphicsDevice->GetContext().Get());
		//

		std::string levelPath;
		SettingsManager::GetPtr()->GetSetting("Application", "DefaultLevel", levelPath);
		_pLevel.reset(DBG_NEW Level("DefaultLevel", _pActorFactory));
		if (!_pLevel->LoadLevel(levelPath))
			return false;

		// camera 1
		auto player = _pActorFactory->CreateCameraActor();

		auto playerTransform = player->GetComponent<Transform>().lock();
		playerTransform->SetPosition(Vector3(-5.0f, 5.0f, -10.0f));
		playerTransform->Rotate(-130.0f, -30.0f, 0.0f);

		auto camera = player->GetComponent<Camera>().lock();
		//camera->SetViewPort(ViewPort(0, 0, 0.5f, 1.f));
		_pLevel->AddActor(player);

		// camera 2
		//player = _pActorFactory->CreateCameraActor();
		//
		//playerTransform = player->GetComponent<Transform>().lock();
		//playerTransform->SetPosition(Vector3(+5.f, 5.0f, +10.0f));
		//playerTransform->Rotate(+50.f, -30.0f, 0.0f);
		//
		//camera = player->GetComponent<Camera>().lock();
		//camera->SetViewPort(ViewPort(0.5f, 0, 0.5f, 1.f));
		//_pLevel->AddActor(player);
		
		_pInputDevice.reset(DBG_NEW InputDevice());
		result = _pInputDevice->Init(GetModuleHandle(nullptr), _pWindow->GetHandle());
		if (!result)
			return false;

		return true;
	}

	void Engine::RegisterComponents()
	{
		auto componentFactory = _pActorFactory->GetComponentFactory();
		
		componentFactory->RegisterComponent("Transform", []() { return StrongComponentPtr(DBG_NEW Transform); });
		componentFactory->RegisterComponent("Camera", []() { return StrongComponentPtr(DBG_NEW Camera); });
		componentFactory->RegisterComponent("MeshRenderer", []() { return StrongComponentPtr(DBG_NEW MeshRenderer); });
		componentFactory->RegisterComponent("DirectionalLight", []() { return StrongComponentPtr(DBG_NEW DirectionalLight); });
		componentFactory->RegisterComponent("PointLight", []() { return StrongComponentPtr(DBG_NEW PointLight); });
		componentFactory->RegisterComponent("SpotLight", []() { return StrongComponentPtr(DBG_NEW SpotLight); });

		_pApplication->RegisterComponents(componentFactory);
	}

	void Engine::RunMainLoop()
	{
		_gameTimer.Reset();
		_isRunning = true;
		while (_isRunning)
		{
			MSG msg;
			if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
				continue;
			}

			_gameTimer.Tick();

			UpdateSystems();

			if (!_isPaused)
			{
				Update();
				Render();
			}
		}
	}

	void Engine::UpdateSystems()
	{
		_pInputDevice->OnUpdate();
		_pEventManager->OnUpdate(_gameTimer);
	}

	void Engine::Update()
	{
		_pLevel->OnUpdate(_gameTimer);
	}

	void Engine::Render() const
	{
		ImGui_ImplDX11_NewFrame();

		RenderGui();

		_pRenderManager->OnRender();
	}

	void Engine::RenderGui() const
	{
		ImGui::SetNextWindowPos(ImVec2(100, 100));
		ImGui::SetNextWindowSize(ImVec2(250, 250), ImGuiSetCond_FirstUseEver);

		ImGuiWindowFlags window_flags = 0;
		//window_flags |= ImGuiWindowFlags_NoTitleBar;
		//window_flags |= ImGuiWindowFlags_ShowBorders;
		//window_flags |= ImGuiWindowFlags_NoResize;
		//window_flags |= ImGuiWindowFlags_NoMove;
		//window_flags |= ImGuiWindowFlags_NoScrollbar;
		//window_flags |= ImGuiWindowFlags_NoCollapse;
		//window_flags |= ImGuiWindowFlags_MenuBar;

		static bool isOpen = true;
		ImGui::Begin("Hello World", &isOpen, window_flags);

		ImGui::Text("Hello World");

		static bool isCheck = false;

		ImGui::Checkbox("Run The Game", &isCheck);

		if (isCheck)
			ImGui::Text("You Checked");
		else
			ImGui::Text("You No Checked");

		if (ImGui::Button("Click To Run") == true)
		{
			isCheck = true;
		}


		static int speed = 10;

		ImGui::SliderInt("Speed", &speed, 0, 100);

		ImGui::Text("Speed = %d", speed);


		ImGui::End();
	}

	void Engine::Shutdown() const
	{
		// no need to register since the settings manager is getting destroyed with us
		// HandleRegistering(false);

		std::stringstream msg;
		msg << "Shutting Down The Game" << std::endl;
		msg << "Total Game Time = " << _gameTimer.GameTime() << " Seconds";
		LOG_M(msg.str());

		_pProfilingManager->DumpInfo();

		StrongEventDataPtr saveEvent(DBG_NEW Event_SavingSettings(_pSettingsManager));
		_pEventManager->TriggerEvent(saveEvent);
		if (!_pSettingsManager->SaveToFile())
			LOG_M("Failed to save the settings to file");

		StrongEventDataPtr closeEvent(DBG_NEW Event_ApplicationExiting());
		_pEventManager->TriggerEvent(closeEvent);

		ImGui_ImplDX11_Shutdown();
	}

	void Engine::Pause()
	{
		_gameTimer.Stop();
		_isPaused = true;
	}

	void Engine::Resume()
	{
		_gameTimer.Start();
		_isPaused = false;
	}

	bool Engine::HandleEvent(StrongEventDataPtr eventData)
	{
		auto eventId = eventData->GetID();
		if (eventId == PauseEvent::kEventID)
		{
			Pause();
		}
		else if (eventId == ResumeEvent::kEventID)
		{
			Resume();
		}
		else if (eventId == ShutdownEvent::kEventID)
		{
			_isRunning = false;
		}

		return false;
	}

	void Engine::HandleRegistering(bool isRegistering)
	{
		ToggleRegisteration(ResumeEvent::kEventID, isRegistering);
		ToggleRegisteration(PauseEvent::kEventID, isRegistering);
		ToggleRegisteration(ShutdownEvent::kEventID, isRegistering);
	}
}
