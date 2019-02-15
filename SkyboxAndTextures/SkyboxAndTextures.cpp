#include "Framework.h"

#include "ShaderSet.h"
#include "Mesh.h"
#include "Texture.h"

//================================================================================
// Skybox and textures Application
// An example of how to use selected parts of this framework.
//================================================================================

class SkyboxAndTexturesApp : public FrameworkApp
{
public:

	struct PerFrameCBData
	{
		m4x4 m_matProjection;
		m4x4 m_matView;
		f32		m_time;
		f32     m_padding[3];
	};

	struct PerDrawCBData
	{
		m4x4 m_matMVP;
		m4x4 m_modelMatrix;
		v3 eyePos;
		float padding[1];
	};
	

	struct ExtraDataCBData
	{
		f32 m_uvScale;
		f32 m_padding[3];
	};

	void on_init(SystemsInterface& systems) override
	{
		m_position = v3(0.5f, 0.5f, 0.5f);
		m_size = 1.0f;
		systems.pCamera->eye = v3(10.f, 5.f, 7.f);
		systems.pCamera->look_at(v3(3.f, 0.5f, 0.f));

		// compile a set of shaders
		m_meshShader.init(systems.pD3DDevice
			, ShaderSetDesc::Create_VS_PS("Assets/Shaders/MinimalShaders.fx", "VS_Mesh", "PS_Mesh")
			, { VertexFormatTraits<MeshVertex>::desc, VertexFormatTraits<MeshVertex>::size }
		);
		m_uvShaders.init(systems.pD3DDevice
			, ShaderSetDesc::Create_VS_PS("Assets/Shaders/UVShaders.fx", "VS_UVShaders", "PS_UVShaders")
			, { VertexFormatTraits<MeshVertex>::desc, VertexFormatTraits<MeshVertex>::size }
		);
		m_skyboxShaders.init(systems.pD3DDevice
			, ShaderSetDesc::Create_VS_PS("Assets/Shaders/SkyboxShaders.fx", "VS_Skybox", "PS_Skybox")
			, { VertexFormatTraits<MeshVertex>::desc, VertexFormatTraits<MeshVertex>::size }
		); 
		m_reflectionShader.init(systems.pD3DDevice
			, ShaderSetDesc::Create_VS_PS("Assets/Shaders/ReflectionShader.fx", "VS_Skybox", "PS_Skybox")
			, { VertexFormatTraits<MeshVertex>::desc, VertexFormatTraits<MeshVertex>::size }
		);

		// Create Per Frame Constant Buffer.
		m_pPerFrameCB = create_constant_buffer<PerFrameCBData>(systems.pD3DDevice);

		// Create Per Frame Constant Buffer.
		m_pPerDrawCB = create_constant_buffer<PerDrawCBData>(systems.pD3DDevice);

		// Extra data constant buffer
		m_pExtraDataCB = create_constant_buffer<ExtraDataCBData>(systems.pD3DDevice);


		// Initialize a mesh directly.
		create_mesh_cube(systems.pD3DDevice, m_meshArray[0], 0.5f);

		// load a grid mesh.
		create_mesh_from_obj(systems.pD3DDevice, m_plane, "Assets/Models/plane.obj", 8.f);

		// Initialize a mesh from an .OBJ file
		create_mesh_from_obj(systems.pD3DDevice, m_meshArray[1], "Assets/Models/apple.obj", 0.01f);

		// Initialise some textures;
		m_textures[0].init_from_dds(systems.pD3DDevice, "Assets/Textures/brick.dds");
		m_textures[1].init_from_dds(systems.pD3DDevice, "Assets/Textures/apple_diffuse.dds");

		// Various checkerboard textures to experiment with
		m_planeTextures[0].init_from_dds(systems.pD3DDevice, "Assets/Textures/checkerboard_nomips.dds");
		m_planeTextures[1].init_from_dds(systems.pD3DDevice, "Assets/Textures/checkerboard_mips.dds");
		m_planeTextures[2].init_from_dds(systems.pD3DDevice, "Assets/Textures/checkerboard_mips_bc3.dds");
		m_planeTextures[3].init_from_dds(systems.pD3DDevice, "Assets/Textures/miplevels.dds");

		// Load the skybox
		m_skyboxCubeMap.init_from_dds(systems.pD3DDevice, "Assets/Textures/sahara_cube.dds");


		// We need a sampler state to define wrapping and mipmap parameters.
		m_pLinearMipSamplerState = create_basic_sampler(systems.pD3DDevice, D3D11_TEXTURE_ADDRESS_WRAP);


		// build more samplers
		{
			HRESULT hr;

			D3D11_SAMPLER_DESC desc = {};
			desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
			desc.MinLOD = 0.f;
			desc.MaxLOD = D3D11_FLOAT32_MAX;
			desc.MaxAnisotropy = 0.f;

			hr = systems.pD3DDevice->CreateSamplerState(&desc, &m_samplerStates[0]);
			ASSERT(SUCCEEDED(hr));

			desc.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			hr = systems.pD3DDevice->CreateSamplerState(&desc, &m_samplerStates[1]);
			ASSERT(SUCCEEDED(hr));

			desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			hr = systems.pD3DDevice->CreateSamplerState(&desc, &m_samplerStates[2]);
			ASSERT(SUCCEEDED(hr));

			desc.Filter = D3D11_FILTER_ANISOTROPIC;
			desc.MaxAnisotropy = 16.f;
			hr = systems.pD3DDevice->CreateSamplerState(&desc, &m_samplerStates[3]);
			ASSERT(SUCCEEDED(hr));
		}


		// Setup per-frame data
		m_perFrameCBData.m_time = 0.0f;
	}

