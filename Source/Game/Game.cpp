//
// Copyright (c) 2008-2019 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Engine/Application.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Engine/Console.h>
#include <Urho3D/UI/Cursor.h>
#include <Urho3D/Engine/DebugHud.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/Engine/EngineDefs.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/Input/Input.h>
#include <Urho3D/Input/InputEvents.h>
#include <Urho3D/Graphics/Model.h>
#include <Urho3D/Graphics/Octree.h>
#include <Urho3D/Graphics/Renderer.h>
#include <Urho3D/Graphics/StaticModel.h>
#include <Urho3D/Graphics/StaticModelGroup.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/Scene/SceneEvents.h>
#include <Urho3D/UI/Sprite.h>
#include <Urho3D/Graphics/Texture2D.h>
#include <Urho3D/Core/Timer.h>
#include <Urho3D/UI/UI.h>
#include <Urho3D/Resource/XMLFile.h>
#include <Urho3D/IO/Log.h>
#include <Urho3D/Core/Profiler.h>


#include "Game.h"
#include "MeshGenerator.h"


URHO3D_DEFINE_APPLICATION_MAIN(Game)

Game::Game(Context* context) :
    Application(context),
    yaw_(0.0f),
    pitch_(0.0f),
    touchEnabled_(false),
    useMouseMode_(MM_ABSOLUTE),
    screenJoystickIndex_(M_MAX_UNSIGNED),
    screenJoystickSettingsIndex_(M_MAX_UNSIGNED),
    paused_(false)
{
}

void Game::Setup()
{
    // Modify engine startup parameters
    engineParameters_[EP_WINDOW_TITLE] = "Hello World";
    engineParameters_[EP_LOG_NAME]     = GetSubsystem<FileSystem>()->GetAppPreferencesDir("urho3d", "logs") + GetTypeName() + ".log";
    engineParameters_[EP_FULL_SCREEN]  = false;
    engineParameters_[EP_HEADLESS]     = false;
    engineParameters_[EP_SOUND]        = false;

    // Construct a search path to find the resource prefix with two entries:
    // The first entry is an empty path which will be substituted with program/bin directory -- this entry is for binary when it is still in build tree
    // The second and third entries are possible relative paths from the installed program/bin directory to the asset directory -- these entries are for binary when it is in the Urho3D SDK installation location
    if (!engineParameters_.Contains(EP_RESOURCE_PREFIX_PATHS))
        engineParameters_[EP_RESOURCE_PREFIX_PATHS] = ";../share/Resources;../share/Urho3D/Resources";
}

void Game::Start()
{
    if (GetPlatform() == "Android" || GetPlatform() == "iOS")
        // On mobile platform, enable touch by adding a screen joystick
        InitTouchInput();
    else if (GetSubsystem<Input>()->GetNumJoysticks() == 0)
        // On desktop platform, do not detect touch when we already got a joystick
        SubscribeToEvent(E_TOUCHBEGIN, URHO3D_HANDLER(Game, HandleTouchBegin));

	Engine* engine = GetSubsystem<Engine>();
	engine->SetMaxFps(9999);
	CreateScene();
	InitMouseMode(MM_RELATIVE);
    // Set custom window Title & Icon
    SetWindowTitleAndIcon();

    // Create console and debug HUD
    CreateConsoleAndDebugHud();

    // Subscribe key down event
    SubscribeToEvent(E_KEYDOWN, URHO3D_HANDLER(Game, HandleKeyDown));
    // Subscribe key up event
    SubscribeToEvent(E_KEYUP, URHO3D_HANDLER(Game, HandleKeyUp));
    // Subscribe scene update event
    SubscribeToEvent(E_SCENEUPDATE, URHO3D_HANDLER(Game, HandleSceneUpdate));
}

void Game::Stop()
{
    engine_->DumpResources(true);
}

