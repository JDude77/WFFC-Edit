//
// Game.cpp
//

#include "pch.h"
#include "Game.h"
#include <string>

using namespace DirectX;
using namespace SimpleMath;
using Microsoft::WRL::ComPtr;

/**
 * \brief Pi to fifty digits
 */
constexpr auto PI_LONG = 3.14159265358979323846264338327950288419716939937510f;
/**
 * \brief Pi to the first five decimal digits
 */
constexpr auto PI_SHORT = 3.14159f;

Game::Game() : m_camera(std::make_unique<Camera>())
{
    m_deviceResources = std::make_unique<DX::DeviceResources>();
    m_deviceResources->RegisterDeviceNotify(this);
	m_displayList.clear();
	
	//Initial settings
	//Modes
	m_grid = false;
}//End default constructor

Game::~Game()
{
#ifdef DXTK_AUDIO
    if (m_audEngine)
    {
        m_audEngine->Suspend();
    }//End if
#endif
}//End destructor

//Initialize the Direct3D resources required to run
void Game::Initialize(const HWND window, const int width, const int height)
{
    m_gamePad = std::make_unique<GamePad>();
    m_keyboard = std::make_unique<Keyboard>();
    m_mouse = std::make_unique<Mouse>();
    m_mouse->SetWindow(window);

    m_deviceResources->SetWindow(window, width, height);
    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    GetClientRect(window, &m_screenDimensions);

#ifdef DXTK_AUDIO
    //Create DirectXTK for audio objects
    AUDIO_ENGINE_FLAGS eflags = AudioEngine_Default;
#ifdef _DEBUG
    eflags = eflags | AudioEngine_Debug;
#endif

    m_audEngine = std::make_unique<AudioEngine>(eflags);

    m_audioEvent = 0;
    m_audioTimerAcc = 10.f;
    m_retryDefault = false;

    m_waveBank = std::make_unique<WaveBank>(m_audEngine.get(), L"adpcmdroid.xwb");

    m_soundEffect = std::make_unique<SoundEffect>(m_audEngine.get(), L"MusicMono_adpcm.wav");
    m_effect1 = m_soundEffect->CreateInstance();
    m_effect2 = m_waveBank->CreateInstance(10);

    m_effect1->Play(true);
    m_effect2->Play();
#endif
}//End Initialize

void Game::SetGridState(const bool state)
{
	m_grid = state;
}//End SetGridState

#pragma region Frame Update
//Executes the basic game loop
void Game::Tick(const InputCommands *input)
{
	//Copy over input commands so we have a local version to use elsewhere
	m_inputCommands = *input;
    m_timer.Tick([&]()
    {
        m_camera->Update(*input);
        Update(m_timer);
    });

#ifdef DXTK_AUDIO
    // Only update audio engine once per frame
    if (!m_audEngine->IsCriticalError() && m_audEngine->Update())
    {
        // Setup a retry in 1 second
        m_audioTimerAcc = 1.f;
        m_retryDefault = true;
    }//End if
#endif

    Render();
}//End Tick