	void on_update(SystemsInterface& systems) override
	{		// This function displays some useful debugging values, camera positions etc.
		DemoFeatures::editorHud(systems.pDebugDrawContext);
		// Update Per Frame Data.
		m_perFrameCBData.m_matProjection = systems.pCamera->projMatrix.Transpose();
		m_perFrameCBData.m_matView = systems.pCamera->viewMatrix.Transpose();
		m_perFrameCBData.m_time += 0.001f;
	}

	void on_render(SystemsInterface& systems) override
	{
		auto ctx = systems.pDebugDrawContext;

		//dd::xzSquareGrid(ctx, -50.0f, 50.0f, 0.0f, 1.f, dd::colors::DimGray);

		// Push Per Frame Data to GPU
		D3D11_MAPPED_SUBRESOURCE subresource;
		if (!FAILED(systems.pD3DContext->Map(m_pPerFrameCB, 0, D3D11_MAP_WRITE_DISCARD, 0, &subresource)))
		{
			memcpy(subresource.pData, &m_perFrameCBData, sizeof(PerFrameCBData));
			systems.pD3DContext->Unmap(m_pPerFrameCB, 0);
		}

		// Bind our set of shaders.
		m_reflectionShader.bind(systems.pD3DContext);

		// Bind Constant Buffers, to both PS and VS stages
		ID3D11Buffer* buffers[] = { m_pPerFrameCB, m_pPerDrawCB, m_pExtraDataCB };
		systems.pD3DContext->VSSetConstantBuffers(0, 3, buffers);
		systems.pD3DContext->PSSetConstantBuffers(0, 3, buffers);

		// Bind a sampler state
		{
			ID3D11SamplerState* samplers[] = { m_pLinearMipSamplerState };
			systems.pD3DContext->PSSetSamplers(0, 1, samplers);
		}


		constexpr f32 kGridSpacing = 1.5f;
		constexpr u32 kNumInstances = 5;
		constexpr u32 kNumModelTypes = 2;
		m_skyboxCubeMap.bind(systems.pD3DContext, ShaderStage::kPixel, 0);
		m_perDrawCBData.eyePos = systems.pCamera->eye;

		for (u32 i = 0; i < kNumModelTypes; ++i)
		{
			// Bind a mesh and texture.
			m_meshArray[i].bind(systems.pD3DContext);
			//m_textures[i].bind(systems.pD3DContext, ShaderStage::kPixel, 0);

			// Draw several instances
			for (u32 j = 0; j < kNumInstances; ++j)
			{
				// Compute MVP matrix.
				m4x4 matModel = m4x4::CreateTranslation(v3(j * kGridSpacing, i * kGridSpacing + 1.0, 0.f));
				m4x4 matMVP = matModel * systems.pCamera->vpMatrix;
				
				// Update Per Draw Data
				m_perDrawCBData.m_matMVP = matMVP.Transpose();
				m_perDrawCBData.m_modelMatrix = matModel.Transpose();
				// Push to GPU
				push_constant_buffer(systems.pD3DContext, m_pPerDrawCB, m_perDrawCBData);

				// Draw the mesh.
				m_meshArray[i].draw(systems.pD3DContext);
			}
		}

		// Draw Ground plane
		{

			// Bind our set of shaders.
			m_uvShaders.bind(systems.pD3DContext);

			m4x4 matMVP = systems.pCamera->vpMatrix;
			m_perDrawCBData.m_matMVP = matMVP.Transpose();
			push_constant_buffer(systems.pD3DContext, m_pPerDrawCB, m_perDrawCBData);

			// Update extra shader data
			m_extraDataCBData.m_uvScale = 200.0;
			push_constant_buffer(systems.pD3DContext, m_pExtraDataCB, m_extraDataCBData);


			// Bind the texture
			{
				static u32 planeTextureIndex = 0;
				ImGui::SliderInt("Plane Texture", (int*)&planeTextureIndex, 0, kMaxPlaneTextures - 1);

				m_planeTextures[planeTextureIndex].bind(systems.pD3DContext, ShaderStage::kPixel, 0);
			}

			// Bind a sampler state
			{
				static u32 samplerStateIndex = 0;
				ImGui::SliderInt("Sampler State", (int*)&samplerStateIndex, 0, kMaxSamplerStates - 1);

				ID3D11SamplerState* samplers[] = { m_samplerStates[samplerStateIndex] };
				systems.pD3DContext->PSSetSamplers(0, 1, samplers);
			}

			m_plane.bind(systems.pD3DContext);
			m_plane.draw(systems.pD3DContext);
		}


		// Draw the sky box.
		{

			// Bind our set of shaders.
			m_skyboxShaders.bind(systems.pD3DContext);

			m4x4 matView = systems.pCamera->viewMatrix;
			matView.m[3][0] = 0.f;
			matView.m[3][1] = 0.f;
			matView.m[3][2] = 0.f;
			matView.m[3][3] = 1.f;

			m4x4 matMVP = matView * systems.pCamera->projMatrix;
			m_perDrawCBData.m_matMVP = matMVP.Transpose();
			push_constant_buffer(systems.pD3DContext, m_pPerDrawCB, m_perDrawCBData);


			// Bind the texture
			{
				m_skyboxCubeMap.bind(systems.pD3DContext, ShaderStage::kPixel, 0);
			}

			// Bind a sampler state
			{
				ID3D11SamplerState* samplers[] = { m_pLinearMipSamplerState };
				systems.pD3DContext->PSSetSamplers(0, 1, samplers);
			}

			// draw the box at camera center.
			// shader does the rest.
			m_meshArray[0].bind(systems.pD3DContext);
			m_meshArray[0].draw(systems.pD3DContext);
		}


	}

