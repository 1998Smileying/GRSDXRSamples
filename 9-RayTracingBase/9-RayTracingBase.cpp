#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // �� Windows ͷ���ų�����ʹ�õ�����
#include <windows.h>
#include <tchar.h>
#include <wrl.h>  //���WTL֧�� ����ʹ��COM
#include <strsafe.h>
#include <atlbase.h>
#include <atlcoll.h> //for atl array
#include <atlconv.h> //for T2A
#include <stdlib.h>
#include <sstream>
#include <iomanip>
#include <list>
#include <string>
#include <shellapi.h>
#include <memory>
#include <unordered_map>
#include <vector>
#include <assert.h>
#include <fstream>  //for ifstream
#include <dxgi1_6.h>
#include <DirectXMath.h>
#include <d3d12.h>//for d3d12
#include <d3d12shader.h>
#include <d3dcompiler.h>
#if defined(_DEBUG)
#include <dxgidebug.h>
#endif

#include "..\Commons\GRSCOMException.h"
#include "..\Commons\GRSD3DInclude.h"
#include "Shader\RayTracingHlslCompat.h" //shader �� C++������ʹ����ͬ��ͷ�ļ����峣���ṹ�� �Լ�����ṹ���

#include "../RayTracingFallback/Libraries/D3D12RaytracingFallback/Include/d3dx12.h"
#include "../RayTracingFallback/Libraries/D3D12RaytracingFallback/Include/d3d12_1.h"
#include "../RayTracingFallback/Libraries/D3D12RaytracingFallback/Include/D3D12RaytracingFallback.h"
#include "../RayTracingFallback/Libraries/D3D12RaytracingFallback/Include/D3D12RaytracingHelpers.hpp"

#include "Debug/x64/CompiledShaders/Raytracing.hlsl.h"

using namespace std;
using namespace Microsoft;
using namespace Microsoft::WRL;
using namespace DirectX;

//------------------------------------------------------------------------------------------------------------
//linker
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")
//------------------------------------------------------------------------------------------------------------

#define GRS_WND_CLASS_NAME _T("Game Window Class")
#define GRS_WND_TITLE	_T("GRS DirectX12 RayTracing Base Sample")

//�¶���ĺ�������ȡ������
#define GRS_UPPER_DIV(A,B) ((UINT)(((A)+((B)-1))/(B)))
//���������ϱ߽�����㷨 �ڴ�����г��� ���ס
#define GRS_UPPER(A,B) ((UINT)(((A)+((B)-1))&~(B - 1)))

#define GRS_UPPER_SIZEOFUINT32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)

#define GRS_SAFE_RELEASE(p) if(p){(p)->Release();(p)=nullptr;}
#define GRS_THROW_IF_FAILED(hr) { HRESULT _hr = (hr); if (FAILED(_hr)){ throw CGRSCOMException(_hr); } }
#define GRS_THROW_IF_FALSE(b) {  if (!(b)){ throw CGRSCOMException(0xF0000001); } }

//����̨������������
#if defined(_DEBUG)
//ʹ�ÿ���̨���������Ϣ��������̵߳���
#define GRS_INIT_OUTPUT() 	if (!::AllocConsole()){throw CGRSCOMException(HRESULT_FROM_WIN32(GetLastError()));}
#define GRS_FREE_OUTPUT()	::FreeConsole();
#define GRS_USEPRINTF() TCHAR pBuf[1024] = {};TCHAR pszOutput[1024] = {};
#define GRS_PRINTF(...) \
    StringCchPrintf(pBuf,1024,__VA_ARGS__);\
	StringCchPrintf(pszOutput,1024,_T("��ID=0x%04x����%s"),::GetCurrentThreadId(),pBuf);\
    WriteConsole(GetStdHandle(STD_OUTPUT_HANDLE),pszOutput,lstrlen(pszOutput),NULL,NULL);
#else
#define GRS_INIT_OUTPUT()
#define GRS_FREE_OUTPUT()
#define GRS_USEPRINTF()
#define GRS_PRINTF(...)
#endif
//------------------------------------------------------------------------------------------------------------
// Ϊ�˵��Լ�����������������ͺ궨�壬Ϊÿ���ӿڶ����������ƣ�����鿴�������
#if defined(_DEBUG)
inline void GRS_SetD3D12DebugName(ID3D12Object* pObject, LPCWSTR name)
{
	pObject->SetName(name);
}

inline void GRS_SetD3D12DebugNameIndexed(ID3D12Object* pObject, LPCWSTR name, UINT index)
{
	WCHAR _DebugName[MAX_PATH] = {};
	if (SUCCEEDED(StringCchPrintfW(_DebugName, _countof(_DebugName), L"%s[%u]", name, index)))
	{
		pObject->SetName(_DebugName);
	}
}
#else

inline void GRS_SetD3D12DebugName(ID3D12Object*, LPCWSTR)
{
}
inline void GRS_SetD3D12DebugNameIndexed(ID3D12Object*, LPCWSTR, UINT)
{
}

#endif

#define GRS_SET_D3D12_DEBUGNAME(x)						GRS_SetD3D12DebugName(x, L#x)
#define GRS_SET_D3D12_DEBUGNAME_INDEXED(x, n)			GRS_SetD3D12DebugNameIndexed(x[n], L#x, n)

#define GRS_SET_D3D12_DEBUGNAME_COMPTR(x)				GRS_SetD3D12DebugName(x.Get(), L#x)
#define GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR(x, n)	GRS_SetD3D12DebugNameIndexed(x[n].Get(), L#x, n)

#if defined(_DEBUG)
inline void GRS_SetDXGIDebugName(IDXGIObject* pObject, LPCWSTR name)
{
	size_t szLen = 0;
	StringCchLengthW(name, 50, &szLen);
	pObject->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(szLen - 1), name);
}

inline void GRS_SetDXGIDebugNameIndexed(IDXGIObject* pObject, LPCWSTR name, UINT index)
{
	size_t szLen = 0;
	WCHAR _DebugName[MAX_PATH] = {};
	if (SUCCEEDED(StringCchPrintfW(_DebugName, _countof(_DebugName), L"%s[%u]", name, index)))
	{
		StringCchLengthW(_DebugName, _countof(_DebugName), &szLen);
		pObject->SetPrivateData(WKPDID_D3DDebugObjectName, static_cast<UINT>(szLen), _DebugName);
	}
}
#else

inline void GRS_SetDXGIDebugName(ID3D12Object*, LPCWSTR)
{
}
inline void GRS_SetDXGIDebugNameIndexed(ID3D12Object*, LPCWSTR, UINT)
{
}

#endif

#define GRS_SET_DXGI_DEBUGNAME(x)						GRS_SetDXGIDebugName(x, L#x)
#define GRS_SET_DXGI_DEBUGNAME_INDEXED(x, n)			GRS_SetDXGIDebugNameIndexed(x[n], L#x, n)

#define GRS_SET_DXGI_DEBUGNAME_COMPTR(x)				GRS_SetDXGIDebugName(x.Get(), L#x)
#define GRS_SET_DXGI_DEBUGNAME_INDEXED_COMPTR(x, n)		GRS_SetDXGIDebugNameIndexed(x[n].Get(), L#x, n)
//------------------------------------------------------------------------------------------------------------

inline UINT Align(UINT size, UINT alignment)
{
	return (size + (alignment - 1)) & ~(alignment - 1);
}

class GpuUploadBuffer
{
public:
	ComPtr<ID3D12Resource> GetResource() { return m_resource; }

protected:
	ComPtr<ID3D12Resource> m_resource;

	GpuUploadBuffer() {}
	~GpuUploadBuffer()
	{
		if (m_resource.Get())
		{
			m_resource->Unmap(0, nullptr);
		}
	}

	void Allocate(ID3D12Device* device, UINT bufferSize, LPCWSTR resourceName = nullptr)
	{
		auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

		auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
		GRS_THROW_IF_FAILED(device->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&m_resource)));
		m_resource->SetName(resourceName);
	}

	uint8_t* MapCpuWriteOnly()
	{
		uint8_t* mappedData;
		// We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
		CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
		GRS_THROW_IF_FAILED(m_resource->Map(0, &readRange, reinterpret_cast<void**>(&mappedData)));
		return mappedData;
	}
};
// Shader record = {{Shader ID}, {RootArguments}}
class ShaderRecord
{
public:
	ShaderRecord(void* pShaderIdentifier, UINT nShaderIdentifierSize) :
		shaderIdentifier(pShaderIdentifier, nShaderIdentifierSize)
	{
	}

	ShaderRecord(void* pShaderIdentifier, UINT nShaderIdentifierSize, void* pLocalRootArguments, UINT localRootArgumentsSize) :
		shaderIdentifier(pShaderIdentifier, nShaderIdentifierSize),
		localRootArguments(pLocalRootArguments, localRootArgumentsSize)
	{
	}

	void CopyTo(void* dest) const
	{
		uint8_t* byteDest = static_cast<uint8_t*>(dest);
		memcpy(byteDest, shaderIdentifier.ptr, shaderIdentifier.size);
		if (localRootArguments.ptr)
		{
			memcpy(byteDest + shaderIdentifier.size, localRootArguments.ptr, localRootArguments.size);
		}
	}

	struct PointerWithSize {
		void* ptr;
		UINT size;

		PointerWithSize() : ptr(nullptr), size(0) {}
		PointerWithSize(void* _ptr, UINT _size) : ptr(_ptr), size(_size) {};
	};
	PointerWithSize shaderIdentifier;
	PointerWithSize localRootArguments;
};

// Shader table = {{ ShaderRecord 1}, {ShaderRecord 2}, ...}
class ShaderTable : public GpuUploadBuffer
{
	uint8_t* m_mappedShaderRecords;
	UINT m_shaderRecordSize;