int Game::MousePicking() const
{
    int selectedID = -1;
    float pickingDistance = 0.0f;
    float shortestDistance = D3D11_FLOAT32_MAX;

    //Set up selection near and far planes
    const XMVECTOR nearPlane    = XMVectorSet(m_inputCommands.mouseX, m_inputCommands.mouseY, 0.0f, 1.0f);
    const XMVECTOR farPlane     = XMVectorSet(m_inputCommands.mouseX, m_inputCommands.mouseY, 1.0f, 1.0f);

    //Loop through the object display list and check each to pick
    for(int id = 0; id < m_displayList.size(); id++)
    {
	    //Get object translation, scale, and rotation
        const XMVECTORF32 translation =
        {
            m_displayList[id].m_position.x,
            m_displayList[id].m_position.y,
            m_displayList[id].m_position.z
        };
        const XMVECTORF32 scale =
        {
            m_displayList[id].m_scale.x,
            m_displayList[id].m_scale.y,
            m_displayList[id].m_scale.z
        };
	    const XMVECTOR rotation = Quaternion::CreateFromYawPitchRoll
    	(
			m_displayList[id].m_orientation.y * PI_SHORT / 180.0f,
            m_displayList[id].m_orientation.x * PI_SHORT / 180.0f,
            m_displayList[id].m_orientation.z * PI_SHORT / 180.0f
        );

        //Construct the matrix of the object in the world
        XMMATRIX objectTransformMatrix = m_world * XMMatrixTransformation
    	(
            g_XMZero,
            Quaternion::Identity,
            scale,
            g_XMZero,
            rotation,
            translation
        );

        //Unproject the points on the near/far plane using the object matrix
        const XMVECTOR nearPoint = XMVector3Unproject
    	(
			nearPlane,
            0.0f,
            0.0f,
            m_screenDimensions.right,
            m_screenDimensions.bottom,
            m_deviceResources->GetScreenViewport().MinDepth,
            m_deviceResources->GetScreenViewport().MaxDepth,
            m_projection,
            m_view,
            objectTransformMatrix
        );
        const XMVECTOR farPoint = XMVector3Unproject
    	(
			farPlane,
            0.0f,
            0.0f,
            m_screenDimensions.right,
            m_screenDimensions.bottom,
            m_deviceResources->GetScreenViewport().MinDepth,
            m_deviceResources->GetScreenViewport().MaxDepth,
            m_projection,
            m_view,
            objectTransformMatrix
        );

        //Transform the points into the picking vector
        XMVECTOR pickingVector = farPoint - nearPoint;
        pickingVector = XMVector3Normalize(pickingVector);

        //Loop through the object's mesh list
        auto& objectMeshList = m_displayList[id].m_model.get()->meshes;
        for(int meshIndex = 0; meshIndex < objectMeshList.size(); meshIndex++)
        {
	        if(objectMeshList[meshIndex]->boundingBox.Intersects(nearPoint, pickingVector, pickingDistance))
	        {
                if(pickingDistance < shortestDistance)
                {
                    shortestDistance = pickingDistance;
					selectedID = id;
                }//End if
	        }//End if
        }//End for
    }//End for

    //Return the ID of the selected object
    return selectedID;
}//End MousePicking

//Updates the world
void Game::Update(DX::StepTimer const& timer)
{
	//Apply camera vectors
    m_view = Matrix::CreateLookAt(m_camera->m_camPosition, m_camera->m_camLookAt, Vector3::UnitY);

    m_batchEffect->SetView(m_view);
    m_batchEffect->SetWorld(Matrix::Identity);
	m_displayChunk.m_terrainEffect->SetView(m_view);
	m_displayChunk.m_terrainEffect->SetWorld(Matrix::Identity);

#ifdef DXTK_AUDIO
    m_audioTimerAcc -= (float)timer.GetElapsedSeconds();
    if (m_audioTimerAcc < 0)
    {
        if (m_retryDefault)
        {
            m_retryDefault = false;
            if (m_audEngine->Reset())
            {
                // Restart looping audio
                m_effect1->Play(true);
            }//End if
        }//End if
        else
        {
            m_audioTimerAcc = 4.f;

            m_waveBank->Play(m_audioEvent++);

            if (m_audioEvent >= 11)
                m_audioEvent = 0;
        }//End else
    }//End if
#endif
}//End Update
#pragma endregion