	void on_resize(SystemsInterface&) override
	{

	}

private:

	PerFrameCBData m_perFrameCBData;
	ID3D11Buffer* m_pPerFrameCB = nullptr;

	PerDrawCBData m_perDrawCBData;
	ID3D11Buffer* m_pPerDrawCB = nullptr;

	ExtraDataCBData m_extraDataCBData;
	ID3D11Buffer* m_pExtraDataCB = nullptr;

	ShaderSet m_meshShader;
	ShaderSet m_uvShaders;
	ShaderSet m_reflectionShader;

	// Skybox related
	ShaderSet m_skyboxShaders;
	Texture m_skyboxCubeMap;

	Mesh m_plane;
	static constexpr u32 kMaxPlaneTextures = 4;
	Texture m_planeTextures[kMaxPlaneTextures];

	Mesh m_meshArray[2];
	Texture m_textures[2];
	ID3D11SamplerState* m_pLinearMipSamplerState = nullptr;
	ID3D11SamplerState* m_pCubemapSamplerState = nullptr;

	// experimental samplers
	static constexpr u32 kMaxSamplerStates = 4;
	ID3D11SamplerState* m_samplerStates[kMaxSamplerStates];

	v3 m_position;
	f32 m_size;
};

SkyboxAndTexturesApp g_app;

FRAMEWORK_IMPLEMENT_MAIN(g_app, "Skybox and Textures")