void Game::InitTouchInput()
{
    touchEnabled_ = true;

    ResourceCache* cache = GetSubsystem<ResourceCache>();
    Input* input = GetSubsystem<Input>();
    XMLFile* layout = cache->GetResource<XMLFile>("UI/ScreenJoystick_Games.xml");
    const String& patchString = GetScreenJoystickPatchString();
    if (!patchString.Empty())
    {
        // Patch the screen joystick layout further on demand
        SharedPtr<XMLFile> patchFile(new XMLFile(context_));
        if (patchFile->FromString(patchString))
            layout->Patch(patchFile);
    }
    screenJoystickIndex_ = (unsigned)input->AddScreenJoystick(layout, cache->GetResource<XMLFile>("UI/DefaultStyle.xml"));
    input->SetScreenJoystickVisible(screenJoystickSettingsIndex_, true);
}

void Game::InitMouseMode(MouseMode mode)
{
    useMouseMode_ = mode;

    Input* input = GetSubsystem<Input>();

    if (GetPlatform() != "Web")
    {
        if (useMouseMode_ == MM_FREE)
            input->SetMouseVisible(true);

        Console* console = GetSubsystem<Console>();
        if (useMouseMode_ != MM_ABSOLUTE)
        {
            input->SetMouseMode(useMouseMode_);
            if (console && console->IsVisible())
                input->SetMouseMode(MM_ABSOLUTE, true);
        }
    }
    else
    {
        input->SetMouseVisible(true);
        SubscribeToEvent(E_MOUSEBUTTONDOWN, URHO3D_HANDLER(Game, HandleMouseModeRequest));
        SubscribeToEvent(E_MOUSEMODECHANGED, URHO3D_HANDLER(Game, HandleMouseModeChange));
    }
}

void Game::SetLogoVisible(bool enable)
{
    if (logoSprite_)
        logoSprite_->SetVisible(enable);
}

void Game::CreateLogo()
{
    // Get logo texture
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    Texture2D* logoTexture = cache->GetResource<Texture2D>("Textures/FishBoneLogo.png");
    if (!logoTexture)
        return;

    // Create logo sprite and add to the UI layout
    UI* ui = GetSubsystem<UI>();
    logoSprite_ = ui->GetRoot()->CreateChild<Sprite>();

    // Set logo sprite texture
    logoSprite_->SetTexture(logoTexture);

    int textureWidth = logoTexture->GetWidth();
    int textureHeight = logoTexture->GetHeight();

    // Set logo sprite scale
    logoSprite_->SetScale(256.0f / textureWidth);

    // Set logo sprite size
    logoSprite_->SetSize(textureWidth, textureHeight);

    // Set logo sprite hot spot
    logoSprite_->SetHotSpot(textureWidth, textureHeight);

    // Set logo sprite alignment
    logoSprite_->SetAlignment(HA_RIGHT, VA_BOTTOM);

    // Make logo not fully opaque to show the scene underneath
    logoSprite_->SetOpacity(0.9f);

    // Set a low priority for the logo so that other UI elements can be drawn on top
    logoSprite_->SetPriority(-100);
}

