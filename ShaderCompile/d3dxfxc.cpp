//====== Copyright © 1996-2006, Valve Corporation, All rights reserved. =======//
//
// Purpose: D3DX command implementation.
//
// $NoKeywords: $
//
//=============================================================================//

#define WIN32_LEAN_AND_MEAN
#define NOWINRES
#define NOSERVICE
#define NOMCX
#define NOIME
#define NOMINMAX

#include "d3dxfxc.h"

#include "basetypes.h"
#include "cfgprocessor.h"
#include "cmdsink.h"
#include "d3dcompiler.h"
#include "gsl/narrow"
#include <malloc.h>
#include <vector>

#if defined(SC_BUILD_PS1_X_COMPILER)
#include <D3DX9Shader.h>

#pragma comment(lib, "d3dx9")
#endif  // SC_BUILD_PS1_X_COMPILER

#pragma comment( lib, "D3DCompiler" )

CSharedFile::CSharedFile( std::vector<char>&& data ) noexcept : std::vector<char>( std::forward<std::vector<char>>( data ) )
{
}

void FileCache::Add( const std::string& fileName, std::vector<char>&& data )
{
	const auto& it = m_map.find( fileName );
	if ( it != m_map.end() )
		return;

	CSharedFile file( std::forward<std::vector<char>>( data ) );
	m_map.emplace( fileName, std::move( file ) );
}

const CSharedFile* FileCache::Get( const std::string& filename ) const
{
	// Search the cache first
	const auto find = m_map.find( filename );
	if ( find != m_map.cend() )
		return &find->second;
	return nullptr;
}

void FileCache::Clear()
{
	m_map.clear();
}

FileCache fileCache;

static struct DxIncludeImpl final : public ID3DInclude
{
	STDMETHOD( Open )( THIS_ D3D_INCLUDE_TYPE, LPCSTR pFileName, LPCVOID, LPCVOID* ppData, UINT* pBytes ) override
	{
		const CSharedFile* file = fileCache.Get( pFileName );
		if ( !file )
			return E_FAIL;

		*ppData = file->Data();
		*pBytes = gsl::narrow<UINT>( file->Size() );

		return S_OK;
	}

	STDMETHOD( Close )( THIS_ LPCVOID ) override
	{
		return S_OK;
	}

	virtual ~DxIncludeImpl() = default;
} s_incDxImpl;

class CResponse final : public CmdSink::IResponse
{
public:
	explicit CResponse( ID3DBlob* pShader, ID3DBlob* pListing, HRESULT hr ) noexcept
		: m_pShader( pShader )
		, m_pListing( pListing )
		, m_hr( hr )
	{
	}

	~CResponse() override
	{
		if ( m_pShader )
			m_pShader->Release();

		if ( m_pListing )
			m_pListing->Release();
	}

	bool Succeeded() const noexcept override { return m_pShader && m_hr == S_OK; }
	size_t GetResultBufferLen() const override { return Succeeded() ? m_pShader->GetBufferSize() : 0; }
	const void* GetResultBuffer() const override { return Succeeded() ? m_pShader->GetBufferPointer() : nullptr; }
	const char* GetListing() const override { return static_cast<const char*>( m_pListing ? m_pListing->GetBufferPointer() : nullptr ); }

protected:
	ID3DBlob* m_pShader;
	ID3DBlob* m_pListing;
	HRESULT m_hr;
};


#if defined(SC_BUILD_PS1_X_COMPILER)

namespace
{

// DirectX 11 -> DirectX 9 wrappers.

class D3DXInclude : public ID3DXInclude
{
public:
	explicit D3DXInclude(ID3DInclude* inner) noexcept : m_inner{inner} {}

	STDMETHODIMP Open(D3DXINCLUDE_TYPE IncludeType, LPCSTR pFileName,
		LPCVOID pParentData, LPCVOID* ppData,
		UINT* pBytes) override
	{
		return m_inner->Open(GetD3dIncludeType(IncludeType), pFileName, pParentData, ppData, pBytes);
	}

	STDMETHODIMP Close(LPCVOID pData) override
	{
		return m_inner->Close(pData);
	}

private:
	ID3DInclude* m_inner;

