/* Copyright (C) 2005-2011, Thorvald Natvig <thorvald@natvig.com>

   All rights reserved.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
   - Neither the name of the Mumble Developers nor the names of its
     contributors may be used to endorse or promote products derived from this
     software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "lib.h"
#include "overlay.hex"
#include <d3d10.h>
#include <d3dx10.h>
#include <time.h>

DXGIData *dxgi = NULL;

static bool bHooked = false;
static bool bChaining = false;
static HardHook hhPresent;
static HardHook hhResize;
static HardHook hhAddRef;
static HardHook hhRelease;

typedef HRESULT(__stdcall *CreateDXGIFactoryType)(REFIID, void **);
typedef HRESULT(__stdcall *D3D10CreateDeviceAndSwapChainType)(IDXGIAdapter *, D3D10_DRIVER_TYPE, HMODULE, UINT, UINT, DXGI_SWAP_CHAIN_DESC *, IDXGISwapChain **, ID3D10Device **);

typedef HRESULT(__stdcall *D3D10CreateStateBlockType)(ID3D10Device *, D3D10_STATE_BLOCK_MASK *, ID3D10StateBlock **);
typedef HRESULT(__stdcall *D3D10StateBlockMaskEnableAllType)(D3D10_STATE_BLOCK_MASK *);
typedef HRESULT(__stdcall *D3D10CreateEffectFromMemoryType)(void *, SIZE_T, UINT, ID3D10Device *, ID3D10EffectPool *, ID3D10Effect **);

typedef HRESULT(__stdcall *PresentType)(IDXGISwapChain *, UINT, UINT);
typedef HRESULT(__stdcall *ResizeBuffersType)(IDXGISwapChain *, UINT, UINT, UINT, DXGI_FORMAT, UINT);
typedef ULONG(__stdcall *AddRefType)(ID3D10Device *);
typedef ULONG(__stdcall *ReleaseType)(ID3D10Device *);

#define HMODREF(mod, func) func##Type p##func = (func##Type) GetProcAddress(mod, #func)

struct SimpleVertex {
	D3DXVECTOR3 Pos;
	D3DXVECTOR2 Tex;
};

class D10State: protected Pipe {
	public:

		LONG lHighMark;

		LONG initRefCount;
		LONG refCount;
		LONG myRefCount;
		DWORD dwMyThread;

		D3D10_VIEWPORT vp;

		ID3D10Device *pDevice;
		IDXGISwapChain *pSwapChain;

		ID3D10StateBlock *pOrigStateBlock;
		ID3D10StateBlock *pMyStateBlock;
		ID3D10RenderTargetView *pRTV;
		ID3D10Effect *pEffect;
		ID3D10EffectTechnique *pTechnique;
		ID3D10EffectShaderResourceVariable * pDiffuseTexture;
		ID3D10InputLayout *pVertexLayout;
		ID3D10Buffer *pVertexBuffer;
		ID3D10Buffer *pIndexBuffer;
		ID3D10BlendState *pBlendState;

		ID3D10Texture2D *pTexture;
		ID3D10ShaderResourceView *pSRView;

		clock_t timeT;
		unsigned int frameCount;

		D10State(IDXGISwapChain *, ID3D10Device *);
		~D10State();
		void init();
		void draw();

		void blit(unsigned int x, unsigned int y, unsigned int w, unsigned int h);
		void setRect();
		void newTexture(unsigned int w, unsigned int h);
};

map<IDXGISwapChain *, D10State *> chains;
map<ID3D10Device *, D10State *> devices;

D10State::D10State(IDXGISwapChain *pSwapChain, ID3D10Device *pDevice) {
	this->pSwapChain = pSwapChain;
	this->pDevice = pDevice;

	lHighMark = initRefCount  = refCount = myRefCount = 0;
	dwMyThread = 0;

	ZeroMemory(&vp, sizeof(vp));

	pOrigStateBlock = NULL;
	pMyStateBlock = NULL;
	pRTV = NULL;
	pEffect = NULL;
	pTechnique = NULL;
	pDiffuseTexture = NULL;
	pVertexLayout = NULL;
	pVertexBuffer = NULL;
	pIndexBuffer = NULL;
	pBlendState = NULL;
	pTexture = NULL;
	pSRView = NULL;

	timeT = clock();
	frameCount = 0;

	pDevice->AddRef();
	initRefCount = pDevice->Release();
}

void D10State::blit(unsigned int x, unsigned int y, unsigned int w, unsigned int h) {
	HRESULT hr;

	ods("D3D10: Blit %d %d %d %d", x, y, w, h);

	if (! pTexture || ! pSRView)
		return;

	D3D10_MAPPED_TEXTURE2D mappedTex;
	hr = pTexture->Map(D3D10CalcSubresource(0, 0, 1), D3D10_MAP_WRITE_DISCARD, 0, &mappedTex);
	if (FAILED(hr)) {
		ods("D3D10: Failed map");
	}

	UCHAR* pTexels = (UCHAR*)mappedTex.pData;

	for (unsigned int r=0;r< uiHeight; ++r) {
		unsigned char *sptr = a_ucTexture + r * uiWidth * 4;
		unsigned char *dptr = reinterpret_cast<unsigned char *>(pTexels) + r * mappedTex.RowPitch;
		memcpy(dptr, sptr, uiWidth * 4);
	}

	pTexture->Unmap(D3D10CalcSubresource(0, 0, 1));
}

void D10State::setRect() {
	HRESULT hr;

	ods("D3D10: Setrect");

	float w = static_cast<float>(uiWidth);
	float h = static_cast<float>(uiHeight);

	float left   = static_cast<float>(uiLeft) - 0.5f;
	float top    = static_cast<float>(uiTop) - 0.5f;
	float right  = static_cast<float>(uiRight) + 0.5f;
	float bottom = static_cast<float>(uiBottom) + 0.5f;

	float texl = (left) / w;
	float text = (top) / h;
	float texr = (right + 1.0f) / w;
	float texb = (bottom + 1.0f) / h;

	left = 2.0f * (left / vp.Width) - 1.0f;
	right = 2.0f * (right / vp.Width) - 1.0f;
	top = -2.0f * (top / vp.Height) + 1.0f;
	bottom = -2.0f * (bottom / vp.Height) + 1.0f;

	ods("Vertex (%f %f) (%f %f)", left, top, right, bottom);

	// Create vertex buffer
	SimpleVertex vertices[] = {
		{ D3DXVECTOR3(left, top, 0.5f), D3DXVECTOR2(texl, text) },
		{ D3DXVECTOR3(right, top, 0.5f), D3DXVECTOR2(texr, text) },
		{ D3DXVECTOR3(right, bottom, 0.5f), D3DXVECTOR2(texr, texb) },
		{ D3DXVECTOR3(left, bottom, 0.5f), D3DXVECTOR2(texl, texb) },
	};

	void *pData = NULL;

	hr = pVertexBuffer->Map(D3D10_MAP_WRITE_DISCARD, 0, &pData);
	memcpy(pData, vertices, sizeof(vertices));
	ods("Map: %lx %d", hr, sizeof(vertices));
	pVertexBuffer->Unmap();
}

void D10State::newTexture(unsigned int w, unsigned int h) {
	HRESULT hr;

	ods("D3D10: newTex %d %d", w, h);

	if (pTexture) {
		pTexture->Release();
		pTexture = NULL;
	}
	if (pSRView) {
		pSRView->Release();
		pSRView = NULL;
	}

	D3D10_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));

	desc.Width = w;
	desc.Height = h;
	desc.MipLevels = desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D10_USAGE_DYNAMIC;
	desc.BindFlags = D3D10_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
	hr = pDevice->CreateTexture2D(&desc, NULL, &pTexture);

	if (! SUCCEEDED(hr)) {
		pTexture = NULL;
		ods("D3D10: Failed to create texture.");
		return;
	}

	D3D10_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = desc.Format;
	srvDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = desc.MipLevels;

	hr = pDevice->CreateShaderResourceView(pTexture, &srvDesc, &pSRView);
	if (! SUCCEEDED(hr)) {
		pSRView = NULL;
		pTexture->Release();
		pTexture = NULL;
		ods("D3D10: Failed to create resource view.");
		return;
	}
}

void D10State::init() {
	static HMODREF(GetModuleHandleW(L"D3D10.DLL"), D3D10CreateEffectFromMemory);
	static HMODREF(GetModuleHandleW(L"D3D10.DLL"), D3D10CreateStateBlock);
	static HMODREF(GetModuleHandleW(L"D3D10.DLL"), D3D10StateBlockMaskEnableAll);

	HRESULT hr;

	dwMyThread = GetCurrentThreadId();

	D3D10_STATE_BLOCK_MASK StateBlockMask;
	ZeroMemory(&StateBlockMask, sizeof(StateBlockMask));
	pD3D10StateBlockMaskEnableAll(&StateBlockMask);
	pD3D10CreateStateBlock(pDevice, &StateBlockMask, &pOrigStateBlock);
	pD3D10CreateStateBlock(pDevice, &StateBlockMask, &pMyStateBlock);

	pOrigStateBlock->Capture();

	ID3D10Texture2D* pBackBuffer = NULL;
	hr = pSwapChain->GetBuffer(0, __uuidof(*pBackBuffer), (LPVOID*)&pBackBuffer);
	if (FAILED(hr))
		ods("D3D10: pSwapChain->GetBuffer failure!");

	pDevice->ClearState();

	D3D10_TEXTURE2D_DESC backBufferSurfaceDesc;
	pBackBuffer->GetDesc(&backBufferSurfaceDesc);

	ZeroMemory(&vp, sizeof(vp));
	vp.Width = backBufferSurfaceDesc.Width;
	vp.Height = backBufferSurfaceDesc.Height;
	vp.MinDepth = 0;
	vp.MaxDepth = 1;
	vp.TopLeftX = 0;
	vp.TopLeftY = 0;
	pDevice->RSSetViewports(1, &vp);

	hr = pDevice->CreateRenderTargetView(pBackBuffer, NULL, &pRTV);
	if (FAILED(hr))
		ods("D3D10: pDevice->CreateRenderTargetView failure!");

	pDevice->OMSetRenderTargets(1, &pRTV, NULL);

	D3D10_BLEND_DESC blend;
	ZeroMemory(&blend, sizeof(blend));
	blend.BlendEnable[0] = TRUE;
	blend.SrcBlend = D3D10_BLEND_ONE;
	blend.DestBlend = D3D10_BLEND_INV_SRC_ALPHA;
	blend.BlendOp = D3D10_BLEND_OP_ADD;
	blend.SrcBlendAlpha = D3D10_BLEND_ONE;
	blend.DestBlendAlpha = D3D10_BLEND_INV_SRC_ALPHA;
	blend.BlendOpAlpha = D3D10_BLEND_OP_ADD;
	blend.RenderTargetWriteMask[0] = D3D10_COLOR_WRITE_ENABLE_ALL;

	pDevice->CreateBlendState(&blend, &pBlendState);
	float bf[4];
	pDevice->OMSetBlendState(pBlendState, bf, 0xffffffff);

	pD3D10CreateEffectFromMemory((void *) g_main, sizeof(g_main), 0, pDevice, NULL, &pEffect);

	pTechnique = pEffect->GetTechniqueByName("Render");
	pDiffuseTexture = pEffect->GetVariableByName("txDiffuse")->AsShaderResource();

	pTexture = NULL;
	pSRView = NULL;

	// Define the input layout
	D3D10_INPUT_ELEMENT_DESC layout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
	};
	UINT numElements = sizeof(layout) / sizeof(layout[0]);

	// Create the input layout
	D3D10_PASS_DESC PassDesc;
	pTechnique->GetPassByIndex(0)->GetDesc(&PassDesc);
	hr = pDevice->CreateInputLayout(layout, numElements, PassDesc.pIAInputSignature, PassDesc.IAInputSignatureSize, &pVertexLayout);
	if (FAILED(hr))
		ods("D3D10: pDevice->CreateInputLayout failure!");
	pDevice->IASetInputLayout(pVertexLayout);

	D3D10_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(bd));
	bd.Usage = D3D10_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(SimpleVertex) * 4;
	bd.BindFlags = D3D10_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
	bd.MiscFlags = 0;
	hr = pDevice->CreateBuffer(&bd, NULL, &pVertexBuffer);
	if (FAILED(hr))
		ods("D3D10: pDevice->CreateBuffer failure!");

	DWORD indices[] = {
		0,1,3,
		1,2,3,
	};

	bd.Usage = D3D10_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(DWORD) * 6;
	bd.BindFlags = D3D10_BIND_INDEX_BUFFER;
	bd.CPUAccessFlags = 0;
	bd.MiscFlags = 0;
	D3D10_SUBRESOURCE_DATA InitData;
	ZeroMemory(&InitData, sizeof(InitData));
	InitData.pSysMem = indices;
	hr = pDevice->CreateBuffer(&bd, &InitData, &pIndexBuffer);
	if (FAILED(hr))
		ods("D3D10: pDevice->CreateBuffer failure!");

	// Set index buffer
	pDevice->IASetIndexBuffer(pIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	// Set primitive topology
	pDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	pMyStateBlock->Capture();
	pOrigStateBlock->Apply();

	pBackBuffer->Release();

	dwMyThread = 0;
}

D10State::~D10State() {
	pBlendState->Release();
	pVertexBuffer->Release();
	pIndexBuffer->Release();
	pVertexLayout->Release();
	pEffect->Release();
	pRTV->Release();
	if (pTexture)
		pTexture->Release();
	if (pSRView)
		pSRView->Release();

	pMyStateBlock->ReleaseAllDeviceObjects();
	pMyStateBlock->Release();

	pOrigStateBlock->ReleaseAllDeviceObjects();
	pOrigStateBlock->Release();
}

void D10State::draw() {
	clock_t t = clock();
	float elapsed = static_cast<float>(t - timeT) / CLOCKS_PER_SEC;
	++frameCount;
	if (elapsed > OVERLAY_FPS_INTERVAL) {
		OverlayMsg om;
		om.omh.uiMagic = OVERLAY_MAGIC_NUMBER;
		om.omh.uiType = OVERLAY_MSGTYPE_FPS;
		om.omh.iLength = sizeof(OverlayMsgFps);
		om.omf.fps = frameCount / elapsed;

		sendMessage(om);

		frameCount = 0;
		timeT = t;
	}

	dwMyThread = GetCurrentThreadId();

	checkMessage(vp.Width, vp.Height);

	if (a_ucTexture && pSRView && (uiLeft != uiRight)) {
		HRESULT hr;
		pOrigStateBlock->Capture();
		pMyStateBlock->Apply();

		D3D10_TECHNIQUE_DESC techDesc;
		pTechnique->GetDesc(&techDesc);

		// Set vertex buffer
		UINT stride = sizeof(SimpleVertex);
		UINT offset = 0;
		pDevice->IASetVertexBuffers(0, 1, &pVertexBuffer, &stride, &offset);

		hr = pDiffuseTexture->SetResource(pSRView);
		if (! SUCCEEDED(hr))
			ods("D3D10: Failed to set resource");

		for (UINT p = 0; p < techDesc.Passes; ++p) {
			pTechnique->GetPassByIndex(p)->Apply(0);
			pDevice->DrawIndexed(6, 0, 0);
		}
		pOrigStateBlock->Apply();
	}

	dwMyThread = 0;
}



static HRESULT __stdcall myPresent(IDXGISwapChain *pSwapChain, UINT SyncInterval, UINT Flags) {
	HRESULT hr;
//	ods("DXGI: Device Present");

	ID3D10Device *pDevice = NULL;

	ods("DXGI: DrawBegin");

	hr = pSwapChain->GetDevice(__uuidof(ID3D10Device), (void **) &pDevice);
	if (pDevice) {
		D10State *ds = chains[pSwapChain];
		if (ds && ds->pDevice != pDevice) {
			ods("DXGI: SwapChain device changed");
			devices.erase(ds->pDevice);
			delete ds;
			ds = NULL;
		}
		if (! ds) {
			ods("DXGI: New state");
			ds = new D10State(pSwapChain, pDevice);
			chains[pSwapChain] = ds;
			devices[pDevice] = ds;
			ds->init();
		}

		ds->draw();
		pDevice->Release();
		ods("DXGI: DrawEnd");
	}

	PresentType oPresent = (PresentType) hhPresent.call;
	hhPresent.restore();
	hr = oPresent(pSwapChain, SyncInterval, Flags);
	hhPresent.inject();
	return hr;
}

static HRESULT __stdcall myResize(IDXGISwapChain *pSwapChain, UINT BufferCount, UINT Width, UINT Height, DXGI_FORMAT NewFormat, UINT SwapChainFlags) {
	HRESULT hr;

	D10State *ds = chains[pSwapChain];
	if (ds) {
		devices.erase(ds->pDevice);
		chains.erase(pSwapChain);
		delete ds;
	}

	ResizeBuffersType oResize = (ResizeBuffersType) hhResize.call;
	hhResize.restore();
	hr = oResize(pSwapChain, BufferCount, Width, Height, NewFormat, SwapChainFlags);
	hhResize.inject();
	return hr;
}

static ULONG __stdcall myAddRef(ID3D10Device *pDevice) {
	AddRefType oAddRef = (AddRefType) hhAddRef.call;

	hhAddRef.restore();
	LONG res = oAddRef(pDevice);
	hhAddRef.inject();

	Mutex m;
	D10State *ds = devices[pDevice];
	if (ds)
		ds->lHighMark = res;

	return res;
}

static ULONG __stdcall myRelease(ID3D10Device *pDevice) {
	ReleaseType oRelease = (ReleaseType) hhRelease.call;

	hhRelease.restore();
	LONG res = oRelease(pDevice);
	hhRelease.inject();

	Mutex m;
	D10State *ds = devices[pDevice];
	if (ds)
		if (res < (ds->lHighMark / 2)) {
			ods("D3D10: Deleting resources %d < .5 %d", res, ds->lHighMark);
			devices.erase(ds->pDevice);
			chains.erase(ds->pSwapChain);
			delete ds;
			ods("D3D10: Deleted");
			ds = NULL;
		}

	return res;
}

static void HookAddRelease(voidFunc vfAdd, voidFunc vfRelease) {
	ods("D3D10: Injecting device add/remove");
	hhAddRef.setup(vfAdd, reinterpret_cast<voidFunc>(myAddRef));
	hhRelease.setup(vfRelease, reinterpret_cast<voidFunc>(myRelease));
}

static void HookPresentRaw(voidFunc vfPresent) {
	hhPresent.setup(vfPresent, reinterpret_cast<voidFunc>(myPresent));
}

static void HookResizeRaw(voidFunc vfResize) {
	ods("DXGI: Injecting ResizeBuffers Raw");
	hhResize.setup(vfResize, reinterpret_cast<voidFunc>(myResize));
}

void checkDXGIHook(bool preonly) {
	if (bChaining) {
		ods("DXGI: Causing a chain");
		return;
	}

	if (! dxgi->iOffsetPresent || ! dxgi->iOffsetResize)
		return;

	bChaining = true;

	HMODULE hDXGI = GetModuleHandleW(L"DXGI.DLL");
	HMODULE hD3D10 = GetModuleHandleW(L"D3D10CORE.DLL");

	if (hDXGI && hD3D10) {
		if (! bHooked) {
			wchar_t procname[2048];
			GetModuleFileNameW(NULL, procname, 2048);
			fods("DXGI: Hookcheck '%ls'", procname);
			bHooked = true;

			// Add a ref to ourselves; we do NOT want to get unloaded directly from this process.
			GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, reinterpret_cast<char *>(&checkDXGIHook), &hSelf);

			// Can we use the prepatch data?
			GetModuleFileNameW(hDXGI, procname, 2048);
			if (_wcsicmp(dxgi->wcDXGIFileName, procname) == 0) {
				unsigned char *raw = (unsigned char *) hDXGI;
				HookPresentRaw((voidFunc)(raw + dxgi->iOffsetPresent));
				HookResizeRaw((voidFunc)(raw + dxgi->iOffsetResize));

				GetModuleFileNameW(hD3D10, procname, 2048);
				if (_wcsicmp(dxgi->wcD3D10FileName, procname) == 0) {
					unsigned char *raw = (unsigned char *) hD3D10;
					HookAddRelease((voidFunc)(raw + dxgi->iOffsetAddRef), (voidFunc)(raw + dxgi->iOffsetRelease));
				}
			} else if (! preonly) {
				fods("DXGI Interface changed, can't rawpatch");
			} else {
				bHooked = false;
			}
		}
	}

	bChaining = false;
}

extern "C" __declspec(dllexport) void __cdecl PrepareDXGI() {
	// This function is called by the Mumble client in Mumble's scope
	// mainly to extract the offsets of various functions in the IDXGISwapChain
	// and IDXGIObject interfaces that need to be hooked in target
	// applications. The data is stored in the dxgi shared memory structure.

	if (! dxgi)
		return;

	ods("Preparing static data for DXGI Injection");

	dxgi->wcDXGIFileName[0] = 0;
	dxgi->wcD3D10FileName[0] = 0;
	dxgi->iOffsetPresent = 0;
	dxgi->iOffsetResize = 0;
	dxgi->iOffsetAddRef = 0;
	dxgi->iOffsetRelease = 0;

	OSVERSIONINFOEXW ovi;
	memset(&ovi, 0, sizeof(ovi));
	ovi.dwOSVersionInfoSize = sizeof(ovi);
	GetVersionExW(reinterpret_cast<OSVERSIONINFOW *>(&ovi));
	if ((ovi.dwMajorVersion >= 7) || ((ovi.dwMajorVersion == 6) && (ovi.dwBuildNumber >= 6001))) { // Make sure this is vista or greater as quite a number of <=WinXP users have fake DX10 libs installed
		HMODULE hD3D10 = LoadLibrary("D3D10.DLL");
		HMODULE hDXGI = LoadLibrary("DXGI.DLL");

		if (hDXGI != NULL && hD3D10 != NULL) {
			CreateDXGIFactoryType pCreateDXGIFactory = reinterpret_cast<CreateDXGIFactoryType>(GetProcAddress(hDXGI, "CreateDXGIFactory"));
			ods("Got %p", pCreateDXGIFactory);
			if (pCreateDXGIFactory) {
				IDXGIFactory * pFactory;
				HRESULT hr = pCreateDXGIFactory(__uuidof(IDXGIFactory), (void**)(&pFactory));
				if (FAILED(hr))
					ods("D3D10: pCreateDXGIFactory failure!");
				if (pFactory) {
					HWND hwnd = CreateWindowW(L"STATIC", L"Mumble DXGI Window", WS_OVERLAPPEDWINDOW,
					                          CW_USEDEFAULT, CW_USEDEFAULT, 640, 480, 0,
					                          NULL, NULL, 0);

					IDXGIAdapter *pAdapter = NULL;
					pFactory->EnumAdapters(0, &pAdapter);

					D3D10CreateDeviceAndSwapChainType pD3D10CreateDeviceAndSwapChain = reinterpret_cast<D3D10CreateDeviceAndSwapChainType>(GetProcAddress(hD3D10, "D3D10CreateDeviceAndSwapChain"));

					IDXGISwapChain *pSwapChain = NULL;
					ID3D10Device *pDevice = NULL;

					DXGI_SWAP_CHAIN_DESC desc;
					ZeroMemory(&desc, sizeof(desc));

					RECT rcWnd;
					GetClientRect(hwnd, &rcWnd);
					desc.BufferDesc.Width = rcWnd.right - rcWnd.left;
					desc.BufferDesc.Height = rcWnd.bottom - rcWnd.top;

					ods("W %d H %d", desc.BufferDesc.Width, desc.BufferDesc.Height);

					desc.BufferDesc.RefreshRate.Numerator = 60;
					desc.BufferDesc.RefreshRate.Denominator = 1;
					desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
					desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
					desc.BufferDesc.Scaling = DXGI_MODE_SCALING_CENTERED;

					desc.SampleDesc.Count = 1;
					desc.SampleDesc.Quality = 0;

					desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

					desc.BufferCount = 2;

					desc.OutputWindow = hwnd;

					desc.Windowed = true;

					desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

					hr = pD3D10CreateDeviceAndSwapChain(pAdapter, D3D10_DRIVER_TYPE_HARDWARE, NULL, 0, D3D10_SDK_VERSION, &desc, &pSwapChain, &pDevice);
					if (FAILED(hr))
						ods("D3D10: pD3D10CreateDeviceAndSwapChain failure!");

					if (pDevice && pSwapChain) {
						HMODULE hRef;
						// For VC++ the vtable is located at the base addr. of the object and each function entry is a single pointer. Since p.e. the base classes
						// of IDXGISwapChain have a total of 8 functions the 8+Xth entry points to the Xth added function in the derived interface.
						void ***vtbl = (void ***) pSwapChain;
						void *pPresent = (*vtbl)[8];
						if (! GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (char *) pPresent, &hRef)) {
							ods("DXGI: Failed to get module for Present");
						} else {
							GetModuleFileNameW(hRef, dxgi->wcDXGIFileName, 2048);
							unsigned char *b = (unsigned char *) pPresent;
							unsigned char *a = (unsigned char *) hRef;
							dxgi->iOffsetPresent = b-a;
							ods("DXGI: Successfully found Present offset: %ls: %d", dxgi->wcDXGIFileName, dxgi->iOffsetPresent);
						}

						void *pResize = (*vtbl)[13];
						if (! GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (char *) pResize, &hRef)) {
							ods("DXGI: Failed to get module for ResizeBuffers");
						} else {
							wchar_t buff[2048];
							GetModuleFileNameW(hRef, buff, 2048);
							if (wcscmp(buff, dxgi->wcDXGIFileName) == 0) {
								unsigned char *b = (unsigned char *) pResize;
								unsigned char *a = (unsigned char *) hRef;
								dxgi->iOffsetResize = b-a;
								ods("DXGI: Successfully found ResizeBuffers offset: %ls: %d", dxgi->wcDXGIFileName, dxgi->iOffsetResize);
							}
						}

						vtbl = (void ***) pDevice;

						void *pAddRef = (*vtbl)[1];
						if (! GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (char *) pAddRef, &hRef)) {
							ods("D3D10: Failed to get module for AddRef");
						} else {
							GetModuleFileNameW(hRef, dxgi->wcD3D10FileName, 2048);
							unsigned char *b = (unsigned char *) pAddRef;
							unsigned char *a = (unsigned char *) hRef;
							dxgi->iOffsetAddRef = b-a;
							ods("D3D10: Successfully found AddRef offset: %ls: %d", dxgi->wcD3D10FileName, dxgi->iOffsetAddRef);
						}

						void *pRelease = (*vtbl)[2];
						if (! GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (char *) pRelease, &hRef)) {
							ods("D3D10: Failed to get module for Release");
						} else {
							wchar_t buff[2048];
							GetModuleFileNameW(hRef, buff, 2048);
							if (wcscmp(buff, dxgi->wcD3D10FileName) == 0) {
								unsigned char *b = (unsigned char *) pRelease;
								unsigned char *a = (unsigned char *) hRef;
								dxgi->iOffsetRelease = b-a;
								ods("D3D10: Successfully found Release offset: %ls: %d", dxgi->wcD3D10FileName, dxgi->iOffsetRelease);
							}
						}
					}

					if (pDevice)
						pDevice->Release();
					if (pSwapChain)
						pSwapChain->Release();
					DestroyWindow(hwnd);

					pFactory->Release();
				}
			}
		}
	} else {
		ods("No DXGI pre-Vista");
	}
}