void Game::CreateScene()
{
	auto* cache = GetSubsystem<ResourceCache>();

	scene_ = new Scene(context_);
	scene_->CreateComponent<Octree>();

	Node* lightNode = scene_->CreateChild("DirectionalLight");
	lightNode->SetDirection(Vector3(0.0f, -1.0f, 1.0f)); // The direction vector does not need to be normalized
	auto* light = lightNode->CreateComponent<Light>();
	light->SetLightType(LIGHT_DIRECTIONAL);

	light->SetCastShadows(true);
	//light->SetShadowBias(BiasParameters(0.00002f, 0.0f));

	// Configure shadow fading for the lights. When they are far away enough, the lights eventually become unshadowed for
	// better GPU performance. Note that we could also set the maximum distance for each object to cast shadows
	//light->SetShadowFadeDistance(100.0f); // Fade start distance
	//light->SetShadowDistance(125.0f); // Fade end distance, shadows are disabled
	// Set half resolution for the shadow maps for increased performance
	light->SetShadowResolution(2.5f);
	// The spot lights will not have anything near them, so move the near plane of the shadow camera farther
	// for better shadow depth resolution
	//light->SetShadowNearFarRatio(0.01f);

	cameraNode_ = scene_->CreateChild("Camera");
	cameraNode_->CreateComponent<Camera>();
	cameraNode_->SetPosition(Vector3(0.0f, 5.0f, 0.0f));


	cache->GetResource<Material>("Materials/StoneSmall.xml");

	Node* planeNode = scene_->CreateChild("Plane");
	planeNode->SetScale(Vector3(100.0f, 1.0f, 100.0f));
	planeNode->SetPosition(Vector3(0.0f, 0.5f, 0.0f));
	auto* planeObject = planeNode->CreateComponent<StaticModel>();
	planeObject->SetModel(cache->GetResource<Model>("Models/Plane.mdl"));
	planeObject->SetMaterial(cache->GetResource<Material>("Materials/StoneTiled.xml"));
	planeObject->SetCastShadows(true);

	/*Node* boxNode = scene_->CreateChild("Plane");
	boxNode->SetScale(Vector3(1.0f, 1.0f, 1.0f));
	boxNode->SetPosition(Vector3(0.0f, 10.0f, 0.0f));
	auto* boxObject = boxNode->CreateComponent<StaticModel>();
	boxObject->SetModel(cache->GetResource<Model>("Models/Box.mdl"));*/


	MeshGenerator* meshGen = new MeshGenerator(context_);

	SharedPtr<Model> fromScratchModel = meshGen->CreateModel((MeshDirection)(MESH_DIR_PositiveX | MESH_DIR_NegativeX | MESH_DIR_PositiveZ | MESH_DIR_NegativeZ));

	Node* node = scene_->CreateChild("FromScratchObject");
	node->SetPosition(Vector3(0.0f, 11.0f, 0.0f));
	auto* object = node->CreateComponent<StaticModel>();
	object->SetModel(fromScratchModel);
	object->SetCastShadows(true);



	//StaticModelGroup* lastGroup = nullptr;
	//bool switchColor = false;
	//for (int y = -125; y < 125; ++y)
	//{
	//	for (int x = -125; x < 125; ++x)
	//	{
	//		// Create new group if no group yet, or the group has already "enough" objects. The tradeoff is between culling
	//		// accuracy and the amount of CPU processing needed for all the objects. Note that the group's own transform
	//		// does not matter, and it does not render anything if instance nodes are not added to it
	//		if (!lastGroup || lastGroup->GetNumInstanceNodes() >= 25 * 25)
	//		{
	//			Node* boxGroupNode = scene_->CreateChild("BoxGroup");
	//			lastGroup = boxGroupNode->CreateComponent<StaticModelGroup>();
	//			if (switchColor) {
	//				lastGroup->SetMaterial(cache->GetResource<Material>("Materials/StoneSmall.xml"));
	//			}
	//			lastGroup->SetModel(cache->GetResource<Model>("Models/Box.mdl"));
	//			switchColor = !switchColor;
	//		}

	//		Node* boxNode = scene_->CreateChild("Plane");
	//		boxNode->SetPosition(Vector3(x * 0.3f, 0.0f, y * 0.3f));
	//		boxNode->SetScale(0.25f);			
	//		boxNodes_.Push(SharedPtr<Node>(boxNode));
	//		lastGroup->AddInstanceNode(boxNode);
	//	}
	//}


	// Set the renderer to viewport
	auto* renderer = GetSubsystem<Renderer>();

	// Set up a viewport to the Renderer subsystem so that the 3D scene can be seen. We need to define the scene and the camera
	// at minimum. Additionally we could configure the viewport screen size and the rendering path (eg. forward / deferred) to
	// use, but now we just use full screen and default render path configured in the engine command line options
	SharedPtr<Viewport> viewport(new Viewport(context_, scene_, cameraNode_->GetComponent<Camera>()));
	renderer->SetViewport(0, viewport);
}

void Game::SetWindowTitleAndIcon()
{
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    Graphics* graphics = GetSubsystem<Graphics>();
    Image* icon = cache->GetResource<Image>("Textures/UrhoIcon.png");
    graphics->SetWindowIcon(icon);
    graphics->SetWindowTitle("Hello World");
}

void Game::CreateConsoleAndDebugHud()
{
    // Get default style
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    XMLFile* xmlFile = cache->GetResource<XMLFile>("UI/DefaultStyle.xml");

    // Create console
    Console* console = engine_->CreateConsole();
    console->SetDefaultStyle(xmlFile);
    console->GetBackground()->SetOpacity(0.8f);

    // Create debug HUD.
    DebugHud* debugHud = engine_->CreateDebugHud();
    debugHud->SetDefaultStyle(xmlFile);
}


