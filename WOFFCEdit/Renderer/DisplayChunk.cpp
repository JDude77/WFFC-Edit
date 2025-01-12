#include <string>
#include "DisplayChunk.h"
#include "Game.h"

using namespace DirectX;
using namespace SimpleMath;

DisplayChunk::DisplayChunk():
m_texture_diffuse(nullptr), m_terrainGeometry{}, m_heightMap{}, m_chunk_x_size_metres(0),
m_chunk_y_size_metres(0), m_chunk_base_resolution(0), m_render_wireframe(false), m_render_normals(false),
m_tex_diffuse_tiling(0), m_tex_splat_1_tiling(0), m_tex_splat_2_tiling(0), m_tex_splat_3_tiling(0), m_tex_splat_4_tiling(0)
{
	//Terrain size in meters
	//Note that this is hard coded here
	//We COULD get it from the terrain chunk along with the other info from the tool if we want to be more flexible.
	m_terrainSize = 512;
	m_terrainHeightScale = 0.25; //Convert our 0-256 terrain to 64
	m_textureCoordStep = 1.0 / (TERRAIN_RESOLUTION - 1);
	//-1 becuase its split into chunks, not vertices - we want the last one in each row to have tex coord 1
	m_terrainPositionScalingFactor = m_terrainSize / static_cast<float>(TERRAIN_RESOLUTION - 1);
} //End default constructor

DisplayChunk::~DisplayChunk()
{
}//End destructor

void DisplayChunk::PopulateChunkData(const ChunkObject* sceneChunk)
{
	m_name = sceneChunk->name;

	m_chunk_x_size_metres =		sceneChunk->chunk_x_size_metres;
	m_chunk_y_size_metres =		sceneChunk->chunk_y_size_metres;
	m_chunk_base_resolution =	sceneChunk->chunk_base_resolution;

	m_height_map_path =			sceneChunk->heightmap_path;
	m_tex_diffuse_path =		sceneChunk->tex_diffuse_path;
	m_tex_splat_alpha_path =	sceneChunk->tex_splat_alpha_path;
	m_tex_splat_1_path =		sceneChunk->tex_splat_1_path;
	m_tex_splat_2_path =		sceneChunk->tex_splat_2_path;
	m_tex_splat_3_path =		sceneChunk->tex_splat_3_path;
	m_tex_splat_4_path =		sceneChunk->tex_splat_4_path;

	m_render_wireframe =		sceneChunk->render_wireframe;
	m_render_normals =			sceneChunk->render_normals;

	m_tex_diffuse_tiling =		sceneChunk->tex_diffuse_tiling;
	m_tex_splat_1_tiling =		sceneChunk->tex_splat_1_tiling;
	m_tex_splat_2_tiling =		sceneChunk->tex_splat_2_tiling;
	m_tex_splat_3_tiling =		sceneChunk->tex_splat_3_tiling;
	m_tex_splat_4_tiling =		sceneChunk->tex_splat_4_tiling;
}//End PopulateChunkData

void DisplayChunk::RenderBatch(const std::shared_ptr<DX::DeviceResources> devResources)
{
	ID3D11DeviceContext* context = devResources->GetD3DDeviceContext();

	m_terrainEffect->Apply(context);
	context->IASetInputLayout(m_terrainInputLayout.Get());

	m_batch->Begin();
	//Looping through quads
	//We subtract one from the terrain array or it will try to draw a quad starting with the last vertex in each row
	for (size_t i = 0; i < TERRAIN_RESOLUTION-1; i++)	
	{
		//Same as above
		for (size_t j = 0; j < TERRAIN_RESOLUTION-1; j++)
		{
			m_batch->DrawQuad(m_terrainGeometry[i][j], m_terrainGeometry[i][j+1], m_terrainGeometry[i+1][j+1], m_terrainGeometry[i+1][j]); //bottom left bottom right, top right top left.
		}//End for
	}//End for
	m_batch->End();
}//End RenderBatch

void DisplayChunk::InitialiseBatch()
{
	//Build geometry for our terrain array
	int index = 0;

	//Iterate through all the vertices of our required resolution terrain
	for (size_t i = 0; i < TERRAIN_RESOLUTION; i++)
	{
		for (size_t j = 0; j < TERRAIN_RESOLUTION; j++)
		{
			index = TERRAIN_RESOLUTION * i + j;
			//This will create a terrain going from -64->64, rather than 0->128, so that the center of the terrain is on the origin
			m_terrainGeometry[i][j].position = Vector3
			(
				j * m_terrainPositionScalingFactor - 0.5 * m_terrainSize,
				static_cast<float>(m_heightMap[index]) * m_terrainHeightScale,
				i * m_terrainPositionScalingFactor - 0.5 * m_terrainSize
			);
			//Standard y = up
			m_terrainGeometry[i][j].normal = Vector3(0.0f, 1.0f, 0.0f);
			//Spread tex coords so that it's distributed evenly across the terrain from 0-1
			m_terrainGeometry[i][j].textureCoordinate =	Vector2
			(
				m_textureCoordStep * j * m_tex_diffuse_tiling,
				m_textureCoordStep * i * m_tex_diffuse_tiling
			);
		}//End j for
	}//End i for
	CalculateTerrainNormals();
}//End InitialiseBatch