	static D3D_INCLUDE_TYPE GetD3dIncludeType(D3DXINCLUDE_TYPE dx_include) noexcept
	{
		switch (dx_include)
		{
			case D3DXINC_LOCAL:
				return D3D_INCLUDE_LOCAL;
			case D3DXINC_SYSTEM:
				return D3D_INCLUDE_SYSTEM;
			default:
				// Failed, return code which should cause failure later.
				return D3D_INCLUDE_FORCE_DWORD;
		}
	}
};

// Lifetime of ID3DXBuffer is tied to D3DBlob.  ID3DXBuffer counter is
// already 1.
class D3DBlob : public ID3DBlob
{
public:
	explicit D3DBlob(ID3DXBuffer* buffer) noexcept : m_buffer{buffer} {}
	~D3DBlob() noexcept = default;

	// IUnknown
	STDMETHODIMP QueryInterface(REFIID iid, LPVOID* ppv) override
	{
		return m_buffer->QueryInterface(iid, ppv);
	}
	STDMETHODIMP_(ULONG) AddRef() override
	{
		return m_buffer->AddRef();
	}
	STDMETHODIMP_(ULONG) Release() override
	{
		const ULONG rc = m_buffer->Release();
		if (rc == 0) delete this;
		return rc;
	}
	
	// ID3DXBuffer
	STDMETHODIMP_(LPVOID) GetBufferPointer() override
	{
		return m_buffer->GetBufferPointer();
	}
	STDMETHODIMP_(SIZE_T) GetBufferSize() override
	{
		return m_buffer->GetBufferSize();
	}

private:
	ID3DXBuffer* m_buffer;
};

DWORD GetD3dxFlagsFromD3dOnes(UINT d3dFlags1, UINT d3dFlags2) noexcept
{
	DWORD d3dx_flags{0};
	
	if (d3dFlags1 & D3DCOMPILE_DEBUG)
	{
		d3dx_flags |= D3DXSHADER_DEBUG;
	}
	
	if (d3dFlags1 & D3DCOMPILE_SKIP_VALIDATION)
	{
		d3dx_flags |= D3DXSHADER_SKIPVALIDATION;
	}
	
	if (d3dFlags1 & D3DCOMPILE_SKIP_OPTIMIZATION)
	{
		d3dx_flags |= D3DXSHADER_SKIPOPTIMIZATION;
	}
	
	if (d3dFlags1 & D3DCOMPILE_PACK_MATRIX_ROW_MAJOR)
	{
		d3dx_flags |= D3DXSHADER_PACKMATRIX_ROWMAJOR;
	}
	
	if (d3dFlags1 & D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR)
	{
		d3dx_flags |= D3DXSHADER_PACKMATRIX_COLUMNMAJOR;
	}
	
	if (d3dFlags1 & D3DCOMPILE_PARTIAL_PRECISION)
	{
		d3dx_flags |= D3DXSHADER_PARTIALPRECISION;
	}
	
	if (d3dFlags1 & D3DCOMPILE_FORCE_VS_SOFTWARE_NO_OPT)
	{
		// This flag was applicable only to Direct3D 9.
		d3dx_flags |= D3DXSHADER_FORCE_VS_SOFTWARE_NOOPT;
	}
	
	if (d3dFlags1 & D3DCOMPILE_FORCE_PS_SOFTWARE_NO_OPT)
	{
		// This flag was applicable only to Direct3D 9.
		d3dx_flags |= D3DXSHADER_FORCE_PS_SOFTWARE_NOOPT;
	}
	
	if (d3dFlags1 & D3DCOMPILE_NO_PRESHADER)
	{
		// This flag was only applicable to legacy Direct3D 9 and Direct3D 10
		// Effects (FX).
		d3dx_flags |= D3DXSHADER_NO_PRESHADER;
	}
	
	if (d3dFlags1 & D3DCOMPILE_AVOID_FLOW_CONTROL)
	{
		d3dx_flags |= D3DXSHADER_AVOID_FLOW_CONTROL;
	}
	
	if (d3dFlags1 & D3DCOMPILE_IEEE_STRICTNESS)
	{
		d3dx_flags |= D3DXSHADER_IEEE_STRICTNESS;
	}
	
	if (d3dFlags1 & D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY)
	{
		d3dx_flags |= D3DXSHADER_ENABLE_BACKWARDS_COMPATIBILITY;
	}
	
	if (d3dFlags1 & D3DCOMPILE_OPTIMIZATION_LEVEL0)
	{
		d3dx_flags |= D3DXSHADER_OPTIMIZATION_LEVEL0;
	}
	
	if (d3dFlags1 & D3DCOMPILE_OPTIMIZATION_LEVEL1)
	{
		d3dx_flags |= D3DXSHADER_OPTIMIZATION_LEVEL1;
	}
	
	if (d3dFlags1 & D3DCOMPILE_OPTIMIZATION_LEVEL2)
	{
		d3dx_flags |= D3DXSHADER_OPTIMIZATION_LEVEL2;
	}
	
	if (d3dFlags1 & D3DCOMPILE_OPTIMIZATION_LEVEL3)
	{
		d3dx_flags |= D3DXSHADER_OPTIMIZATION_LEVEL3;
	}
	
	// These have no D3DX equivalents.
	// D3DCOMPILE_ENABLE_STRICTNESS
	// D3DCOMPILE_WARNINGS_ARE_ERRORS
	// D3DCOMPILE_RESOURCES_MAY_ALIAS
	// D3DCOMPILE_ENABLE_UNBOUNDED_DESCRIPTOR_TABLES
	// D3DCOMPILE_ALL_RESOURCES_BOUND
	// D3DCOMPILE_DEBUG_NAME_FOR_SOURCE
	// D3DCOMPILE_DEBUG_NAME_FOR_BINARY
	
	return d3dx_flags;
}

bool IsTargetEqual(std::string_view target, std::string_view expected) noexcept
{
	return target == expected;
}

}  // namespace