void Game::HandleKeyUp(StringHash /*eventType*/, VariantMap& eventData)
{
    using namespace KeyUp;

    int key = eventData[P_KEY].GetInt();

    // Close console (if open) or exit when ESC is pressed
    if (key == KEY_ESCAPE)
    {
        Console* console = GetSubsystem<Console>();
        if (console->IsVisible())
            console->SetVisible(false);
        else
        {
            if (GetPlatform() == "Web")
            {
                GetSubsystem<Input>()->SetMouseVisible(true);
                if (useMouseMode_ != MM_ABSOLUTE)
                    GetSubsystem<Input>()->SetMouseMode(MM_FREE);
            }
            else
                engine_->Exit();
        }
    }
}

void Game::HandleKeyDown(StringHash /*eventType*/, VariantMap& eventData)
{
    using namespace KeyDown;

    int key = eventData[P_KEY].GetInt();

    // Toggle console with F1
    if (key == KEY_F1)
        GetSubsystem<Console>()->Toggle();

    // Toggle debug HUD with F2
    else if (key == KEY_F2)
        GetSubsystem<DebugHud>()->ToggleAll();

    // Common rendering quality controls, only when UI has no focused element
    else if (!GetSubsystem<UI>()->GetFocusElement())
    {
        Renderer* renderer = GetSubsystem<Renderer>();

        // Preferences / Pause
        if (key == KEY_SELECT && touchEnabled_)
        {
            paused_ = !paused_;

            Input* input = GetSubsystem<Input>();
            if (screenJoystickSettingsIndex_ == M_MAX_UNSIGNED)
            {
                // Lazy initialization
                ResourceCache* cache = GetSubsystem<ResourceCache>();
                screenJoystickSettingsIndex_ = (unsigned)input->AddScreenJoystick(cache->GetResource<XMLFile>("UI/ScreenJoystickSettings_Games.xml"), cache->GetResource<XMLFile>("UI/DefaultStyle.xml"));
            }
            else
                input->SetScreenJoystickVisible(screenJoystickSettingsIndex_, paused_);
        }

        // Texture quality
        else if (key == '1')
        {
            auto quality = (unsigned)renderer->GetTextureQuality();
            ++quality;
            if (quality > QUALITY_HIGH)
                quality = QUALITY_LOW;
            renderer->SetTextureQuality((MaterialQuality)quality);
        }

        // Material quality
        else if (key == '2')
        {
            auto quality = (unsigned)renderer->GetMaterialQuality();
            ++quality;
            if (quality > QUALITY_HIGH)
                quality = QUALITY_LOW;
            renderer->SetMaterialQuality((MaterialQuality)quality);
        }

        // Specular lighting
        else if (key == '3')
            renderer->SetSpecularLighting(!renderer->GetSpecularLighting());

        // Shadow rendering
        else if (key == '4')
            renderer->SetDrawShadows(!renderer->GetDrawShadows());

        // Shadow map resolution
        else if (key == '5')
        {
            int shadowMapSize = renderer->GetShadowMapSize();
            shadowMapSize *= 2;
            if (shadowMapSize > 2048)
                shadowMapSize = 512;
            renderer->SetShadowMapSize(shadowMapSize);
        }

        // Shadow depth and filtering quality
        else if (key == '6')
        {
            ShadowQuality quality = renderer->GetShadowQuality();
            quality = (ShadowQuality)(quality + 1);
            if (quality > SHADOWQUALITY_BLUR_VSM)
                quality = SHADOWQUALITY_SIMPLE_16BIT;
            renderer->SetShadowQuality(quality);
        }

        // Occlusion culling
        else if (key == '7')
        {
            bool occlusion = renderer->GetMaxOccluderTriangles() > 0;
            occlusion = !occlusion;
            renderer->SetMaxOccluderTriangles(occlusion ? 5000 : 0);
        }

        // Instancing
        else if (key == '8')
            renderer->SetDynamicInstancing(!renderer->GetDynamicInstancing());

        // Take screenshot
        else if (key == '9')
        {
            Graphics* graphics = GetSubsystem<Graphics>();
            Image screenshot(context_);
            graphics->TakeScreenShot(screenshot);
            // Here we save in the Data folder with date and time appended
            screenshot.SavePNG(GetSubsystem<FileSystem>()->GetProgramDir() + "Data/Screenshot_" +
                Time::GetTimeStamp().Replaced(':', '_').Replaced('.', '_').Replaced(' ', '_') + ".png");
        }
    }
}

