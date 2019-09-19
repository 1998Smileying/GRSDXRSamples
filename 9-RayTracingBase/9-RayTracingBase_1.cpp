#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN // �� Windows ͷ���ų�����ʹ�õ�����
#include <windows.h>
#include <tchar.h>
#include <wrl.h>  //���WTL֧�� ����ʹ��COM
#include <strsafe.h>
#include <atlcoll.h> //for atl array
#include <dxgi1_6.h>
#include <DirectXMath.h>

//for d3d12
#include <d3d12.h>
#include <d3d12shader.h>
#include <d3dcompiler.h>

#if defined(_DEBUG)
#include <dxgidebug.h>
#endif

#include "..\WindowsCommons\d3dx12.h"

using namespace Microsoft;
using namespace Microsoft::WRL;
using namespace DirectX;

//linker
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")


#define GRS_WND_CLASS_NAME _T("Game Window Class")
#define GRS_WND_TITLE	_T("GRS DirectX12 RayTracing Base Sample")

//�¶���ĺ�������ȡ������
#define GRS_UPPER_DIV(A,B) ((UINT)(((A)+((B)-1))/(B)))
//���������ϱ߽�����㷨 �ڴ�����г��� ���ס
#define GRS_UPPER(A,B) ((UINT)(((A)+((B)-1))&~(B - 1)))

#define GRS_SAFE_RELEASE(p) if(p){(p)->Release();(p)=nullptr;}
#define GRS_THROW_IF_FAILED(hr) { HRESULT _hr = (hr); if (FAILED(_hr)){ throw CGRSCOMException(_hr); } }

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


class CGRSCOMException
{
public:
	CGRSCOMException(HRESULT hr) : m_hrError(hr)
	{
	}
	HRESULT Error() const
	{
		return m_hrError;
	}
private:
	const HRESULT m_hrError;
};

struct GRS_VERTEX
{
	XMFLOAT3 m_vPos;
	XMFLOAT4 color;
};

LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR    lpCmdLine, int nCmdShow)
{
	int iWidth = 1024;
	int iHeight = 768;

	HWND hWnd = nullptr;
	MSG	msg = {};

	const UINT c_nFrameBackBufCount = 3u;
	UINT nFrameIndex = 0;
	UINT nDXGIFactoryFlags = 0U;	
	DXGI_FORMAT fmtDepthStencil = DXGI_FORMAT_D24_UNORM_S8_UINT;
	const float c_faClearColor[] = { 0.2f, 0.5f, 1.0f, 1.0f };
	CD3DX12_VIEWPORT stViewPort(0.0f, 0.0f, static_cast<float>(iWidth), static_cast<float>(iHeight));
	CD3DX12_RECT	 stScissorRect(0, 0, static_cast<LONG>(iWidth), static_cast<LONG>(iHeight));
	
	ComPtr<IDXGIFactory5>				pIDXGIFactory5;
	ComPtr<IDXGIFactory6>				pIDXGIFactory6;
	ComPtr<ID3D12Device4>				pID3D12Device4;

	ComPtr<ID3D12CommandQueue>			pICMDQueue;
	ComPtr<ID3D12CommandAllocator>		pICMDAlloc;
	ComPtr<ID3D12GraphicsCommandList>	pICMDList;
	ComPtr<ID3D12Fence1>				pIFence1;
	
	ComPtr<ID3D12DescriptorHeap>		pIRTVHeap;   //Render Target View
	ComPtr<ID3D12DescriptorHeap>		pIDSVHeap;   //Depth Stencil View

	ComPtr<IDXGISwapChain1>				pISwapChain1;
	ComPtr<IDXGISwapChain3>				pISwapChain3;

	ComPtr<ID3D12Resource>				pIRenderTargetBufs[c_nFrameBackBufCount];
	ComPtr<ID3D12Resource>				pIDepthStencilBuf;

	HANDLE								hEventFence1 = nullptr;
	UINT64								n64FenceValue = 0i64;
	CAtlArray<HANDLE>					arWaitHandles;

	UINT								nRTVDescriptorSize = 0;
	
	try
	{
		GRS_THROW_IF_FAILED(::CoInitialize(nullptr));  //for WIC & COM

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
			RegisterClassEx(&wcex);

			DWORD dwWndStyle = WS_OVERLAPPED | WS_SYSMENU;
			RECT rtWnd = { 0, 0, iWidth, iHeight };
			AdjustWindowRect(&rtWnd, dwWndStyle, FALSE);

			hWnd = CreateWindowW(GRS_WND_CLASS_NAME
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
				return FALSE;
			}

			ShowWindow(hWnd, nCmdShow);
			UpdateWindow(hWnd);
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
		}

		//ö����ʾ�������������豸
		{
			ComPtr<IDXGIAdapter1> pIDXGIAdapter1;
			GRS_THROW_IF_FAILED(pIDXGIFactory6->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&pIDXGIAdapter1)));
			GRS_THROW_IF_FAILED(D3D12CreateDevice(pIDXGIAdapter1.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&pID3D12Device4)));

			//ComPtr<ID3D12Device> testDevice;
			//ComPtr<ID3D12DeviceRaytracingPrototype> testRaytracingDevice;
			//UUID experimentalFeatures[] = { D3D12ExperimentalShaderModels, D3D12RaytracingPrototype };

			//return SUCCEEDED(D3D12EnableExperimentalFeatures(2, experimentalFeatures, nullptr, nullptr))
			//	&& SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&testDevice)))
			//	&& SUCCEEDED(testDevice->QueryInterface(IID_PPV_ARGS(&testRaytracingDevice)));
					
		}

		//����������С�����������������б�
		{
			D3D12_COMMAND_QUEUE_DESC stCMDQueueDesc = {};
			stCMDQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
			stCMDQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandQueue(&stCMDQueueDesc, IID_PPV_ARGS(&pICMDQueue)));

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&pICMDAlloc)));
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,pICMDAlloc.Get(),nullptr,IID_PPV_ARGS(&pICMDList)));
		}

		//����Fence
		{
			// ����һ��ͬ�����󡪡�Χ�������ڵȴ���Ⱦ��ɣ���Ϊ����Draw Call���첽����
			GRS_THROW_IF_FAILED(pID3D12Device4->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&pIFence1)));
			// ����һ��Eventͬ���������ڵȴ�Χ���¼�֪ͨ
			hEventFence1 = CreateEvent(nullptr, FALSE, FALSE, nullptr);
			if ( nullptr == hEventFence1 )
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

			nRTVDescriptorSize = pID3D12Device4->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);


			D3D12_DESCRIPTOR_HEAP_DESC stDSVHeapSesc = {};
			stDSVHeapSesc.NumDescriptors = 1;						//ͨ�� DSV -> Depth Stencil Bufferֻ��һ��
			stDSVHeapSesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;

			GRS_THROW_IF_FAILED(pID3D12Device4->CreateDescriptorHeap(&stDSVHeapSesc, IID_PPV_ARGS(&pIDSVHeap)));

		}

		//����������
		{
			DXGI_SWAP_CHAIN_DESC1 stSwapChainDesc = {};
			stSwapChainDesc.BufferCount			= c_nFrameBackBufCount;
			stSwapChainDesc.Width				= iWidth;
			stSwapChainDesc.Height				= iHeight;
			stSwapChainDesc.Format				= DXGI_FORMAT_R8G8B8A8_UNORM;
			stSwapChainDesc.BufferUsage			= DXGI_USAGE_RENDER_TARGET_OUTPUT;
			stSwapChainDesc.SwapEffect			= DXGI_SWAP_EFFECT_FLIP_DISCARD;
			stSwapChainDesc.SampleDesc.Count	= 1;

			GRS_THROW_IF_FAILED(pIDXGIFactory6->CreateSwapChainForHwnd(
				pICMDQueue.Get(),		
				hWnd,
				&stSwapChainDesc,
				nullptr,
				nullptr,
				&pISwapChain1
			));

			//ע��˴�ʹ���˸߰汾��SwapChain�ӿڵĺ���
			GRS_THROW_IF_FAILED(pISwapChain1.As(&pISwapChain3));
			nFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();

			//---------------------------------------------------------------------------------------------
			CD3DX12_CPU_DESCRIPTOR_HANDLE stRTVHandle(pIRTVHeap->GetCPUDescriptorHandleForHeapStart());
			for (UINT i = 0; i < c_nFrameBackBufCount; i++)
			{//���ѭ����©����������ʵ�����Ǹ�����ı���
				GRS_THROW_IF_FAILED(pISwapChain3->GetBuffer(i, IID_PPV_ARGS(&pIRenderTargetBufs[i])));
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

		//��Ϣѭ������Ⱦ���壩
		{
			// ������ʱ�������Ա��ڴ�����Ч����Ϣѭ��
			HANDLE hWaitTime = CreateWaitableTimer(NULL, FALSE, NULL);
			LARGE_INTEGER liDueTime = {};
			liDueTime.QuadPart = -1i64;//1���ʼ��ʱ
			SetWaitableTimer(hWaitTime, &liDueTime, 1, NULL, NULL, 0);//40ms������

			arWaitHandles.Add(hWaitTime);

			// ��ʼ��Ϣѭ�����������в�����Ⱦ
			DWORD dwRet = 0;
			BOOL bExit = FALSE;

			//�״�ִ�У���������ģ��һ֡��Ⱦ����ʱ�ı���״̬
			SetEvent(hEventFence1);
			GRS_THROW_IF_FAILED(pICMDList->Close());
			//const DWORD dwWaitMsgVal = arWaitHandles.GetCount();
			while (!bExit)
			{
				dwRet = ::MsgWaitForMultipleObjects(static_cast<DWORD>(arWaitHandles.GetCount()), arWaitHandles.GetData(), FALSE, INFINITE, QS_ALLINPUT);
				switch (dwRet - WAIT_OBJECT_0)
				{
				case 0:
				{//OnRender()��һ֡��Ⱦ������������һ֡ hEventFence1 ���ź�״̬
					{
						//---------------------------------------------------------------------------------------------
						//��ȡ�µĺ󻺳���ţ���ΪPresent�������ʱ�󻺳����ž͸�����
						nFrameIndex = pISwapChain3->GetCurrentBackBufferIndex();
						//�����������Resetһ��
						GRS_THROW_IF_FAILED(pICMDAlloc->Reset());
						//Reset�����б�������ָ�������������PSO����
						GRS_THROW_IF_FAILED(pICMDList->Reset(pICMDAlloc.Get(), nullptr));
						//---------------------------------------------------------------------------------------------

						pICMDList->ResourceBarrier
						(
							1
							, &CD3DX12_RESOURCE_BARRIER::Transition(
								pIRenderTargetBufs[nFrameIndex].Get()
								, D3D12_RESOURCE_STATE_PRESENT
								, D3D12_RESOURCE_STATE_RENDER_TARGET
							)
						);

						//ƫ��������ָ�뵽ָ��֡������ͼλ��
						CD3DX12_CPU_DESCRIPTOR_HANDLE stRTVHandle(pIRTVHeap->GetCPUDescriptorHandleForHeapStart(), nFrameIndex, nRTVDescriptorSize);
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



						//---------------------------------------------------------------------------------------------
						pICMDList->ResourceBarrier
						(
							1
							, &CD3DX12_RESOURCE_BARRIER::Transition(
								pIRenderTargetBufs[nFrameIndex].Get()
								, D3D12_RESOURCE_STATE_RENDER_TARGET
								, D3D12_RESOURCE_STATE_PRESENT
							)
						);
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
					while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
					{
						if (WM_QUIT != msg.message)
						{
							::TranslateMessage(&msg);
							::DispatchMessage(&msg);
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