#endif  // SC_BUILD_PS1_X_COMPILER


void Compiler::ExecuteCommand( const CfgProcessor::ComboBuildCommand& pCommand, CmdSink::IResponse* &pResponse, unsigned int flags )
{
	// Macros to be defined for D3DX
	std::vector<D3D_SHADER_MACRO> macros;
	macros.resize( pCommand.defines.size() + 1 );
	std::transform( pCommand.defines.cbegin(), pCommand.defines.cend(), macros.begin(), []( const auto& d ) { return D3D_SHADER_MACRO{ d.first.data(), d.second.data() }; } );

	ID3DBlob* pShader        = nullptr; // NOTE: Must release the COM interface later
	ID3DBlob* pErrorMessages = nullptr; // NOTE: Must release COM interface later

	LPCVOID lpcvData = nullptr;
	UINT numBytes    = 0;
	HRESULT hr       = s_incDxImpl.Open( D3D_INCLUDE_LOCAL, pCommand.fileName.data(), nullptr, &lpcvData, &numBytes );
	if ( !FAILED( hr ) )
	{
		std::string_view target = pCommand.shaderModel;

#if defined(SC_BUILD_PS1_X_COMPILER)
		// The current HLSL shader D3DCompile* functions don't support legacy 1.x
		// pixel shaders.  The last version of HLSL to support these targets was D3DX9
		// in the October 2006 release of the DirectX SDK.
		if (!IsTargetEqual(target, "ps_1_1") && !IsTargetEqual(target, "ps_1_2") &&
			!IsTargetEqual(target, "ps_1_3") && !IsTargetEqual(target, "ps_1_4"))
		{
#endif  // SC_BUILD_PS1_X_COMPILER
			hr = D3DCompile( lpcvData, numBytes, pCommand.fileName.data(),
				macros.data(), &s_incDxImpl, pCommand.entryPoint.data(),
				target.data(), flags, 0, &pShader, &pErrorMessages );
#if defined(SC_BUILD_PS1_X_COMPILER)
		}
		else
		{
			static_assert(sizeof(*macros.data()) == sizeof(D3DXMACRO),
				"Ensure D3D_SHADER_MACRO and D3DXMACRO are same size.");
			static_assert(alignof(decltype(*macros.data())) == alignof(D3DXMACRO),
				"Ensure D3D_SHADER_MACRO and D3DXMACRO are same alignment.");
			
			D3DXInclude d3dx_include_wrapper{&s_incDxImpl};
			ID3DXInclude* d3dx_include{&d3dx_include_wrapper};
			
			ID3DXBuffer *d3dx_shader{nullptr}, *d3dx_errors{nullptr};
			
			hr = D3DXCompileShader(static_cast<const char*>(lpcvData), numBytes,
				reinterpret_cast<const D3DXMACRO*>(macros.data()),
				d3dx_include, pCommand.entryPoint.data(), target.data(),
				GetD3dxFlagsFromD3dOnes(flags, 0),
				&d3dx_shader, &d3dx_errors, nullptr);
			if (SUCCEEDED(hr))
			{
				pShader = new D3DBlob{d3dx_shader};
				pErrorMessages = new D3DBlob{d3dx_errors};
			}
		}
#endif  // SC_BUILD_PS1_X_COMPILER

		// Close the file
		s_incDxImpl.Close( lpcvData );
	}

	pResponse = new( std::nothrow ) CResponse( pShader, pErrorMessages, hr );
}