void Game::HandleSceneUpdate(StringHash /*eventType*/, VariantMap& eventData)
{
	float timeStep = eventData["TimeStep"].GetFloat();

    // Move the camera by touch, if the camera node is initialized by descendant Game class
		// Do not move if the UI has a focused element (the console)
	if (GetSubsystem<UI>()->GetFocusElement())
		return;

	auto* input = GetSubsystem<Input>();

	// Movement speed as world units per second
	const float MOVE_SPEED = 20.0f;
	// Mouse sensitivity as degrees per pixel
	const float MOUSE_SENSITIVITY = 0.1f;

	// Use this frame's mouse motion to adjust camera node yaw and pitch. Clamp the pitch between -90 and 90 degrees
	IntVector2 mouseMove = input->GetMouseMove();
	yaw_ += MOUSE_SENSITIVITY * mouseMove.x_;
	pitch_ += MOUSE_SENSITIVITY * mouseMove.y_;
	pitch_ = Clamp(pitch_, -90.0f, 90.0f);

	// Construct new orientation for the camera scene node from yaw and pitch. Roll is fixed to zero
	cameraNode_->SetRotation(Quaternion(pitch_, yaw_, 0.0f));

	// Read WASD keys and move the camera scene node to the corresponding direction if they are pressed
	// Use the Translate() function (default local space) to move relative to the node's orientation.
	if (input->GetKeyDown(KEY_W))
		cameraNode_->Translate(Vector3::FORWARD * MOVE_SPEED * timeStep);
	if (input->GetKeyDown(KEY_S))
		cameraNode_->Translate(Vector3::BACK * MOVE_SPEED * timeStep);
	if (input->GetKeyDown(KEY_A))
		cameraNode_->Translate(Vector3::LEFT * MOVE_SPEED * timeStep);
	if (input->GetKeyDown(KEY_D))
		cameraNode_->Translate(Vector3::RIGHT * MOVE_SPEED * timeStep);
	if (input->GetKeyDown(KEY_SPACE))
		cameraNode_->Translate(Vector3::UP * MOVE_SPEED * timeStep);
	if (input->GetKeyDown(KEY_SHIFT))
		cameraNode_->Translate(Vector3::DOWN * MOVE_SPEED * timeStep);

	if (input->GetKeyPress(KEY_M))
		animate_ = !animate_;

	if (animate_)
		AnimateObjects(timeStep);
}

void Game::HandleTouchBegin(StringHash /*eventType*/, VariantMap& eventData)
{
    // On some platforms like Windows the presence of touch input can only be detected dynamically
    InitTouchInput();
    UnsubscribeFromEvent("TouchBegin");
}

// If the user clicks the canvas, attempt to switch to relative mouse mode on web platform
void Game::HandleMouseModeRequest(StringHash /*eventType*/, VariantMap& eventData)
{
    Console* console = GetSubsystem<Console>();
    if (console && console->IsVisible())
        return;
    Input* input = GetSubsystem<Input>();
    if (useMouseMode_ == MM_ABSOLUTE)
        input->SetMouseVisible(false);
    else if (useMouseMode_ == MM_FREE)
        input->SetMouseVisible(true);
    input->SetMouseMode(useMouseMode_);
}

void Game::HandleMouseModeChange(StringHash /*eventType*/, VariantMap& eventData)
{
    Input* input = GetSubsystem<Input>();
    bool mouseLocked = eventData[MouseModeChanged::P_MOUSELOCKED].GetBool();
    input->SetMouseVisible(!mouseLocked);
}



void Game::AnimateObjects(float timeStep)
{
	URHO3D_PROFILE(AnimateObjects);

	const float ROTATE_SPEED = 15.0f;
	// Rotate about the Z axis (roll)
	Quaternion rotateQuat(ROTATE_SPEED * timeStep, Vector3::FORWARD);

	for (unsigned i = 0; i < boxNodes_.Size(); ++i)
		boxNodes_[i]->Rotate(rotateQuat);
}