void DisplayChunk::LoadHeightMap(const std::shared_ptr<DX::DeviceResources> devResources)
{
	auto device = devResources->GetD3DDevice();
	auto devicecontext = devResources->GetD3DDeviceContext();

	//Load in raw heightmap
	FILE *pFile = fopen(m_height_map_path.c_str(), "rb");

	//Check to see if we found the file and could open it
	if (pFile == nullptr)
	{
		//Display error message and stop the function
		MessageBox(nullptr, L"Can't find the height map!", L"Error", MB_OK);
		return;
	}//End if

	//Load the .RAW file into our pHeightMap data array
	//We are only reading in '1', and the size is width * height
	fread(m_heightMap, 1, TERRAIN_RESOLUTION*TERRAIN_RESOLUTION, pFile);

	fclose(pFile);
	
	//Load the diffuse texture
	const std::wstring texturewstr = StringToWCHART(m_tex_diffuse_path);
	//Load texture into shader resource	view and resource
	HRESULT rs = CreateDDSTextureFromFile(device, texturewstr.c_str(), nullptr, &m_texture_diffuse);	
	
	//Set up terrain effect
	m_terrainEffect = std::make_unique<BasicEffect>(device);
	m_terrainEffect->EnableDefaultLighting();
	m_terrainEffect->SetLightingEnabled(true);
	m_terrainEffect->SetTextureEnabled(true);
	m_terrainEffect->SetTexture(m_texture_diffuse);

	void const* shaderByteCode;
	size_t byteCodeLength;

	m_terrainEffect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);

	//Set up batch
	DX::ThrowIfFailed(
		device->CreateInputLayout(VertexPositionNormalTexture::InputElements,
			VertexPositionNormalTexture::InputElementCount,
			shaderByteCode,
			byteCodeLength,
			m_terrainInputLayout.GetAddressOf())
		);

	m_batch = std::make_unique<PrimitiveBatch<VertexPositionNormalTexture>>(devicecontext);
}//End LoadHeightMap

void DisplayChunk::SaveHeightMap()
{
	//I have no idea why this is kept here, this is a Matt original comment and I'm just keeping it for posterity
	/*for (size_t i = 0; i < TERRAIN_RESOLUTION * TERRAIN_RESOLUTION; i++)
	{
		m_heightMap[i] = 0;
	}*/

	FILE *pFile = fopen(m_height_map_path.c_str(), "wb+");

	//Check to see if we found the file and could open it
	if (pFile == nullptr)
	{
		//Display error message and stop the function
		MessageBox(nullptr, L"Can't Find The Height Map!", L"Error", MB_OK);
		return;
	}//End if

	fwrite(m_heightMap, 1, TERRAIN_RESOLUTION*TERRAIN_RESOLUTION, pFile);
	fclose(pFile);
}//End SaveHeightMap

void DisplayChunk::UpdateTerrain()
{
	for (size_t i = 0; i < TERRAIN_RESOLUTION; i++)
	{
		for (size_t j = 0; j < TERRAIN_RESOLUTION; j++)
		{
			const int index = TERRAIN_RESOLUTION * i + j;
			m_terrainGeometry[i][j].position.y = static_cast<float>(m_heightMap[index]) * m_terrainHeightScale;	
		}//End j for
	}//End i for
	CalculateTerrainNormals();
}//End UpdateTerrain

void DisplayChunk::GenerateHeightmap()
{
	//TODO: Code heightmap generation
}//End GenerateHeightmap

void DisplayChunk::CalculateTerrainNormals()
{
	int index1, index2, index3, index4;
	Vector3 normalVector;

	for (int i = 0; i < (TERRAIN_RESOLUTION - 1); i++)
	{
		for (int j = 0; j < (TERRAIN_RESOLUTION - 1); j++)
		{
			Vector3 leftRightVector;
			Vector3 upDownVector;
			upDownVector.x = (m_terrainGeometry[i + 1][j].position.x - m_terrainGeometry[i - 1][j].position.x);
			upDownVector.y = (m_terrainGeometry[i + 1][j].position.y - m_terrainGeometry[i - 1][j].position.y);
			upDownVector.z = (m_terrainGeometry[i + 1][j].position.z - m_terrainGeometry[i - 1][j].position.z);

			leftRightVector.x = (m_terrainGeometry[i][j - 1].position.x - m_terrainGeometry[i][j + 1].position.x);
			leftRightVector.y = (m_terrainGeometry[i][j - 1].position.y - m_terrainGeometry[i][j + 1].position.y);
			leftRightVector.z = (m_terrainGeometry[i][j - 1].position.z - m_terrainGeometry[i][j + 1].position.z);

			leftRightVector.Cross(upDownVector, normalVector);
			normalVector.Normalize();

			m_terrainGeometry[i][j].normal = normalVector;
		}//End j for
	}//End i for
}//End CalculateTerrainNormals