	// Debug support
	std::wstring m_name;
	std::vector<ShaderRecord> m_shaderRecords;

	ShaderTable() {}
public:
	ShaderTable(ID3D12Device* device, UINT nNumShaderRecords, UINT nShaderRecordSize, LPCWSTR resourceName = nullptr)
		: m_name(resourceName)
	{
		m_shaderRecordSize = Align(nShaderRecordSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
		m_shaderRecords.reserve(nNumShaderRecords);
		UINT bufferSize = nNumShaderRecords * m_shaderRecordSize;
		Allocate(device, bufferSize, resourceName);
		m_mappedShaderRecords = MapCpuWriteOnly();
	}

	void push_back(const ShaderRecord& shaderRecord)
	{
		GRS_THROW_IF_FALSE(m_shaderRecords.size() < m_shaderRecords.capacity());
		m_shaderRecords.push_back(shaderRecord);
		shaderRecord.CopyTo(m_mappedShaderRecords);
		m_mappedShaderRecords += m_shaderRecordSize;
	}

	UINT GetShaderRecordSize() { return m_shaderRecordSize; }

	// Pretty-print the shader records.
	void DebugPrint(std::unordered_map<void*, std::wstring> shaderIdToStringMap)
	{
		std::wstringstream wstr;
		wstr << L"|--------------------------------------------------------------------\n";
		wstr << L"|Shader table - " << m_name.c_str() << L": "
			<< m_shaderRecordSize << L" | "
			<< m_shaderRecords.size() * m_shaderRecordSize << L" bytes\n";

		for (UINT i = 0; i < m_shaderRecords.size(); i++)
		{
			wstr << L"| [" << i << L"]: ";
			wstr << shaderIdToStringMap[m_shaderRecords[i].shaderIdentifier.ptr] << L", ";
			wstr << m_shaderRecords[i].shaderIdentifier.size << L" + " << m_shaderRecords[i].localRootArguments.size << L" bytes \n";
		}
		wstr << L"|--------------------------------------------------------------------\n";
		wstr << L"\n";
		OutputDebugStringW(wstr.str().c_str());
	}
};


XMVECTOR g_vEye		= {0.0f,0.0f,-20.0f,0.0f};
XMVECTOR g_vLookAt	= {0.0f,0.0f,0.0f};
XMVECTOR g_vUp		= {0.0f,1.0f,0.0f,0.0f};

LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
BOOL LoadMeshVertex(const CHAR* pszMeshFileName, UINT& nVertexCnt, ST_GRS_VERTEX*& ppVertex, GRS_TYPE_INDEX*& ppIndices);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR    lpCmdLine, int nCmdShow)
{
	int											iWidth = 1024;
	int											iHeight = 768;

	HWND										hWnd = nullptr;
	MSG											stMsg = {};
	TCHAR										pszAppPath[MAX_PATH] = {};

	const UINT									c_nFrameBackBufCount = 3u;
	UINT										nCurFrameIndex = 0;
	UINT										nDXGIFactoryFlags = 0U;
	DXGI_FORMAT									fmtBackBuffer = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT									fmtDepthStencil = DXGI_FORMAT_D24_UNORM_S8_UINT;
	const float									c_faClearColor[] = { 0.2f, 0.5f, 1.0f, 1.0f };
	CD3DX12_VIEWPORT							stViewPort(0.0f, 0.0f, static_cast<float>(iWidth), static_cast<float>(iHeight));
	CD3DX12_RECT								stScissorRect(0, 0, static_cast<LONG>(iWidth), static_cast<LONG>(iHeight));

	ComPtr<IDXGIFactory5>						pIDXGIFactory5;
	ComPtr<IDXGIFactory6>						pIDXGIFactory6;
	ComPtr<IDXGIAdapter1>						pIDXGIAdapter1;
	ComPtr<ID3D12Device4>						pID3D12Device4;

	ComPtr<ID3D12CommandQueue>					pICMDQueue;
	ComPtr<ID3D12CommandAllocator>				pICMDAlloc;
	ComPtr<ID3D12GraphicsCommandList>			pICMDList;
	ComPtr<ID3D12Fence1>						pIFence1;

	ComPtr<ID3D12DescriptorHeap>				pIRTVHeap;   //Render Target View
	ComPtr<ID3D12DescriptorHeap>				pIDSVHeap;   //Depth Stencil View

	ComPtr<IDXGISwapChain1>						pISwapChain1;
	ComPtr<IDXGISwapChain3>						pISwapChain3;

	ComPtr<ID3D12Resource>						pIRenderTargetBufs[c_nFrameBackBufCount];
	ComPtr<ID3D12Resource>						pIDepthStencilBuf;

	//=====================================================================================================================
	//DXR �ӿ�
	ComPtr<ID3D12RaytracingFallbackDevice>      pID3D12DXRFallbackDevice;
	ComPtr<ID3D12RaytracingFallbackCommandList> pIDXRFallbackCMDList;
	ComPtr<ID3D12RaytracingFallbackStateObject> pIDXRFallbackPSO;

	ComPtr<ID3D12Device5>						pID3D12DXRDevice;
	ComPtr<ID3D12GraphicsCommandList4>			pIDXRCmdList;
	ComPtr<ID3D12StateObjectPrototype>			pIDXRPSO;

	ComPtr<ID3D12Resource>						pIDXRUAVBufs;
	ComPtr<ID3D12DescriptorHeap>				pIDXRUAVHeap;		//UAV Output��VB�� IB�� Scene CB�� Module CB

	//��ǩ�� DXR����Ҫ������ǩ����һ����ȫ�ֵĸ�ǩ������һ�����Ǿֲ��ĸ�ǩ��
	ComPtr<ID3D12RootSignature>					pIRSGlobal;
	ComPtr<ID3D12RootSignature>					pIRSLocal;

	ComPtr<ID3DBlob>							pIDXRHLSLCode; //Ray Tracing Shader Code

	ComPtr<ID3D12Resource>						pIUAVBottomLevelAccelerationStructure;
	ComPtr<ID3D12Resource>						pIUAVTopLevelAccelerationStructure;
	ComPtr<ID3D12Resource>						pIUAVScratchResource;
	ComPtr<ID3D12Resource>						pIUploadBufInstanceDescs;
	
	ComPtr<ID3D12Resource>						pIRESMissShaderTable;
	ComPtr<ID3D12Resource>						pIRESHitGroupShaderTable;
	ComPtr<ID3D12Resource>						pIRESRayGenShaderTable;

	WRAPPED_GPU_POINTER							pFallbackTopLevelAccelerationStructurePointer = {};

	const wchar_t*								c_pszHitGroupName			= L"MyHitGroup";
	const wchar_t*								c_pszRaygenShaderName		= L"MyRaygenShader";
	const wchar_t*								c_pszClosestHitShaderName	= L"MyClosestHitShader";
	const wchar_t*								c_pszMissShaderName			= L"MyMissShader";
	//=====================================================================================================================

	ComPtr<ID3D12Resource>						pIVBBufs;			//ST_GRS_VERTEX Buffer
	ComPtr<ID3D12Resource>						pIIBBufs;			//GRS_TYPE_INDEX Buffer
	ComPtr<ID3D12Resource>						pICBFrameConstant;

	UINT										nRTVDescriptorSize = 0;
	UINT										nSRVDescriptorSize = 0;

	const UINT									c_nDSHIndxUAVOutput = 0;
	const UINT									c_nDSHIndxIBView = 1;
	const UINT									c_nDSHIndxVBView = 2;
	const UINT									c_nDSHIndxCBScene = 3;
	const UINT									c_nDSHIndxCBModule = 4;
	const UINT									c_nDSHIndxASBottom1 = 5;
	const UINT									c_nDSHIndxASBottom2 = 6;
	UINT										nMaxDSCnt = 7;

//	ST_SCENE_CONSANTBUFFER						stCBScene[c_nFrameBackBufCount];
	ST_MODULE_CONSANTBUFFER						stCBModule = { XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f) };
	ST_SCENE_CONSANTBUFFER*						pstCBScene = nullptr;

	D3D_FEATURE_LEVEL							emMinFeature = D3D_FEATURE_LEVEL_12_1;
	HANDLE										hEventFence1 = nullptr;
	UINT64										n64FenceValue = 0i64;
	CAtlArray<HANDLE>							arWaitHandles;

	BOOL										bISDXRSupport = FALSE;

	ST_GRS_VERTEX*								pstVertices = nullptr;
	GRS_TYPE_INDEX*								pnIndices = nullptr;
	UINT										nVertexCnt = 0;
	UINT										nIndexCnt = 0;

	GRS_USEPRINTF();
	try
	{
		GRS_THROW_IF_FAILED(::CoInitialize(nullptr));  //for WIC & COM
		GRS_INIT_OUTPUT();

		// �õ���ǰ�Ĺ���Ŀ¼����������ʹ�����·�������ʸ�����Դ�ļ�
		{
			UINT nBytes = GetCurrentDirectory(MAX_PATH, pszAppPath);
			if (MAX_PATH == nBytes)
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}
		}

		// ע�Ტ��������
		{
			WNDCLASSEX wcex = {};
			wcex.cbSize = sizeof(WNDCLASSEX);
			wcex.style = CS_GLOBALCLASS;
			wcex.lpfnWndProc = WndProc;
			wcex.cbClsExtra = 0;
			wcex.cbWndExtra = 0;
			wcex.hInstance = hInstance;
			wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
			wcex.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);		//��ֹ���ĵı����ػ�
			wcex.lpszClassName = GRS_WND_CLASS_NAME;

			::RegisterClassEx(&wcex);

			DWORD dwWndStyle = WS_OVERLAPPED | WS_SYSMENU;
			RECT rtWnd = { 0, 0, iWidth, iHeight };
			AdjustWindowRect(&rtWnd, dwWndStyle, FALSE);

			hWnd = CreateWindowW(
				GRS_WND_CLASS_NAME
				, GRS_WND_TITLE
				, dwWndStyle
				, CW_USEDEFAULT
				, 0
				, rtWnd.right - rtWnd.left
				, rtWnd.bottom - rtWnd.top
				, nullptr
				, nullptr
				, hInstance
				, nullptr);

			if (!hWnd)
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}
		}

		//����ʾ��ϵͳ�ĵ���֧��
		{
#if defined(_DEBUG)
			ComPtr<ID3D12Debug> debugController;
			if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
			{
				debugController->EnableDebugLayer();
				// �򿪸��ӵĵ���֧��
				nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
			}
#endif
		}

		//����DXGI Factory����
		{
			GRS_THROW_IF_FAILED(CreateDXGIFactory2(nDXGIFactoryFlags, IID_PPV_ARGS(&pIDXGIFactory5)));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIFactory5);
			// �ر�ALT+ENTER���л�ȫ���Ĺ��ܣ���Ϊ����û��ʵ��OnSize���������ȹر�
			GRS_THROW_IF_FAILED(pIDXGIFactory5->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

			GRS_THROW_IF_FAILED(pIDXGIFactory5.As(&pIDXGIFactory6));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIFactory6);
		}

		//ö����ʾ��������ѡ���Կ�����ΪҪDXR ����ѡ��������ǿ�ģ�
		{//ע�� ���������½ӿ�IDXGIFactory6���·���EnumAdapterByGpuPreference
			GRS_THROW_IF_FAILED(pIDXGIFactory6->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&pIDXGIAdapter1)));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pIDXGIAdapter1);
		}

		//=====================================================================================================================
		// ����������Ƿ�֧�ּ��ݼ���Ĺ�׷��Ⱦ����֧�֣��ͳ����޷��ˣ�ֻ���˳�
		{
			ComPtr<ID3D12Device> pID3D12DeviceTemp;
			UUID UUIDExperimentalFeatures[] = { D3D12ExperimentalShaderModels };

			// ����չ����֧��
			HRESULT hr1 = D3D12EnableExperimentalFeatures(1, UUIDExperimentalFeatures, nullptr, nullptr);
			// ����һ���豸���Կ��в���
			HRESULT hr2 = D3D12CreateDevice(pIDXGIAdapter1.Get(), emMinFeature, IID_PPV_ARGS(&pID3D12DeviceTemp));

			// �ۺ������������õĽ���Ϳ���ȷ�������ܲ��ܴ�DXR���������ǿ��Դ�DXR Fallback��֧�֣�Ҳ������ͨ�ü�����������DXR
			if (!(SUCCEEDED(hr1) && SUCCEEDED(hr2)))
			{
				::MessageBox(hWnd
					, _T("�ǳ���Ǹ��֪ͨ����\r\n��ϵͳ����NB���Կ�Ҳ����֧�ּ��ݼ���Ĺ�׷��Ⱦ������û���������У�\r\n�����˳���")
					, GRS_WND_TITLE
					, MB_OK | MB_ICONINFORMATION);

				return -1;
			}
		}

		// ��һ�����DXR֧�ֵ�ʲô����
		{
			D3D12_FEATURE_DATA_D3D12_OPTIONS5 stFeatureSupportData = {};

			// ����D3D12�豸
			GRS_THROW_IF_FAILED(D3D12CreateDevice(pIDXGIAdapter1.Get(), emMinFeature, IID_PPV_ARGS(&pID3D12Device4)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pID3D12Device4);
			// ���DXR֧�����(�������������ʧ�ܿ��Ժ��ԣ����ǽ�����������Ҫ����Ӳ����Win10 ��1809 17763�����ϰ汾)
			HRESULT hr = pID3D12Device4->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &stFeatureSupportData, sizeof(stFeatureSupportData));

			//��¼�Ƿ���ֱ��֧��DXR���������ΪFALSE����ô����Fallback��ʽ֧�֣��������Ϊ��D3D�е����ģ������֧��֮��
			//Ŀǰֻ��N��20ϵ���ϵ��Կ���ȫ֧��DXR����һ��ĵ�GTX9xx���϶��Ǽ��ݷ�ʽ
			bISDXRSupport = SUCCEEDED(hr) && (stFeatureSupportData.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED);

			DXGI_ADAPTER_DESC1 stAdapterDesc = {};
			pIDXGIAdapter1->GetDesc1(&stAdapterDesc);
			if (bISDXRSupport)
			{
				GRS_PRINTF(_T("��ϲ�������Կ���%s��ֱ��֧��DXR��\n"), stAdapterDesc.Description);
			}
			else
			{
				GRS_PRINTF(_T("�ǳ��ź��������Կ���%s����ֱ��֧��DXR����ʹ��Fallback����ģʽ���У�\n"), stAdapterDesc.Description);
			}

		}
		//=====================================================================================================================

		//����������С�����������������б�
		{
			D3D12_COMMAND_QUEUE_DESC stCMDQueueDesc = {};
			stCMDQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			stCMDQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandQueue(&stCMDQueueDesc, IID_PPV_ARGS(&pICMDQueue)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDQueue);

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&pICMDAlloc)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDAlloc);

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pICMDAlloc.Get(), nullptr, IID_PPV_ARGS(&pICMDList)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pICMDList);
		}

		//=====================================================================================================================
		//����DXR�豸���Լ�DXR�����б�
		{
			if (!bISDXRSupport)
			{
				GRS_THROW_IF_FAILED(D3D12CreateRaytracingFallbackDevice(
					pID3D12Device4.Get()
					, CreateRaytracingFallbackDeviceFlags::ForceComputeFallback
					, 0
					, IID_PPV_ARGS(&pID3D12DXRFallbackDevice)));
				pID3D12DXRFallbackDevice->QueryRaytracingCommandList(pICMDList.Get()
					, IID_PPV_ARGS(&pIDXRFallbackCMDList));
			}
			else
			{// DirectX Raytracing
				GRS_THROW_IF_FAILED(pID3D12Device4->QueryInterface(IID_PPV_ARGS(&pID3D12DXRDevice)));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(pID3D12DXRDevice);
				GRS_THROW_IF_FAILED(pICMDList->QueryInterface(IID_PPV_ARGS(&pIDXRCmdList)));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDXRCmdList);
			}

		}
		//=====================================================================================================================

		//����Fence
		{
			// ����һ��ͬ�����󡪡�Χ�������ڵȴ���Ⱦ��ɣ���Ϊ����Draw Call���첽����
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pIFence1)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIFence1);

			// ����һ��Eventͬ���������ڵȴ�Χ���¼�֪ͨ
			hEventFence1 = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if (nullptr == hEventFence1)
			{
				GRS_THROW_IF_FAILED(HRESULT_FROM_WIN32(GetLastError()));
			}

			arWaitHandles.Add(hEventFence1);
		}

		//����RTV �� DSV
		{
			D3D12_DESCRIPTOR_HEAP_DESC stRTVHeapDesc = {};
			stRTVHeapDesc.NumDescriptors = c_nFrameBackBufCount;
			stRTVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&stRTVHeapDesc, IID_PPV_ARGS(&pIRTVHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRTVHeap);

			nRTVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

			D3D12_DESCRIPTOR_HEAP_DESC stDSVHeapSesc = {};
			stDSVHeapSesc.NumDescriptors = 1;						//ͨ�� DSV -> Depth Stencil Bufferֻ��һ��
			stDSVHeapSesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&stDSVHeapSesc, IID_PPV_ARGS(&pIDSVHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDSVHeap);
		}

		//����������
		{
			DXGI_SWAP_CHAIN_DESC1 stSwapChainDesc = {};
			stSwapChainDesc.BufferCount = c_nFrameBackBufCount;
			stSwapChainDesc.Width = iWidth;
			stSwapChainDesc.Height = iHeight;
			stSwapChainDesc.Format = fmtBackBuffer;
			stSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
			stSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
			stSwapChainDesc.SampleDesc.Count = 1;

			GRS_THROW_IF_FAILED(pIDXGIFactory6->CreateSwapChainForHwnd(
				pICMDQueue.Get(),
				hWnd,
				&stSwapChainDesc,
				nullptr,
				nullptr,
				&pISwapChain1
			));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pISwapChain1);

			//ע��˴�ʹ���˸߰汾��SwapChain�ӿڵĺ���
			GRS_THROW_IF_FAILED(pISwapChain1.As(&pISwapChain3));
			GRS_SET_DXGI_DEBUGNAME_COMPTR(pISwapChain3);

			nCurFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

			//---------------------------------------------------------------------------------------------
			CD3DX12_CPU_DESCRIPTOR_HANDLE stRTVHandle(pIRTVHeap->GetCPUDescriptorHandleForHeapStart());
			for (UINT i = 0; i < c_nFrameBackBufCount; i++)
			{//���ѭ����©����������ʵ�����Ǹ�����ı���
				GRS_THROW_IF_FAILED(pISwapChain3->GetBuffer(i, IID_PPV_ARGS(&pIRenderTargetBufs[i])));
				GRS_SET_D3D12_DEBUGNAME_INDEXED_COMPTR(pIRenderTargetBufs, i);
				pID3D12Device4->CreateRenderTargetView(pIRenderTargetBufs[i].Get(), nullptr, stRTVHandle);
				stRTVHandle.Offset(1, nRTVDescriptorSize);
			}
		}

		//����������建��
		{
			CD3DX12_HEAP_PROPERTIES objDepthHeapProperties(D3D12_HEAP_TYPE_DEFAULT);

			D3D12_RESOURCE_DESC stDepthStencilDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				fmtDepthStencil,
				iWidth,
				iHeight,
				1,
				1
			);
			stDepthStencilDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

			D3D12_CLEAR_VALUE stDepthOptimizedClearValue = {};
			stDepthOptimizedClearValue.Format = fmtDepthStencil;
			stDepthOptimizedClearValue.DepthStencil.Depth = 1.0f;
			stDepthOptimizedClearValue.DepthStencil.Stencil = 0;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(&objDepthHeapProperties,
				D3D12_HEAP_FLAG_NONE,
				&stDepthStencilDesc,
				D3D12_RESOURCE_STATE_DEPTH_WRITE,
				&stDepthOptimizedClearValue,
				IID_PPV_ARGS(&pIDepthStencilBuf)
			));

			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDepthStencilBuf);

			D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
			dsvDesc.Format = fmtDepthStencil;
			dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

			pID3D12Device4->CreateDepthStencilView(pIDepthStencilBuf.Get(), &dsvDesc, pIDSVHeap->GetCPUDescriptorHandleForHeapStart());
		}

		//������Ⱦ��������Ҫ�ĸ�����������
		{
			D3D12_DESCRIPTOR_HEAP_DESC stDXRDescriptorHeapDesc = {};
			// ���7��������:
			// 2 - �������������� SRVs
			// 1 - ��׷��Ⱦ������� SRV
			// 2 - ���ٽṹ�Ļ��� UAVs
			// 2 - ���ٽṹ�����ݻ��� UAVs ��Ҫ����fallback��
			stDXRDescriptorHeapDesc.NumDescriptors = nMaxDSCnt;
			stDXRDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
			stDXRDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&stDXRDescriptorHeapDesc, IID_PPV_ARGS(&pIDXRUAVHeap)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDXRUAVHeap);

			nSRVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		}

		//=====================================================================================================================
		//����RayTracing Render Target UAV
		{
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT)
				, D3D12_HEAP_FLAG_NONE
				, &CD3DX12_RESOURCE_DESC::Tex2D(fmtBackBuffer, iWidth, iHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
				, D3D12_RESOURCE_STATE_UNORDERED_ACCESS
				, nullptr
				, IID_PPV_ARGS(&pIDXRUAVBufs)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIDXRUAVBufs);

			//UAV Heap�ĵ�һ�������� �ͷ�UAV Ҳ���� DXR��Output
			CD3DX12_CPU_DESCRIPTOR_HANDLE stUAVDescriptorHandle(
				pIDXRUAVHeap->GetCPUDescriptorHandleForHeapStart()
				, c_nDSHIndxUAVOutput
				, nSRVDescriptorSize);
			D3D12_UNORDERED_ACCESS_VIEW_DESC stUAVDesc = {};
			stUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
			pID3D12Device4->CreateUnorderedAccessView(pIDXRUAVBufs.Get(), nullptr, &stUAVDesc, stUAVDescriptorHandle);
		}

		//������ǩ�� ע��DXR����������ǩ����һ����ȫ�֣�Global����ǩ����һ���Ǳ��أ�Local����ǩ��
		{
			CD3DX12_DESCRIPTOR_RANGE stRanges[2] = {}; // Perfomance TIP: Order from most frequent to least frequent.
			stRanges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);  // 1 output texture
			stRanges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1);  // 2 static index and vertex buffers.

			CD3DX12_ROOT_PARAMETER stGlobalRootParams[4];
			stGlobalRootParams[0].InitAsDescriptorTable(1, &stRanges[0]);
			stGlobalRootParams[1].InitAsShaderResourceView(0);
			stGlobalRootParams[2].InitAsConstantBufferView(0);
			stGlobalRootParams[3].InitAsDescriptorTable(1, &stRanges[1]);
			CD3DX12_ROOT_SIGNATURE_DESC stGlobalRootSignatureDesc(ARRAYSIZE(stGlobalRootParams), stGlobalRootParams);

			CD3DX12_ROOT_PARAMETER stLocalRootParams[1] = {};
			stLocalRootParams[0].InitAsConstants(GRS_UPPER_SIZEOFUINT32(ST_MODULE_CONSANTBUFFER), 1);
			CD3DX12_ROOT_SIGNATURE_DESC stLocalRootSignatureDesc(ARRAYSIZE(stLocalRootParams), stLocalRootParams);
			stLocalRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

			ComPtr<ID3DBlob> pIRSBlob;
			ComPtr<ID3DBlob> pIRSErrMsg;
			HRESULT hrRet = S_OK;

			if (!bISDXRSupport)
			{//Fallback ��ʽ����������ǩ��
				hrRet = pID3D12DXRFallbackDevice->D3D12SerializeRootSignature(&stGlobalRootSignatureDesc
					, D3D_ROOT_SIGNATURE_VERSION_1
					, &pIRSBlob
					, &pIRSErrMsg);
				if (FAILED(hrRet))
				{
					if (pIRSErrMsg)
					{
						GRS_PRINTF(_T("�����ǩ������%s\n"), static_cast<wchar_t*>(pIRSErrMsg->GetBufferPointer()));
					}
					GRS_THROW_IF_FAILED(hrRet);
				}

				GRS_THROW_IF_FAILED(pID3D12DXRFallbackDevice->CreateRootSignature(1
					, pIRSBlob->GetBufferPointer()
					, pIRSBlob->GetBufferSize()
					, IID_PPV_ARGS(&pIRSGlobal)));
				pIRSBlob.Reset();
				pIRSErrMsg.Reset();

				hrRet = pID3D12DXRFallbackDevice->D3D12SerializeRootSignature(&stLocalRootSignatureDesc
					, D3D_ROOT_SIGNATURE_VERSION_1
					, &pIRSBlob
					, &pIRSErrMsg);
				if (FAILED(hrRet))
				{
					if (pIRSErrMsg)
					{
						GRS_PRINTF(_T("�����ǩ������%s\n"), static_cast<wchar_t*>(pIRSErrMsg->GetBufferPointer()));
					}
					GRS_THROW_IF_FAILED(hrRet);
				}

				GRS_THROW_IF_FAILED(pID3D12DXRFallbackDevice->CreateRootSignature(1
					, pIRSBlob->GetBufferPointer()
					, pIRSBlob->GetBufferSize()
					, IID_PPV_ARGS(&pIRSLocal)));

			}
			else // DirectX Raytracing
			{
				hrRet = D3D12SerializeRootSignature(&stGlobalRootSignatureDesc
					, D3D_ROOT_SIGNATURE_VERSION_1
					, &pIRSBlob
					, &pIRSErrMsg);
				if (FAILED(hrRet))
				{
					if (pIRSErrMsg)
					{
						GRS_PRINTF(_T("�����ǩ������%s\n"), static_cast<wchar_t*>(pIRSErrMsg->GetBufferPointer()));
					}
					GRS_THROW_IF_FAILED(hrRet);
				}

				GRS_THROW_IF_FAILED(pID3D12DXRDevice->CreateRootSignature(1
					, pIRSBlob->GetBufferPointer()
					, pIRSBlob->GetBufferSize()
					, IID_PPV_ARGS(&pIRSGlobal)));

				pIRSBlob.Reset();
				pIRSErrMsg.Reset();

				hrRet = D3D12SerializeRootSignature(&stLocalRootSignatureDesc
					, D3D_ROOT_SIGNATURE_VERSION_1
					, &pIRSBlob
					, &pIRSErrMsg);
				if (FAILED(hrRet))
				{
					if (pIRSErrMsg)
					{
						GRS_PRINTF(_T("�����ǩ������%s\n"), static_cast<wchar_t*>(pIRSErrMsg->GetBufferPointer()));
					}
					GRS_THROW_IF_FAILED(hrRet);
				}

				GRS_THROW_IF_FAILED(pID3D12DXRDevice->CreateRootSignature(1
					, pIRSBlob->GetBufferPointer()
					, pIRSBlob->GetBufferSize()
					, IID_PPV_ARGS(&pIRSLocal)));
			}

			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRSGlobal);
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIRSLocal);
		}

		//����RayTracing����״̬����
		{ {
			// Create 7 subobjects that combine into a RTPSO:
			// Subobjects need to be associated with DXIL exports (i.e. shaders) either by way of default or explicit associations.
			// Default association applies to every exported shader entrypoint that doesn't have any of the same type of subobject associated with it.
			// This simple sample utilizes default shader association except for local root signature subobject
			// which has an explicit association specified purely for demonstration purposes.
			// 1 - DXIL library
			// 1 - Triangle hit group
			// 1 - Shader config
			// 2 - Local root signature and association
			// 1 - Global root signature
			// 1 - Pipeline config
			CD3D12_STATE_OBJECT_DESC objRaytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

			// DXIL library
			// This contains the shaders and their entrypoints for the state object.
			// Since shaders are not considered a subobject, they need to be passed in via DXIL library subobjects.
			auto objDXILLib = objRaytracingPipeline.CreateSubobject<CD3D12_DXIL_LIBRARY_SUBOBJECT>();
			D3D12_SHADER_BYTECODE stLibDXIL = CD3DX12_SHADER_BYTECODE((void*)g_pRaytracing, ARRAYSIZE(g_pRaytracing));
			objDXILLib->SetDXILLibrary(&stLibDXIL);
			// Define which shader exports to surface from the library.
			// If no shader exports are defined for a DXIL library subobject, all shaders will be surfaced.
			// In this sample, this could be ommited for convenience since the sample uses all shaders in the library. 
			{
				objDXILLib->DefineExport(c_pszRaygenShaderName);
				objDXILLib->DefineExport(c_pszClosestHitShaderName);
				objDXILLib->DefineExport(c_pszMissShaderName);
			}

			// Triangle hit group
			// A hit group specifies closest hit, any hit and intersection shaders to be executed when a ray intersects the geometry's triangle/AABB.
			// In this sample, we only use triangle geometry with a closest hit shader, so others are not set.
			auto objHitGroup = objRaytracingPipeline.CreateSubobject<CD3D12_HIT_GROUP_SUBOBJECT>();
			objHitGroup->SetClosestHitShaderImport(c_pszClosestHitShaderName);
			objHitGroup->SetHitGroupExport(c_pszHitGroupName);
			objHitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

			// Shader config
			// Defines the maximum sizes in bytes for the ray payload and attribute structure.
			auto objShaderConfig = objRaytracingPipeline.CreateSubobject<CD3D12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
			UINT nPayloadSize = sizeof(XMFLOAT4);    // float4 pixelColor
			UINT nAttributeSize = sizeof(XMFLOAT2);  // float2 barycentrics
			objShaderConfig->Config(nPayloadSize, nAttributeSize);

			// Local root signature and shader association
			// This is a root signature that enables a shader to have unique arguments that come from shader tables.
			auto objLocalRootSignature = objRaytracingPipeline.CreateSubobject<CD3D12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
			objLocalRootSignature->SetRootSignature(pIRSLocal.Get());
			// Define explicit shader association for the local root signature. 
			{
				auto objRootSignatureAssociation = objRaytracingPipeline.CreateSubobject<CD3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
				objRootSignatureAssociation->SetSubobjectToAssociate(*objLocalRootSignature);
				objRootSignatureAssociation->AddExport(c_pszHitGroupName);
			}
			// Global root signature
			// This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
			auto objGlobalRootSignature = objRaytracingPipeline.CreateSubobject<CD3D12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
			objGlobalRootSignature->SetRootSignature(pIRSGlobal.Get());

			// Pipeline config
			// Defines the maximum TraceRay() recursion depth.
			auto objPipelineConfig = objRaytracingPipeline.CreateSubobject<CD3D12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
			// PERFOMANCE TIP: Set max recursion depth as low as needed 
			// as drivers may apply optimization strategies for low recursion depths.
			UINT nMaxRecursionDepth = 1; // ~ primary rays only. 
			objPipelineConfig->Config(nMaxRecursionDepth);

			// Create the state object.
			if (!bISDXRSupport)
			{
				GRS_THROW_IF_FAILED(pID3D12DXRFallbackDevice->CreateStateObject(objRaytracingPipeline, IID_PPV_ARGS(&pIDXRFallbackPSO)));
			}
			else // DirectX Raytracing
			{
				GRS_THROW_IF_FAILED(pID3D12DXRDevice->CreateStateObject(objRaytracingPipeline, IID_PPV_ARGS(&pIDXRPSO)));
			}
		} }
		//=====================================================================================================================

		//����ģ������
		{
			USES_CONVERSION;

			CHAR pszMeshFileName[MAX_PATH] = {};
			StringCchPrintfA(pszMeshFileName, MAX_PATH, "%s\\Mesh\\sphere.txt", T2A(pszAppPath));

			LoadMeshVertex(pszMeshFileName, nVertexCnt, pstVertices, pnIndices);
			nIndexCnt = nVertexCnt;
			//*******************************************************************************************
			//nIndexCnt = nVertexCnt = 3;

			//pstVertices = (ST_GRS_VERTEX*)GRS_CALLOC(nVertexCnt * sizeof(ST_GRS_VERTEX));
			//pnIndices = (GRS_TYPE_INDEX*)GRS_CALLOC(nIndexCnt * sizeof(GRS_TYPE_INDEX));

			//pnIndices[0] = 0;
			//pnIndices[1] = 1;
			//pnIndices[2] = 2;

			//float depthValue = 1.0;
			//float offset = 0.7f;

			//pstVertices[0].m_vPos = XMFLOAT4(0, -offset, depthValue, 1.0f);
			//pstVertices[1].m_vPos = XMFLOAT4(-offset, offset, depthValue, 1.0f);
			//pstVertices[2].m_vPos = XMFLOAT4(offset, offset, depthValue, 1.0f);

			//pstVertices[0].m_vNor = XMFLOAT3(-1.0f, 0.0f, -1.0f);
			//pstVertices[1].m_vNor = XMFLOAT3(0.0f, -1.0f, -1.0f);
			//pstVertices[2].m_vNor = XMFLOAT3(0.0f, 0.0f, -1.0f);

			//pstVertices[0].m_vTex = XMFLOAT2(1.0f, 1.0f);
			//pstVertices[1].m_vTex = XMFLOAT2(1.0f, 1.0f);
			//pstVertices[2].m_vTex = XMFLOAT2(1.0f, 1.0f);
			//*******************************************************************************************

			//���� ST_GRS_VERTEX Buffer ��ʹ��Upload��ʽ��
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
				, D3D12_HEAP_FLAG_NONE
				, &CD3DX12_RESOURCE_DESC::Buffer(nVertexCnt * sizeof(ST_GRS_VERTEX))
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pIVBBufs)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIVBBufs);

			//ʹ��map-memcpy-unmap�󷨽����ݴ������㻺�����
			UINT8* pVertexDataBegin = nullptr;
			CD3DX12_RANGE stReadRange(0, 0);		// We do not intend to read from this resource on the CPU.

			GRS_THROW_IF_FAILED(pIVBBufs->Map(0, &stReadRange, reinterpret_cast<void**>(&pVertexDataBegin)));
			memcpy(pVertexDataBegin, pstVertices, nVertexCnt * sizeof(ST_GRS_VERTEX));
			pIVBBufs->Unmap(0, nullptr);

			//���� GRS_TYPE_INDEX Buffer ��ʹ��Upload��ʽ��
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD)
				, D3D12_HEAP_FLAG_NONE
				, &CD3DX12_RESOURCE_DESC::Buffer(nIndexCnt * sizeof(GRS_TYPE_INDEX))
				, D3D12_RESOURCE_STATE_GENERIC_READ
				, nullptr
				, IID_PPV_ARGS(&pIIBBufs)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIIBBufs);

			UINT8* pIndexDataBegin = nullptr;
			GRS_THROW_IF_FAILED(pIIBBufs->Map(0, &stReadRange, reinterpret_cast<void**>(&pIndexDataBegin)));
			memcpy(pIndexDataBegin, pnIndices, nIndexCnt * sizeof(GRS_TYPE_INDEX));
			pIIBBufs->Unmap(0, nullptr);

			GRS_SAFE_FREE(pstVertices);
			GRS_SAFE_FREE(pnIndices);
			
			// GRS_TYPE_INDEX SRV
			D3D12_SHADER_RESOURCE_VIEW_DESC stSRVDesc = {};
			stSRVDesc.ViewDimension					= D3D12_SRV_DIMENSION_BUFFER;
			stSRVDesc.Shader4ComponentMapping		= D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			stSRVDesc.Format						= DXGI_FORMAT_R32_TYPELESS;
			stSRVDesc.Buffer.Flags					= D3D12_BUFFER_SRV_FLAG_RAW;
			stSRVDesc.Buffer.NumElements			= (nIndexCnt * sizeof(GRS_TYPE_INDEX))/4;// GRS_UPPER_DIV((nIndexCnt * sizeof(GRS_TYPE_INDEX)), 4);
			stSRVDesc.Buffer.StructureByteStride	= 0;

			pID3D12Device4->CreateShaderResourceView( pIIBBufs.Get()
				, &stSRVDesc
				, CD3DX12_CPU_DESCRIPTOR_HANDLE(pIDXRUAVHeap->GetCPUDescriptorHandleForHeapStart()
					,c_nDSHIndxIBView
					,nSRVDescriptorSize) );

			// Vertex SRV
			stSRVDesc.Format						= DXGI_FORMAT_UNKNOWN;
			stSRVDesc.Buffer.Flags					= D3D12_BUFFER_SRV_FLAG_NONE;
			stSRVDesc.Buffer.NumElements			= nVertexCnt;
			stSRVDesc.Buffer.StructureByteStride	= sizeof(ST_GRS_VERTEX);
			
			pID3D12Device4->CreateShaderResourceView(pIVBBufs.Get()
				, &stSRVDesc
				, CD3DX12_CPU_DESCRIPTOR_HANDLE(pIDXRUAVHeap->GetCPUDescriptorHandleForHeapStart()
					, c_nDSHIndxVBView
					, nSRVDescriptorSize));

		}

		//=====================================================================================================================
		//����ģ�͵ļ��ٽṹ��
		{
			//pICMDList->Reset(pICMDAlloc.Get(), nullptr);

			D3D12_RAYTRACING_GEOMETRY_DESC stModuleGeometryDesc = {};
			stModuleGeometryDesc.Type									= D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
			stModuleGeometryDesc.Triangles.IndexBuffer					= pIIBBufs->GetGPUVirtualAddress();
			stModuleGeometryDesc.Triangles.IndexCount					= static_cast<UINT>(pIIBBufs->GetDesc().Width) / sizeof(GRS_TYPE_INDEX);
			stModuleGeometryDesc.Triangles.IndexFormat					= DXGI_FORMAT_R16_UINT;
			stModuleGeometryDesc.Triangles.Transform3x4					= 0;
			stModuleGeometryDesc.Triangles.VertexFormat					= DXGI_FORMAT_R32G32B32_FLOAT;
			stModuleGeometryDesc.Triangles.VertexCount					= static_cast<UINT>(pIVBBufs->GetDesc().Width) / sizeof(ST_GRS_VERTEX);
			stModuleGeometryDesc.Triangles.VertexBuffer.StartAddress	= pIVBBufs->GetGPUVirtualAddress();
			stModuleGeometryDesc.Triangles.VertexBuffer.StrideInBytes	= sizeof(ST_GRS_VERTEX);

			// Mark the geometry as opaque. 
			// PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
			// Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
			stModuleGeometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

			// Get required sizes for an acceleration structure.
			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS emBuildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;

			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC stBottomLevelBuildDesc = {};
			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& stBottomLevelInputs = stBottomLevelBuildDesc.Inputs;
			stBottomLevelInputs.DescsLayout		= D3D12_ELEMENTS_LAYOUT_ARRAY;
			stBottomLevelInputs.Flags			= emBuildFlags;
			stBottomLevelInputs.NumDescs		= 1;
			stBottomLevelInputs.Type			= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
			stBottomLevelInputs.pGeometryDescs	= &stModuleGeometryDesc;

			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC stTopLevelBuildDesc = {};
			D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS& stTopLevelInputs = stTopLevelBuildDesc.Inputs;
			stTopLevelInputs.DescsLayout		= D3D12_ELEMENTS_LAYOUT_ARRAY;
			stTopLevelInputs.Flags				= emBuildFlags;
			stTopLevelInputs.NumDescs			= 1;
			stTopLevelInputs.pGeometryDescs		= nullptr;
			stTopLevelInputs.Type				= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO stTopLevelPrebuildInfo = {};
			if ( !bISDXRSupport )
			{
				pID3D12DXRFallbackDevice->GetRaytracingAccelerationStructurePrebuildInfo(&stTopLevelInputs, &stTopLevelPrebuildInfo);
			}
			else // DirectX Raytracing
			{
				pID3D12DXRDevice->GetRaytracingAccelerationStructurePrebuildInfo(&stTopLevelInputs, &stTopLevelPrebuildInfo);
			}

			GRS_THROW_IF_FALSE(stTopLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

			D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO stBottomLevelPrebuildInfo = {};
			if (!bISDXRSupport)
			{
				pID3D12DXRFallbackDevice->GetRaytracingAccelerationStructurePrebuildInfo(&stBottomLevelInputs, &stBottomLevelPrebuildInfo);
			}
			else // DirectX Raytracing
			{
				pID3D12DXRDevice->GetRaytracingAccelerationStructurePrebuildInfo(&stBottomLevelInputs, &stBottomLevelPrebuildInfo);
			}
			GRS_THROW_IF_FALSE(stBottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(
					max(stTopLevelPrebuildInfo.ScratchDataSizeInBytes
						, stBottomLevelPrebuildInfo.ScratchDataSizeInBytes)
					, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				nullptr,
				IID_PPV_ARGS(&pIUAVScratchResource)));
			GRS_SET_D3D12_DEBUGNAME_COMPTR(pIUAVScratchResource);


			// Allocate resources for acceleration structures.
			// Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
			// Default heap is OK since the application doesn�t need CPU read/write access to them. 
			// The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
			// and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
			//  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
			//  - from the app point of mxView, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.
			{
				D3D12_RESOURCE_STATES emInitialResourceState;
				if (!bISDXRSupport)
				{
					emInitialResourceState = pID3D12DXRFallbackDevice->GetAccelerationStructureResourceState();
				}
				else // DirectX Raytracing
				{
					emInitialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
				}

				GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC::Buffer(stBottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
					emInitialResourceState,
					nullptr,
					IID_PPV_ARGS(&pIUAVBottomLevelAccelerationStructure)));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(pIUAVBottomLevelAccelerationStructure);

				GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC::Buffer(stTopLevelPrebuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
					emInitialResourceState,
					nullptr,
					IID_PPV_ARGS(&pIUAVTopLevelAccelerationStructure)));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(pIUAVTopLevelAccelerationStructure);

			}

			// Note on Emulated GPU pointers (AKA Wrapped pointers) requirement in Fallback Layer:
			// The primary point of divergence between the DXR API and the compute-based Fallback layer is the handling of GPU pointers. 
			// DXR fundamentally requires that GPUs be able to dynamically read from arbitrary addresses in GPU memory. 
			// The existing Direct Compute API today is more rigid than DXR and requires apps to explicitly inform the GPU what blocks of memory it will access with SRVs/UAVs.
			// In order to handle the requirements of DXR, the Fallback Layer uses the concept of Emulated GPU pointers, 
			// which requires apps to create views around all memory they will access for raytracing, 
			// but retains the DXR-like flexibility of only needing to bind the top level acceleration structure at DispatchRays.
			//
			// The Fallback Layer interface uses WRAPPED_GPU_POINTER to encapsulate the underlying pointer
			// which will either be an emulated GPU pointer for the compute - based path or a GPU_VIRTUAL_ADDRESS for the DXR path.

			// Create an instance desc for the bottom-level acceleration structure.


			if (!bISDXRSupport)
			{
				D3D12_RAYTRACING_FALLBACK_INSTANCE_DESC stInstanceDesc = {};
				stInstanceDesc.Transform[0][0] = stInstanceDesc.Transform[1][1] = stInstanceDesc.Transform[2][2] = 1;
				stInstanceDesc.InstanceMask = 1;
				UINT nNumBufferElements = static_cast<UINT>(stBottomLevelPrebuildInfo.ResultDataMaxSizeInBytes) / sizeof(UINT32);

				D3D12_UNORDERED_ACCESS_VIEW_DESC stRawBufferUavDesc = {};
				stRawBufferUavDesc.ViewDimension		= D3D12_UAV_DIMENSION_BUFFER;
				stRawBufferUavDesc.Buffer.Flags			= D3D12_BUFFER_UAV_FLAG_RAW;
				stRawBufferUavDesc.Format				= DXGI_FORMAT_R32_TYPELESS;
				stRawBufferUavDesc.Buffer.NumElements	= nNumBufferElements;


				CD3DX12_CPU_DESCRIPTOR_HANDLE stBottomLevelDescriptor(pIDXRUAVHeap->GetCPUDescriptorHandleForHeapStart(), c_nDSHIndxASBottom1, nSRVDescriptorSize);
				// Only compute fallback requires a valid descriptor index when creating a wrapped pointer.

				if ( !pID3D12DXRFallbackDevice->UsingRaytracingDriver() )
				{
					//nDescriptorHeapIndex = AllocateDescriptor(&stBottomLevelDescriptor);
					pID3D12Device4->CreateUnorderedAccessView(pIUAVBottomLevelAccelerationStructure.Get(), nullptr, &stRawBufferUavDesc, stBottomLevelDescriptor);
				}
				stInstanceDesc.AccelerationStructure = pID3D12DXRFallbackDevice->GetWrappedPointerSimple(c_nDSHIndxASBottom1
					, pIUAVBottomLevelAccelerationStructure->GetGPUVirtualAddress());


				GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC::Buffer(sizeof(stInstanceDesc)),
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&pIUploadBufInstanceDescs)));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(pIUploadBufInstanceDescs);

				void* pMappedData;
				pIUploadBufInstanceDescs->Map(0, nullptr, &pMappedData);
				memcpy(pMappedData, &stInstanceDesc, sizeof(stInstanceDesc));
				pIUploadBufInstanceDescs->Unmap(0, nullptr);

			}
			else // DirectX Raytracing
			{
				D3D12_RAYTRACING_INSTANCE_DESC stInstanceDesc = {};
				stInstanceDesc.Transform[0][0] = stInstanceDesc.Transform[1][1] = stInstanceDesc.Transform[2][2] = 1;
				stInstanceDesc.InstanceMask = 1;
				stInstanceDesc.AccelerationStructure = pIUAVBottomLevelAccelerationStructure->GetGPUVirtualAddress();

				GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
					&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
					D3D12_HEAP_FLAG_NONE,
					&CD3DX12_RESOURCE_DESC::Buffer(sizeof(stInstanceDesc)),
					D3D12_RESOURCE_STATE_GENERIC_READ,
					nullptr,
					IID_PPV_ARGS(&pIUploadBufInstanceDescs)));
				GRS_SET_D3D12_DEBUGNAME_COMPTR(pIUploadBufInstanceDescs);

				void* pMappedData;
				pIUploadBufInstanceDescs->Map(0, nullptr, &pMappedData);
				memcpy(pMappedData, &stInstanceDesc, sizeof(stInstanceDesc));
				pIUploadBufInstanceDescs->Unmap(0, nullptr);
			}

			// Create a wrapped pointer to the acceleration structure.
			if (!bISDXRSupport)
			{
				UINT nNumBufferElements = static_cast<UINT>(stTopLevelPrebuildInfo.ResultDataMaxSizeInBytes) / sizeof(UINT32);

				D3D12_UNORDERED_ACCESS_VIEW_DESC rawBufferUavDesc = {};
				rawBufferUavDesc.ViewDimension		= D3D12_UAV_DIMENSION_BUFFER;
				rawBufferUavDesc.Buffer.Flags		= D3D12_BUFFER_UAV_FLAG_RAW;
				rawBufferUavDesc.Format				= DXGI_FORMAT_R32_TYPELESS;
				rawBufferUavDesc.Buffer.NumElements = nNumBufferElements;

				// Only compute fallback requires a valid descriptor index when creating a wrapped pointer.
				CD3DX12_CPU_DESCRIPTOR_HANDLE stBottomLevelDescriptor(pIDXRUAVHeap->GetCPUDescriptorHandleForHeapStart(), c_nDSHIndxASBottom2, nSRVDescriptorSize);

				if (!pID3D12DXRFallbackDevice->UsingRaytracingDriver())
				{
					//descriptorHeapIndex = AllocateDescriptor(&bottomLevelDescriptor);
					pID3D12Device4->CreateUnorderedAccessView(pIUAVTopLevelAccelerationStructure.Get(), nullptr, &rawBufferUavDesc, stBottomLevelDescriptor);
				}
				pFallbackTopLevelAccelerationStructurePointer = pID3D12DXRFallbackDevice->GetWrappedPointerSimple(c_nDSHIndxASBottom2
					, pIUAVTopLevelAccelerationStructure->GetGPUVirtualAddress());
			}

			// Bottom Level Acceleration Structure desc
			{
				stBottomLevelBuildDesc.ScratchAccelerationStructureData = pIUAVScratchResource->GetGPUVirtualAddress();
				stBottomLevelBuildDesc.DestAccelerationStructureData = pIUAVBottomLevelAccelerationStructure->GetGPUVirtualAddress();
			}

			// Top Level Acceleration Structure desc
			{
				stTopLevelBuildDesc.DestAccelerationStructureData = pIUAVTopLevelAccelerationStructure->GetGPUVirtualAddress();
				stTopLevelBuildDesc.ScratchAccelerationStructureData = pIUAVScratchResource->GetGPUVirtualAddress();
				stTopLevelBuildDesc.Inputs.InstanceDescs = pIUploadBufInstanceDescs->GetGPUVirtualAddress();
			}

			// Build acceleration structure.
			if (!bISDXRSupport)
			{
				// Set the descriptor heaps to be used during acceleration structure build for the Fallback Layer.
				ID3D12DescriptorHeap* pDescriptorHeaps[] = { pIDXRUAVHeap.Get() };
				pIDXRFallbackCMDList->SetDescriptorHeaps(ARRAYSIZE(pDescriptorHeaps), pDescriptorHeaps);
				pIDXRFallbackCMDList->BuildRaytracingAccelerationStructure(&stBottomLevelBuildDesc, 0, nullptr);
				pICMDList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(pIUAVBottomLevelAccelerationStructure.Get()));
				pIDXRFallbackCMDList->BuildRaytracingAccelerationStructure(&stTopLevelBuildDesc, 0, nullptr);

			}
			else // DirectX Raytracing
			{
				pIDXRCmdList->BuildRaytracingAccelerationStructure(&stBottomLevelBuildDesc, 0, nullptr);
				pICMDList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(pIUAVBottomLevelAccelerationStructure.Get()));
				pIDXRCmdList->BuildRaytracingAccelerationStructure(&stTopLevelBuildDesc, 0, nullptr);
			}

			// Kick off acceleration structure construction.
			GRS_THROW_IF_FAILED(pICMDList->Close());
			ID3D12CommandList* pIarCMDList[] = { pICMDList.Get() };
			pICMDQueue->ExecuteCommandLists(ARRAYSIZE(pIarCMDList), pIarCMDList);

			const UINT64 n64CurFenceValue = n64FenceValue;
			GRS_THROW_IF_FAILED(pICMDQueue->Signal(pIFence1.Get(), n64CurFenceValue));
			n64FenceValue++;

			//---------------------------------------------------------------------------------------------
			// ��������û������ִ�е�Χ����ǵ����û�о������¼�ȥ�ȴ���ע��ʹ�õ���������ж����ָ��
			if (pIFence1->GetCompletedValue() < n64CurFenceValue)
			{
				GRS_THROW_IF_FAILED(pIFence1->SetEventOnCompletion(n64CurFenceValue, hEventFence1));
				WaitForSingleObject(hEventFence1, INFINITE);
			}

		}

		//����Shader Table
		{
			void* pRayGenShaderIdentifier;
			void* pMissShaderIdentifier;
			void* pHitGroupShaderIdentifier;

			// Get shader identifiers.
			UINT nShaderIdentifierSize = 0;
			if ( !bISDXRSupport )
			{
				pRayGenShaderIdentifier		= pIDXRFallbackPSO->GetShaderIdentifier(c_pszRaygenShaderName);
				pMissShaderIdentifier		= pIDXRFallbackPSO->GetShaderIdentifier(c_pszMissShaderName);
				pHitGroupShaderIdentifier	= pIDXRFallbackPSO->GetShaderIdentifier(c_pszHitGroupName);
				nShaderIdentifierSize		= pID3D12DXRFallbackDevice->GetShaderIdentifierSize();
			}
			else // DirectX Raytracing
			{
				ComPtr<ID3D12StateObjectPropertiesPrototype> pIDXRStateObjectProperties;
				GRS_THROW_IF_FAILED(pIDXRPSO.As(&pIDXRStateObjectProperties));

				pRayGenShaderIdentifier		= pIDXRStateObjectProperties->GetShaderIdentifier(c_pszRaygenShaderName);
				pMissShaderIdentifier		= pIDXRStateObjectProperties->GetShaderIdentifier(c_pszMissShaderName);
				pHitGroupShaderIdentifier	= pIDXRStateObjectProperties->GetShaderIdentifier(c_pszHitGroupName);
				nShaderIdentifierSize		= D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
			}

			// Ray gen shader table
			{
				UINT nNumShaderRecords = 1;
				UINT nShaderRecordSize = nShaderIdentifierSize;
				ShaderTable objRayGenShaderTable(pID3D12Device4.Get(), nNumShaderRecords, nShaderRecordSize, L"RayGenShaderTable");
				objRayGenShaderTable.push_back(ShaderRecord(pRayGenShaderIdentifier, nShaderIdentifierSize));
				pIRESRayGenShaderTable = objRayGenShaderTable.GetResource();
			}

			// Miss shader table
			{
				UINT nNumShaderRecords = 1;
				UINT nShaderRecordSize = nShaderIdentifierSize;
				ShaderTable objMissShaderTable(pID3D12Device4.Get(), nNumShaderRecords, nShaderRecordSize, L"MissShaderTable");
				objMissShaderTable.push_back(ShaderRecord(pMissShaderIdentifier, nShaderIdentifierSize));
				pIRESMissShaderTable = objMissShaderTable.GetResource();
			}

			// Hit group shader table
			{
				//struct RootArguments {
				//	ST_MODULE_CONSANTBUFFER cb;
				//} rootArguments;
				//rootArguments.cb = stCBModule;

				UINT nNumShaderRecords = 1;
				UINT nShaderRecordSize = nShaderIdentifierSize + sizeof(stCBModule);
				ShaderTable hitGroupShaderTable(pID3D12Device4.Get(), nNumShaderRecords, nShaderRecordSize, L"HitGroupShaderTable");
				hitGroupShaderTable.push_back(ShaderRecord(pHitGroupShaderIdentifier, nShaderIdentifierSize, &stCBModule, sizeof(stCBModule)));
				pIRESHitGroupShaderTable = hitGroupShaderTable.GetResource();
			}
		}
		//=====================================================================================================================

		//������������
		{
			size_t szCBBuf = GRS_UPPER(sizeof(ST_SCENE_CONSANTBUFFER), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
			szCBBuf *= c_nFrameBackBufCount;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommittedResource(
				&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
				D3D12_HEAP_FLAG_NONE,
				&CD3DX12_RESOURCE_DESC::Buffer(szCBBuf),
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&pICBFrameConstant)));

			CD3DX12_RANGE readRange(0, 0);
			GRS_THROW_IF_FAILED(pICBFrameConstant->Map(0, nullptr, reinterpret_cast<void**>(&pstCBScene)));
		}

		//��Ϣѭ������Ⱦ���壩
		{
			//��������ʾ��ˢ�´��ڣ����������д��ڳ�ʱ�䲻��Ӧ������
			ShowWindow(hWnd, nCmdShow);
			UpdateWindow(hWnd);

			////�״�ִ�У���������ģ��һ֡��Ⱦ����ʱ�ı���״̬
			SetEvent(hEventFence1);
			//GRS_THROW_IF_FAILED(pICMDList->Close());
			//const DWORD dwWaitMsgVal = arWaitHandles.GetCount();

			// ������ʱ�������Ա��ڴ�����Ч����Ϣѭ��
			HANDLE hWaitTime = CreateWaitableTimer(NULL, FALSE, NULL);
			LARGE_INTEGER liDueTime = {};
			liDueTime.QuadPart = -1i64;//1���ʼ��ʱ
			SetWaitableTimer(hWaitTime, &liDueTime, 1, NULL, NULL, 0);//40ms������
			arWaitHandles.Add(hWaitTime);


			float fovAngleY = 120.0f;
			float fAspectRatio = static_cast<float>(iWidth) / static_cast<float>(iHeight);
			XMMATRIX mxView = XMMatrixLookAtLH(g_vEye, g_vLookAt, g_vUp);
			XMMATRIX mxProj = XMMatrixPerspectiveFovLH(XMConvertToRadians(fovAngleY), fAspectRatio, 1.0f, 125.0f);
			XMMATRIX mxViewProj = mxView * mxProj;

			
			XMFLOAT4 lightPosition = XMFLOAT4(0.0f, 10.8f, -3.0f, 0.0f);
			pstCBScene->m_vLightPos = XMLoadFloat4(&lightPosition);

			XMFLOAT4 lightAmbientColor = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
			pstCBScene->m_vLightAmbientColor = XMLoadFloat4(&lightAmbientColor);

			XMFLOAT4 lightDiffuseColor = XMFLOAT4(0.5f, 0.5f, 0.0f, 1.0f);
			pstCBScene->m_vLightDiffuseColor = XMLoadFloat4(&lightDiffuseColor);


			// ��ʼ��Ϣѭ�����������в�����Ⱦ
			DWORD dwRet = 0;
			BOOL bExit = FALSE;
			while (!bExit)
			{
				dwRet = ::MsgWaitForMultipleObjects(static_cast<DWORD>(arWaitHandles.GetCount()), arWaitHandles.GetData(), FALSE, INFINITE, QS_ALLINPUT);
				switch (dwRet - WAIT_OBJECT_0)
				{
				case 0:
				{//OnRender()��һ֡��Ⱦ������������һ֡ hEventFence1 ���ź�״̬
					{
						pstCBScene->m_vCameraPos = g_vEye;
						pstCBScene->m_mxP2W = XMMatrixInverse(nullptr, mxViewProj);

						//---------------------------------------------------------------------------------------------
						//��ȡ�µĺ󻺳���ţ���ΪPresent�������ʱ�󻺳����ž͸�����
						nCurFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();
						//�����������Resetһ��
						GRS_THROW_IF_FAILED(pICMDAlloc->Reset());
						//Reset�����б�������ָ�������������PSO����
						GRS_THROW_IF_FAILED(pICMDList->Reset(pICMDAlloc.Get(), nullptr));
						//---------------------------------------------------------------------------------------------

						pICMDList->ResourceBarrier
						(
							1
							, &CD3DX12_RESOURCE_BARRIER::Transition(
								pIRenderTargetBufs[nCurFrameIndex].Get()
								, D3D12_RESOURCE_STATE_PRESENT
								, D3D12_RESOURCE_STATE_RENDER_TARGET
							)
						);

						//ƫ��������ָ�뵽ָ��֡������ͼλ��
						CD3DX12_CPU_DESCRIPTOR_HANDLE stRTVHandle(pIRTVHeap->GetCPUDescriptorHandleForHeapStart(), nCurFrameIndex, nRTVDescriptorSize);
						CD3DX12_CPU_DESCRIPTOR_HANDLE stDSVHandle(pIDSVHeap->GetCPUDescriptorHandleForHeapStart());
						//������ȾĿ��
						pICMDList->OMSetRenderTargets(1, &stRTVHandle, FALSE, &stDSVHandle);

						pICMDList->RSSetViewports(1, &stViewPort);
						pICMDList->RSSetScissorRects(1, &stScissorRect);


						pICMDList->ClearRenderTargetView(stRTVHandle, c_faClearColor, 0, nullptr);
						pICMDList->ClearDepthStencilView(pIDSVHeap->GetCPUDescriptorHandleForHeapStart()
							, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
						//---------------------------------------------------------------------------------------------
						//��ʼ��Ⱦ

						auto DispatchRays = [&](auto* commandList, auto* stateObject, auto* dispatchDesc)
						{
							// Since each shader table has only one shader record, the stride is same as the size.
							dispatchDesc->HitGroupTable.StartAddress = pIRESHitGroupShaderTable->GetGPUVirtualAddress();
							dispatchDesc->HitGroupTable.SizeInBytes = pIRESHitGroupShaderTable->GetDesc().Width;
							dispatchDesc->HitGroupTable.StrideInBytes = dispatchDesc->HitGroupTable.SizeInBytes;
							dispatchDesc->MissShaderTable.StartAddress = pIRESMissShaderTable->GetGPUVirtualAddress();
							dispatchDesc->MissShaderTable.SizeInBytes = pIRESMissShaderTable->GetDesc().Width;
							dispatchDesc->MissShaderTable.StrideInBytes = dispatchDesc->MissShaderTable.SizeInBytes;
							dispatchDesc->RayGenerationShaderRecord.StartAddress = pIRESRayGenShaderTable->GetGPUVirtualAddress();
							dispatchDesc->RayGenerationShaderRecord.SizeInBytes = pIRESRayGenShaderTable->GetDesc().Width;
							dispatchDesc->Width = iWidth;
							dispatchDesc->Height = iHeight;
							dispatchDesc->Depth = 1;
							commandList->SetPipelineState1(stateObject);
							commandList->DispatchRays(dispatchDesc);
						};

						auto SetCommonPipelineState = [&](auto* descriptorSetCommandList)
						{
							descriptorSetCommandList->SetDescriptorHeaps(1,pIDXRUAVHeap.GetAddressOf());
							// Set index and successive vertex buffer decriptor tables
							CD3DX12_GPU_DESCRIPTOR_HANDLE objIBHandle(pIDXRUAVHeap->GetGPUDescriptorHandleForHeapStart(), c_nDSHIndxIBView, nSRVDescriptorSize);
							pICMDList->SetComputeRootDescriptorTable(3, objIBHandle);
							CD3DX12_GPU_DESCRIPTOR_HANDLE objUAVHandle(pIDXRUAVHeap->GetGPUDescriptorHandleForHeapStart(), c_nDSHIndxUAVOutput, nSRVDescriptorSize);
							pICMDList->SetComputeRootDescriptorTable(0, objUAVHandle);
						};

						pICMDList->SetComputeRootSignature(pIRSGlobal.Get());
						pICMDList->SetComputeRootConstantBufferView(2, pICBFrameConstant->GetGPUVirtualAddress());

						// Bind the heaps, acceleration structure and dispatch rays.
						D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
						if (!bISDXRSupport)
						{
							SetCommonPipelineState(pIDXRFallbackCMDList.Get());
							pIDXRFallbackCMDList->SetTopLevelAccelerationStructure(1, pFallbackTopLevelAccelerationStructurePointer);
							DispatchRays(pIDXRFallbackCMDList.Get(), pIDXRFallbackPSO.Get(), &dispatchDesc);
						}
						else // DirectX Raytracing
						{
							SetCommonPipelineState(pICMDList.Get());
							pICMDList->SetComputeRootShaderResourceView(1, pIUAVTopLevelAccelerationStructure->GetGPUVirtualAddress());
							DispatchRays(pIDXRCmdList.Get(), pIDXRPSO.Get(), &dispatchDesc);
						}
						//---------------------------------------------------------------------------------------------
						D3D12_RESOURCE_BARRIER preCopyBarriers[2];
						preCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
							pIRenderTargetBufs[nCurFrameIndex].Get()
							, D3D12_RESOURCE_STATE_RENDER_TARGET
							, D3D12_RESOURCE_STATE_COPY_DEST);
						preCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
							pIDXRUAVBufs.Get()
							, D3D12_RESOURCE_STATE_UNORDERED_ACCESS
							, D3D12_RESOURCE_STATE_COPY_SOURCE);

						pICMDList->ResourceBarrier(ARRAYSIZE(preCopyBarriers), preCopyBarriers);

						pICMDList->CopyResource(
							pIRenderTargetBufs[nCurFrameIndex].Get()
							, pIDXRUAVBufs.Get());

						D3D12_RESOURCE_BARRIER postCopyBarriers[2];
						postCopyBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
							pIRenderTargetBufs[nCurFrameIndex].Get()
							, D3D12_RESOURCE_STATE_COPY_DEST
							, D3D12_RESOURCE_STATE_PRESENT);
						postCopyBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
							pIDXRUAVBufs.Get()
							, D3D12_RESOURCE_STATE_COPY_SOURCE
							, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

						pICMDList->ResourceBarrier(ARRAYSIZE(postCopyBarriers), postCopyBarriers);
						
						//�ر������б�����ȥִ����
						GRS_THROW_IF_FAILED(pICMDList->Close());

						//ִ�������б�
						ID3D12CommandList* ppCommandLists[] = { pICMDList.Get() };
						pICMDQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

						//�ύ����
						GRS_THROW_IF_FAILED(pISwapChain3->Present(1, 0));

						//---------------------------------------------------------------------------------------------
						//��ʼͬ��GPU��CPU��ִ�У��ȼ�¼Χ�����ֵ
						const UINT64 n64CurFenceValue = n64FenceValue;
						GRS_THROW_IF_FAILED(pICMDQueue->Signal(pIFence1.Get(), n64CurFenceValue));
						n64FenceValue++;
						GRS_THROW_IF_FAILED(pIFence1->SetEventOnCompletion(n64CurFenceValue, hEventFence1));
					}
				}
				break;
				case 1:
				{// ��ʱ����

				}
				break;
				case 2://arWaitHandles.GetCount() == 2
				{//������Ϣ
					while (::PeekMessage(&stMsg, NULL, 0, 0, PM_REMOVE))
					{
						if (WM_QUIT != stMsg.message)
						{
							::TranslateMessage(&stMsg);
							::DispatchMessage(&stMsg);
						}
						else
						{
							bExit = TRUE;
						}
					}
				}
				break;
				case WAIT_TIMEOUT:
				{//��ʱ����

				}
				break;
				default:
					break;
				}


			}
		}


	}
	catch (CGRSCOMException& e)
	{//������COM�쳣
		e.Error();
	}

	::_tsystem(_T("PAUSE"));
	GRS_FREE_OUTPUT();

	return 0;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