#pragma region Frame Render
//Draws the scene
void Game::Render()
{
    //Don't try to render anything before the first update
    if (m_timer.GetFrameCount() == 0) return;

    Clear();

    m_deviceResources->PIXBeginEvent(L"Render");
    const auto context = m_deviceResources->GetD3DDeviceContext();

	if (m_grid)
	{
		//Draw procedurally-generated dynamic grid
		const XMVECTORF32 xAxis = { 512.f, 0.f, 0.f };
		const XMVECTORF32 yAxis = { 0.f, 0.f, 512.f };
		DrawGrid(xAxis, yAxis, g_XMZero, 512, 512, Colors::Gray);
	}//End if

	//CAMERA POSITION ON HUD
	m_sprites->Begin();
    const std::wstring cameraPositionText =
        L"Cam X: " + std::to_wstring(m_camera->m_camPosition.x) +
        L"            " +
        L"Cam Y: " + std::to_wstring(m_camera->m_camPosition.y) +
        L"            " +
        L"Cam Z: " + std::to_wstring(m_camera->m_camPosition.z);
	m_font->DrawString(m_sprites.get(), cameraPositionText.c_str() , XMFLOAT2(100, 10), Colors::Yellow);
	m_sprites->End();

	//RENDER OBJECTS FROM SCENEGRAPH
    const int numRenderObjects = m_displayList.size();
	for (int i = 0; i < numRenderObjects; i++)
	{
		m_deviceResources->PIXBeginEvent(L"Draw Model");
		const XMVECTORF32 scale = { m_displayList[i].m_scale.x,
									m_displayList[i].m_scale.y,
									m_displayList[i].m_scale.z };

		const XMVECTORF32 translate = { m_displayList[i].m_position.x,
										m_displayList[i].m_position.y,
										m_displayList[i].m_position.z };

		//Convert degrees into radians for rotation matrix
		const XMVECTOR rotate = Quaternion::CreateFromYawPitchRoll(m_displayList[i].m_orientation.y *3.1415 / 180,
		                                                           m_displayList[i].m_orientation.x *3.1415 / 180,
		                                                           m_displayList[i].m_orientation.z *3.1415 / 180);

		const XMMATRIX local = m_world * XMMatrixTransformation(g_XMZero, Quaternion::Identity, scale, g_XMZero, rotate, translate);

		//Last variable in draw - make last boolean TRUE for wireframe mode
		m_displayList[i].m_model->Draw(context, *m_states, local, m_view, m_projection, false);

		m_deviceResources->PIXEndEvent();
	}//End for
    m_deviceResources->PIXEndEvent();

	//RENDER TERRAIN
	context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
	context->OMSetDepthStencilState(m_states->DepthDefault(),0);
	context->RSSetState(m_states->CullNone());
    //Uncomment for wireframe
	//context->RSSetState(m_states->Wireframe());		

	//Render the batch
	//This is handled in the display chunk becuase it has the potential to get complex
	m_displayChunk.RenderBatch(m_deviceResources);

    m_deviceResources->Present();
}//End Render

//Helper method to clear the back buffers
void Game::Clear()
{
    m_deviceResources->PIXBeginEvent(L"Clear");

    //Clear the views
    const auto context = m_deviceResources->GetD3DDeviceContext();
    const auto renderTarget = m_deviceResources->GetBackBufferRenderTargetView();
    const auto depthStencil = m_deviceResources->GetDepthStencilView();

    context->ClearRenderTargetView(renderTarget, Colors::CornflowerBlue);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

    //Set the viewport
    const auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    m_deviceResources->PIXEndEvent();
}//End Clear

void XM_CALLCONV Game::DrawGrid(FXMVECTOR xAxis, FXMVECTOR yAxis, FXMVECTOR origin, size_t xDivs, size_t yDivs, GXMVECTOR color)
{
    m_deviceResources->PIXBeginEvent(L"Draw Grid");

    auto context = m_deviceResources->GetD3DDeviceContext();
    context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_states->DepthNone(), 0);
    context->RSSetState(m_states->CullCounterClockwise());

    m_batchEffect->Apply(context);

    context->IASetInputLayout(m_batchInputLayout.Get());

    m_batch->Begin();

    xDivs = std::max<size_t>(1, xDivs);
    yDivs = std::max<size_t>(1, yDivs);

    for (size_t i = 0; i <= xDivs; ++i)
    {
        float fPercent = static_cast<float>(i) / static_cast<float>(xDivs);
        fPercent = (fPercent * 2.0f) - 1.0f;
        XMVECTOR vScale = XMVectorScale(xAxis, fPercent);
        vScale = XMVectorAdd(vScale, origin);

        VertexPositionColor v1(XMVectorSubtract(vScale, yAxis), color);
        VertexPositionColor v2(XMVectorAdd(vScale, yAxis), color);
        m_batch->DrawLine(v1, v2);
    }//End for

    for (size_t i = 0; i <= yDivs; i++)
    {
        float fPercent = static_cast<float>(i) / static_cast<float>(yDivs);
        fPercent = (fPercent * 2.0f) - 1.0f;
        XMVECTOR vScale = XMVectorScale(yAxis, fPercent);
        vScale = XMVectorAdd(vScale, origin);

        VertexPositionColor v1(XMVectorSubtract(vScale, xAxis), color);
        VertexPositionColor v2(XMVectorAdd(vScale, xAxis), color);
        m_batch->DrawLine(v1, v2);
    }//End for

    m_batch->End();

    m_deviceResources->PIXEndEvent();
}//End DrawGrid
#pragma endregion

#pragma region Message Handlers
//Message handlers
void Game::OnActivated()
{
}//End OnActivated

void Game::OnDeactivated()
{
}//End OnDeactivated

void Game::OnSuspending()
{
#ifdef DXTK_AUDIO
    m_audEngine->Suspend();
#endif
}//End OnSuspending

void Game::OnResuming()
{
    m_timer.ResetElapsedTime();

#ifdef DXTK_AUDIO
    m_audEngine->Resume();
#endif
}//End OnResuming

void Game::OnWindowSizeChanged(const int width, const int height)
{
    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;

    CreateWindowSizeDependentResources();
}//End OnWindowSizeChanged

void Game::BuildDisplayList(const std::vector<SceneObject>* sceneGraph)
{
	const auto device = m_deviceResources->GetD3DDevice();

	if (!m_displayList.empty()) m_displayList.clear();

	const int numObjects = sceneGraph->size();
    //For every item in the SceneGraph
	for (int i = 0; i < numObjects; i++)
	{
		//Create a temporary display object that we will populate then append to the display list
		DisplayObject newDisplayObject;
		
		//Load the model
		std::wstring modelwstr = StringToWCHART(sceneGraph->at(i).model_path);
        //Get DXSDK to load model
        //Set final boolean to "false" for left-handed coordinate system (Maya)
		newDisplayObject.m_model = Model::CreateFromCMO(device, modelwstr.c_str(), *m_fxFactory, true);	

		//Load diffuse texture
		std::wstring texturewstr = StringToWCHART(sceneGraph->at(i).tex_diffuse_path);
        //Load texture into shader resource
		const HRESULT rs = CreateDDSTextureFromFile(device, texturewstr.c_str(), nullptr, &newDisplayObject.m_texture_diffuse);	

		//If texture loading fails, load error default
		if (rs)
		{
            //Load texture into shader resource
			CreateDDSTextureFromFile(device, L"database/data/Error.dds", nullptr, &newDisplayObject.m_texture_diffuse);
		}//End if

		//Apply new texture to model's effect
        //Handled using an in-line function via Lambda
		newDisplayObject.m_model->UpdateEffects([&](IEffect* effect)
		{
			const auto lights = dynamic_cast<BasicEffect*>(effect);
			if (lights)
			{
				lights->SetTexture(newDisplayObject.m_texture_diffuse);			
			}//End if
		});

		//Set position
		newDisplayObject.m_position.x = sceneGraph->at(i).posX;
		newDisplayObject.m_position.y = sceneGraph->at(i).posY;
		newDisplayObject.m_position.z = sceneGraph->at(i).posZ;
		
		//Set orientation
		newDisplayObject.m_orientation.x = sceneGraph->at(i).rotX;
		newDisplayObject.m_orientation.y = sceneGraph->at(i).rotY;
		newDisplayObject.m_orientation.z = sceneGraph->at(i).rotZ;

		//Set scale
		newDisplayObject.m_scale.x = sceneGraph->at(i).scaX;
		newDisplayObject.m_scale.y = sceneGraph->at(i).scaY;
		newDisplayObject.m_scale.z = sceneGraph->at(i).scaZ;

		//Set wireframe/render flags
		newDisplayObject.m_render		= sceneGraph->at(i).editor_render;
		newDisplayObject.m_wireframe	= sceneGraph->at(i).editor_wireframe;

        //Set light data
		newDisplayObject.m_light_type		    = sceneGraph->at(i).light_type;
		newDisplayObject.m_light_diffuse_r	    = sceneGraph->at(i).light_diffuse_r;
		newDisplayObject.m_light_diffuse_g	    = sceneGraph->at(i).light_diffuse_g;
		newDisplayObject.m_light_diffuse_b	    = sceneGraph->at(i).light_diffuse_b;
		newDisplayObject.m_light_specular_r     = sceneGraph->at(i).light_specular_r;
		newDisplayObject.m_light_specular_g     = sceneGraph->at(i).light_specular_g;
		newDisplayObject.m_light_specular_b     = sceneGraph->at(i).light_specular_b;
		newDisplayObject.m_light_spot_cutoff    = sceneGraph->at(i).light_spot_cutoff;
		newDisplayObject.m_light_constant	    = sceneGraph->at(i).light_constant;
		newDisplayObject.m_light_linear		    = sceneGraph->at(i).light_linear;
		newDisplayObject.m_light_quadratic	    = sceneGraph->at(i).light_quadratic;
		
		m_displayList.push_back(newDisplayObject);
	}//End for
}//End BuildDisplayList