BOOL LoadMeshVertex(const CHAR* pszMeshFileName, UINT& nVertexCnt, ST_GRS_VERTEX*& ppVertex, GRS_TYPE_INDEX*& ppIndices)
{
	ifstream fin;
	char input;
	BOOL bRet = TRUE;
	try
	{
		fin.open(pszMeshFileName);
		if (fin.fail())
		{
			throw CGRSCOMException(E_FAIL);
		}
		fin.get(input);
		while (input != ':')
		{
			fin.get(input);
		}
		fin >> nVertexCnt;

		fin.get(input);
		while (input != ':')
		{
			fin.get(input);
		}
		fin.get(input);
		fin.get(input);

		ppVertex = (ST_GRS_VERTEX*)HeapAlloc(::GetProcessHeap()
			, HEAP_ZERO_MEMORY
			, nVertexCnt * sizeof(ST_GRS_VERTEX));
		ppIndices = (GRS_TYPE_INDEX*)HeapAlloc(::GetProcessHeap()
			, HEAP_ZERO_MEMORY
			, nVertexCnt * sizeof(GRS_TYPE_INDEX));

		for (UINT i = 0; i < nVertexCnt; i++)
		{
			fin >> ppVertex[i].m_vPos.x >> ppVertex[i].m_vPos.y >> ppVertex[i].m_vPos.z;
			ppVertex[i].m_vPos.w = 1.0f;
			fin >> ppVertex[i].m_vTex.x >> ppVertex[i].m_vTex.y;
			fin >> ppVertex[i].m_vNor.x >> ppVertex[i].m_vNor.y >> ppVertex[i].m_vNor.z;

			ppIndices[i] = static_cast<GRS_TYPE_INDEX>(i);
		}
	}
	catch (CGRSCOMException& e)
	{
		e;
		bRet = FALSE;
	}
	return bRet;
}