void Game::BuildDisplayChunk(const ChunkObject* sceneChunk)
{
	//Populate our local display chunk with all the chunk info we need from the object stored in toolmain
	m_displayChunk.PopulateChunkData(sceneChunk);
	m_displayChunk.LoadHeightMap(m_deviceResources);
	m_displayChunk.m_terrainEffect->SetProjection(m_projection);
	m_displayChunk.InitialiseBatch();
}//End BuildDisplayChunk

void Game::SaveDisplayChunk(ChunkObject* sceneChunk)
{
	m_displayChunk.SaveHeightMap();
}//End SaveDisplayChunk

#ifdef DXTK_AUDIO
void Game::NewAudioDevice()
{
    if (m_audEngine && !m_audEngine->IsAudioDevicePresent())
    {
        // Setup a retry in 1 second
        m_audioTimerAcc = 1.f;
        m_retryDefault = true;
    }//End if
}//End NewAudioDevice
#endif
#pragma endregion

#pragma region Direct3D Resources
//These are the resources that depend on the device
void Game::CreateDeviceDependentResources()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto device = m_deviceResources->GetD3DDevice();

    m_states = std::make_unique<CommonStates>(device);

    m_fxFactory = std::make_unique<EffectFactory>(device);
    //Look in the database directory
	m_fxFactory->SetDirectory(L"database/data/");
    //We must set this to false - otherwise, it will share effects based on the initial texture loaded (when the model loads) rather than what we change them to
	m_fxFactory->SetSharing(false);

    m_sprites = std::make_unique<SpriteBatch>(context);

    m_batch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(context);

    m_batchEffect = std::make_unique<BasicEffect>(device);
    m_batchEffect->SetVertexColorEnabled(true);
    
    void const* shaderByteCode;
    size_t byteCodeLength;

    m_batchEffect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);

    DX::ThrowIfFailed(
        device->CreateInputLayout(VertexPositionColor::InputElements,
            VertexPositionColor::InputElementCount,
            shaderByteCode, byteCodeLength,
            m_batchInputLayout.ReleaseAndGetAddressOf())
    );

    m_font = std::make_unique<SpriteFont>(device, L"Resources/SegoeUI_18.spritefont");

	//m_shape = GeometricPrimitive::CreateTeapot(context, 4.f, 8);

    //SDKMESH has to use clockwise winding with right-handed coordinates, so textures are flipped in the U-axis
    m_model = Model::CreateFromSDKMESH(device, L"Resources/tiny.sdkmesh", *m_fxFactory);
	
    //Load textures
    DX::ThrowIfFailed(
        CreateDDSTextureFromFile(device, L"Resources/seafloor.dds", nullptr, m_texture1.ReleaseAndGetAddressOf())
    );

    DX::ThrowIfFailed(
        CreateDDSTextureFromFile(device, L"Resources/windowslogo.dds", nullptr, m_texture2.ReleaseAndGetAddressOf())
    );
}//End CreateDeviceDependentResources

//Allocate all memory resources that change on a window SizeChanged event
void Game::CreateWindowSizeDependentResources()
{
	const auto size = m_deviceResources->GetOutputSize();
	const float aspectRatio = static_cast<float>(size.right) / static_cast<float>(size.bottom);
    float fovAngleY = 70.0f * XM_PI / 180.0f;

    //This is a simple example of change that can be made when the app is in portrait or snapped view
    if (aspectRatio < 1.0f)
    {
        fovAngleY *= 2.0f;
    }//End if

    //This sample makes use of a right-handed coordinate system using row-major matrices
    m_projection = Matrix::CreatePerspectiveFieldOfView(
        fovAngleY,
        aspectRatio,
        0.01f,
        1000.0f
    );

    m_batchEffect->SetProjection(m_projection);
}//End CreateWindowSizeDependentResources

void Game::OnDeviceLost()
{
    m_states.reset();
    m_fxFactory.reset();
    m_sprites.reset();
    m_batch.reset();
    m_batchEffect.reset();
    m_font.reset();
    m_shape.reset();
    m_model.reset();
    m_texture1.Reset();
    m_texture2.Reset();
    m_batchInputLayout.Reset();
}//End OnDeviceLost

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}//End OnDeviceRestored
#pragma endregion

std::wstring StringToWCHART(const std::string s)
{
	const int slength = static_cast<int>(s.length()) + 1;
	const int len = MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, nullptr, 0);
	wchar_t* buf = new wchar_t[len];
	MultiByteToWideChar(CP_ACP, 0, s.c_str(), slength, buf, len);
	std::wstring r(buf);
	delete[] buf;
	return r;
}//End StringToWCHART