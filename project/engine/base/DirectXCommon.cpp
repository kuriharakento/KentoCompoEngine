#include "base/DirectXCommon.h"

#include <cassert>
#include <format>
#include <thread>

//自作クラス
#include "base/Logger.h"
#include "base/StringUtility.h"

//DirectX
#include <dxcapi.h>
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#include "externals/DirectXTex/d3dx12.h"

//ImGui
//#include "externals/imgui/imgui.h"
//#include "externals/imgui/imgui_impl_dx12.h"
//#include "externals/imgui/imgui_impl_win32.h"
//extern  IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

using namespace Microsoft::WRL;

void DirectXCommon::Initialize(WinApp* winApp)
{
	//Null検出
	assert(winApp);
	//引数をメンバ変数に記録
	winApp_ = winApp;

	/*--------------[ 初期化 ]-----------------*/
	//FPS固定初期化
	InitializeFixFPS();
	//デバイスの初期化
	InitializeDevice();
	//コマンド関連の初期化
	InitializeCommand();
	//スワップチェインの生成
	CreateSwapChain();
	//深度バッファの生成
	CreateDepthBuffer();
	//各種ディスクリプタの生成
	CreateDescriptorHeap();
	//レンダーターゲットビューの生成
	CreateRenderTargetView();
	//深度ステンシルビューの初期化
	CreateDepthStencilView();
	//フェンスの生成
	CreateFence();
	//ビューポート矩形の初期化
	InitializeViewPort();
	//シザリング矩形の初期化
	InitializeScissorRect();
	//DXCコンパイラの初期化
	InitializeDXCCompiler();
}

void DirectXCommon::PreDraw()
{
	/*--------------[ バックバッファの番号取得 ]-----------------*/

	UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

	/*--------------[ リソースバリアで書き込み可能に変更 ]-----------------*/

	//今回のバリアはTransition
	barrier_.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	//Noneにしておく
	barrier_.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	//バリアを張る対称のリソース。現在のバックバッファに対して行う
	barrier_.Transition.pResource = swapChainResources_[backBufferIndex].Get();
	//遷移前（現在）のresourceState
	barrier_.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	//遷移後のResourceState
	barrier_.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	//transitionBarrierを張る
	commandList_->ResourceBarrier(1, &barrier_);

	/*--------------[ 描画先のRTVとDSVを指定する ]-----------------*/

	//ディスクリプタの先頭を取得する
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

	//commandList_->OMSetRenderTargets(1, &rtvHandles_[backBufferIndex], false, nullptr);
	commandList_->OMSetRenderTargets(1, &rtvHandles_[backBufferIndex], false, &dsvHandle);

	/*--------------[ 画面全体の色をクリア ]-----------------*/
	float clearColor[] = { 0.1f,0.25f,0.5f,1.0f };	//青っぽい色。RGBAの順
	commandList_->ClearRenderTargetView(rtvHandles_[backBufferIndex], clearColor, 0, nullptr);

	/*--------------[ 画面全体の深度をクリアする ]-----------------*/

	commandList_->ClearDepthStencilView(
		dsvHandle,
		D3D12_CLEAR_FLAG_DEPTH,
		1.0f,
		0,
		0,
		nullptr
	);

	/*--------------[ ビューポート領域の設定 ]-----------------*/

	commandList_->RSSetViewports(1, &viewport_);

	/*--------------[ シザー矩形の設定 ]-----------------*/

	commandList_->RSSetScissorRects(1, &scissorRect_);
}

void DirectXCommon::PostDraw()
{
	HRESULT hr;

	/*--------------[ バックバッファの番号取得 ]-----------------*/

	UINT backBufferIndex = swapChain_->GetCurrentBackBufferIndex();

	/*--------------[ リソースバリアで表示状態に変更 ]-----------------*/

	//今回のバリアはTransition
	barrier_.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	//Noneにしておく
	barrier_.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	//バリアを張る対称のリソース。現在のバックバッファに対して行う
	barrier_.Transition.pResource = swapChainResources_[backBufferIndex].Get();
	//遷移前（現在）のresourceState
	barrier_.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	//遷移後のResourceState
	barrier_.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	//transitionBarrierを張る
	commandList_->ResourceBarrier(1, &barrier_);

	/*--------------[ グラフィックスコマンドをクローズ ]-----------------*/

	//コマンドリストの内容を確定させる。すべてのコマンドを頼んでからCloseすること
	hr = commandList_->Close();
	assert(SUCCEEDED(hr));

	/*--------------[ GPUコマンドの実行 ]-----------------*/

	//GPU二コマンドリストの実行を行わせる
	Microsoft::WRL::ComPtr<ID3D12CommandList> commandLists[] = { commandList_ };
	commandQueue_->ExecuteCommandLists(1, commandLists->GetAddressOf());

	/*--------------[ GPU画面の交換を通知 ]-----------------*/

	//GPUとOSに画面の交換を行うように通知する
	swapChain_->Present(1, 0);

	/*--------------[ Fenceの値を更新 ]-----------------*/

	//Fenceの値を更新
	fenceValue_++;

	/*--------------[ コマンドキューにシグナルを送る ]-----------------*/

	//GPUがここまでたどり着いたときに、Fenceの値を指定した値を代入するようにSignalを送る
	commandQueue_->Signal(fence_.Get(), fenceValue_);

	/*--------------[ コマンド完了待ち ]-----------------*/


	//Fenceの値が指定したSignal値にたどり着いているか確認する
	//GetCompleteValueの初期値はFence制作時に渡した初期値
	if (fence_->GetCompletedValue() < fenceValue_)
	{
		HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
		//指定したSignalにたどり着いていないので、たどり着くまで待つようにイベントを設定する
		fence_->SetEventOnCompletion(fenceValue_, fenceEvent);
		//イベントを待つ
		WaitForSingleObject(fenceEvent, INFINITE);
		CloseHandle(fenceEvent);
	}

	/*--------------[ FPS固定 ]-----------------*/

	UpdateFixFPS();

	/*--------------[ コマンドアロケータのリセット ]-----------------*/

	//次フレーム用のコマンドリストを準備
	hr = commandAllocator_->Reset();
	assert(SUCCEEDED(hr));

	/*--------------[ コマンドリストのリセット ]-----------------*/

	hr = commandList_->Reset(commandAllocator_.Get(), nullptr);
	assert(SUCCEEDED(hr));

}

void DirectXCommon::InitializeDevice()
{
	HRESULT hr;

	/*--------------[ デバッグレイヤーをオンにする ]-----------------*/

#ifdef _DEBUG
	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
		//デバッグレイヤーを有効化する
		debugController->EnableDebugLayer();
		//さらにGPU側でもチェックを行うようにする
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
#endif

	/*--------------[ DXGIファクトリーの生成 ]-----------------*/

	//HRESULTはWindows刑のエラーコードであり、
	//関数が成功したかどうかをSUCCEEDEDマクロで判定できる
	hr = CreateDXGIFactory(IID_PPV_ARGS(&dxgiFactory_));
	//初期化の根本的な部分でエラーが出た場合はプログラムが間違っているか、
	//どうにもできない場合が多いのでassertにしておく
	assert(SUCCEEDED(hr));

	/*--------------[ アダプターの列挙 ]-----------------*/

	//使用するアダプタ用の変数。最初にnullptrを入れておく
	Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter;
	//いい順にアダプタを頼む
	for (UINT i = 0; dxgiFactory_->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; i++)
	{
		//アダプターの情報を取得する
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter->GetDesc3(&adapterDesc);
		assert(SUCCEEDED(hr));	//取得できないのは一大事
		//ソフトウェアアダプタでなければ採用！
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE))
		{
			//採用したアダプタの情報をログに出力。wstringの方なので注意
			Logger::Log(StringUtility::ConvertString(std::format(L"Use Adapter:{}\n", adapterDesc.Description)));
			break;
		}
		useAdapter = nullptr;
	}
	//適切なアダプタが見つからなかったので起動できない
	assert(useAdapter != nullptr);

	/*--------------[ デバイス生成 ]-----------------*/

	//機能レベルとログの出力用の文字列
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2,D3D_FEATURE_LEVEL_12_1,D3D_FEATURE_LEVEL_12_0
	};
	const char* featureLevelStrings[] = { "12.2","12.1","12.0" };
	//高い順に生成できるか試していく
	for (size_t i = 0; i < _countof(featureLevels); ++i)
	{
		//採用したアダプターでデバイスを生成
		hr = D3D12CreateDevice(useAdapter.Get(), featureLevels[i], IID_PPV_ARGS(&device_));
		//指定した機能レベルでデバイスが生成できたかを確認
		if (SUCCEEDED(hr))
		{
			//生成できたのでログ出力を行ってループを抜ける
			Logger::Log(std::format("FeatureLevel : {}\n", featureLevelStrings[i]));
			break;
		}
	}

	//デバイスの生成がうまくいかなかったので起動できない
	assert(device_ != nullptr);
	Logger::Log("Complete create D3D12Device!!!");//初期化完了のログを出す

	/*--------------[ エラー時にブレークを発生させる設定 ]-----------------*/

#ifdef _DEBUG
	Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue = nullptr;
	if (SUCCEEDED(device_->QueryInterface(IID_PPV_ARGS(&infoQueue))))
	{
		//やばいエラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		//エラー時に止まる
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		//警告時に止まる
		//infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);

		//抑制するメッセージのID
		D3D12_MESSAGE_ID denyIds[] = {
			//windows11までのDXGIデバッグレイヤーとDX12デバッグレイヤーの相互作用バグによるエラーメッセージ
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
		};
		//抑制するレベル
		D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
		D3D12_INFO_QUEUE_FILTER filter{};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;
		//指定したメッセージの表示を抑制する
		infoQueue->PushStorageFilter(&filter);

		////解放
		//infoQueue->Release();
	}
#endif
}

void DirectXCommon::InitializeCommand()
{
	HRESULT hr;

	/*--------------[ コマンドアロケータの生成 ]-----------------*/


	hr = device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator_));
	//コマンドアロケータの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	/*--------------[ コマンドリストの生成 ]-----------------*/

	hr = device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(&commandList_));
	//コマンドリストの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

	/*--------------[ コマンドキューの生成 ]-----------------*/


	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	hr = device_->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&commandQueue_));
	//コマンドキューの生成がうまくいかなかったので起動できない
	assert(SUCCEEDED(hr));

}

void DirectXCommon::CreateSwapChain()
{
	HRESULT hr;

	/*--------------[ スワップチェインの設定 ]-----------------*/

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = WinApp::kClientWidth;								//画面の幅。ウィンドウのクライアント領域を同じものにしておく
	swapChainDesc.Height = WinApp::kClientHeight;							//画面の高さ。ウィンドウのクライアント領域を同じものにしておく
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;				//色の形式
	swapChainDesc.SampleDesc.Count = 1;								//マルチサンプルしない
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;	//描画のターゲットとして利用する
	swapChainDesc.BufferCount = 2;									//ダブルバッファ
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;		//モニタにうつしたら、中身を廃棄

	/*--------------[ スワップチェインの生成 ]-----------------*/

	//コマンドキュー、ウィンドウハンドル、設定を渡して生成する
	hr = dxgiFactory_->CreateSwapChainForHwnd(commandQueue_.Get(), winApp_->GetHwnd(), &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(swapChain_.GetAddressOf()));
	assert(SUCCEEDED(hr));
}

void DirectXCommon::CreateDepthBuffer()
{
	HRESULT hr;

	/*--------------[ 深度バッファリソースの設定 ]-----------------*/

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = WinApp::kClientWidth;											//Textureの幅
	resourceDesc.Height = WinApp::kClientHeight;										//Textureの高さ
	resourceDesc.MipLevels = 1;											//mipmapの数
	resourceDesc.DepthOrArraySize = 1;									//奥行き or 配列Textureの配列数
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;				//DepthStencilとして利用可能なフォーマット
	resourceDesc.SampleDesc.Count = 1;									//サンプリングカウント。１個指定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;		//２次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;		//DepthStencilとして使う通知

	/*--------------[ 利用するHeapの設定 ]-----------------*/

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;						//VRAM上に作る

	/*--------------[ 深度値のクリア設定 ]-----------------*/

	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.0f;							//1.0f(最大値)でクリア
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;				//フォーマット。Resourceと合わせる

	/*--------------[ 深度バッファの生成 ]-----------------*/

	//生成
	hr = device_->CreateCommittedResource(
		&heapProperties,												//Heapの設定
		D3D12_HEAP_FLAG_NONE,											//Heapの特殊な設定。特になし
		&resourceDesc,													//Resourceの設定
		D3D12_RESOURCE_STATE_DEPTH_WRITE,								//深度値を書き込む状態にしておく
		&depthClearValue,												//Clear最適値
		IID_PPV_ARGS(&depthBuffer_)											//作成するResourceポインタへのポインタ
	);
	assert(SUCCEEDED(hr));

}

void DirectXCommon::CreateDescriptorHeap()
{

	///===================================================================
	///ディスクリプタヒープのサイズ
	///===================================================================

	descriptorSizeRTV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	descriptorSizeDSV_ = device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	///===================================================================
	///ディスクリプタヒープの生成
	///===================================================================

	//RTV用のヒープでディスクリプタの数は２。RTVはShader内で触るものではないので、ShaderVisibleはfalse
	rtvDescriptorHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);

	//DSV用のヒープでディスクリプタの数は１。DSVはSharder内で触るものではないので、ShaderVisibleはfalse
	dsvDescriptorHeap_ = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);


}

void DirectXCommon::CreateRenderTargetView()
{
	HRESULT hr;

	/*--------------[ SwapChainからResourceを引っ張ってくる ]-----------------*/

	hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(&swapChainResources_[0]));
	//うまく取得できなければ起動できない
	assert(SUCCEEDED(hr));
	hr = swapChain_->GetBuffer(1, IID_PPV_ARGS(&swapChainResources_[1]));
	assert(SUCCEEDED(hr));

	/*--------------[ RTVの設定 ]-----------------*/

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	//ディスクリプタの先頭を取得する
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart();

	/*--------------[ RTVハンドルの要素数を２個に変更する ]-----------------*/

	rtvHandles_[0] = rtvStartHandle;
	device_->CreateRenderTargetView(swapChainResources_[0].Get(), &rtvDesc, rtvHandles_[0]);
	//２つ目のディスクリプタハンドルを得る
	rtvHandles_[1].ptr = rtvHandles_[0].ptr + device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	//２つ目を作る
	device_->CreateRenderTargetView(swapChainResources_[1].Get(), &rtvDesc, rtvHandles_[1]);
}

void DirectXCommon::CreateDepthStencilView()
{
	/*--------------[ DSVの設定 ]-----------------*/

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

	/*--------------[ DSVHeapの先頭にDSVを作る ]-----------------*/
	device_->CreateDepthStencilView(
		depthBuffer_.Get(),
		&dsvDesc,
		dsvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart()
	);
}

void DirectXCommon::CreateFence()
{
	HRESULT hr;

	/*--------------[ フェンスの生成 ]-----------------*/

	hr = device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence_));
	assert(SUCCEEDED(hr));

	////FenceのSignalを待つためのイベントを作成する
	//fenceEvent_ = CreateEvent(NULL, FALSE, FALSE, NULL);
	//assert(fenceEvent_ != nullptr);
}

void DirectXCommon::InitializeViewPort()
{
	/*--------------[ ビューポート矩形の設定 ]-----------------*/

	viewport_.Width = WinApp::kClientWidth;
	viewport_.Height = WinApp::kClientHeight;
	viewport_.TopLeftX = 0;
	viewport_.TopLeftY = 0;
	viewport_.MinDepth = 0.0f;
	viewport_.MaxDepth = 1.0f;
}

void DirectXCommon::InitializeScissorRect()
{
	/*--------------[ シザリング矩形の設定 ]-----------------*/
	scissorRect_.left = 0;
	scissorRect_.right = WinApp::kClientWidth;
	scissorRect_.top = 0;
	scissorRect_.bottom = WinApp::kClientHeight;
}

void DirectXCommon::InitializeDXCCompiler()
{
	HRESULT hr;
	/*--------------[ DXCユーティリティの生成 ]-----------------*/

	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils_));
	assert(SUCCEEDED(hr));

	/*--------------[ DXCコンパイラの生成 ]-----------------*/


	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler_));
	assert(SUCCEEDED(hr));

	/*--------------[ デフォルトインクルードハンドラの生成 ]-----------------*/

	//現時点でincludeはしないが、includeに対応するための設定を行っておく
	hr = dxcUtils_->CreateDefaultIncludeHandler(&includeHandler_);
	assert(SUCCEEDED(hr));
}

void DirectXCommon::InitializeFixFPS()
{
	//現在時間を記録する
	reference_ = std::chrono::steady_clock::now();
}

void DirectXCommon::UpdateFixFPS()
{
	// 1//60秒ぴったりの時間
	const std::chrono::microseconds kMinTime(uint64_t(1000000.0f / 60.0f));
	// 1/60秒よりわずかに短い時間
	const std::chrono::microseconds kMinCheckTime(uint64_t(1000000.0f / 65.0f));

	//現在時間を取得
	std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
	//前回記録からの経過時間を取得する
	std::chrono::microseconds elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - reference_);

	// 1/60(よりわずかに短い時間) 経っていない場合
	if(elapsed < kMinCheckTime)
	{
		// 1/60秒経過するまで微小なスリープを繰り返す
		while(std::chrono::steady_clock::now() - reference_ < kMinTime)
		{
			//1マイクロ秒スリープ
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}
	}
	//現在時間の記録をする
	reference_ = std::chrono::steady_clock::now();

}

Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> DirectXCommon::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptor,
                                                                                 bool shaderVisible)
{
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = heapType; descriptorHeapDesc.NumDescriptors = numDescriptor;
	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HRESULT hr = device_->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap));
	assert(SUCCEEDED(hr));
	return descriptorHeap;
}


void DirectXCommon::CreateSamplerHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC desc{};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	desc.NumDescriptors = 1;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	HRESULT hr = device_->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&samplerHeap_));
	assert(SUCCEEDED(hr) && "Failed to create Sampler Heap!");

	// デバッグログ
	D3D12_GPU_DESCRIPTOR_HANDLE samplerHeapBase = samplerHeap_->GetGPUDescriptorHandleForHeapStart();
	OutputDebugStringA(("Sampler Heap Base Address: " + std::to_string(samplerHeapBase.ptr) + "\n").c_str());
	assert(samplerHeap_ != nullptr && "Sampler Descriptor Heap is null!");

	D3D12_SAMPLER_DESC samplerDesc{};
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc.MipLODBias = 0.0f;
	samplerDesc.MaxAnisotropy = 1;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	device_->CreateSampler(&samplerDesc, samplerHeap_->GetCPUDescriptorHandleForHeapStart());

}

D3D12_CPU_DESCRIPTOR_HANDLE DirectXCommon::GetCPUDescriptorHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectXCommon::GetGPUDescriptorHandle(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap, uint32_t descriptorSize, uint32_t index)
{
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}

Microsoft::WRL::ComPtr<IDxcBlob> DirectXCommon::CompileSharder(const std::wstring& filePath, const wchar_t* profile)
{
	///===================================================================
	///1.hlslファイルを読む
	///===================================================================

	//これからシェーダーをコンパイルする旨をログに出す
	Logger::Log(StringUtility::ConvertString(std::format(L"Begin CompileSharder, path:{}, profile:{}\n", filePath, profile)));
	//hlslファイルを読む
	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT hr = dxcUtils_->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	//読めなかったら止める
	assert(SUCCEEDED(hr));
	//読み込んだファイルの内容を設定する
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;		//UTF8の文字コードであることを通知

	///===================================================================
	///2.コンパイルする
	///===================================================================

	LPCWSTR arguments[] = {
		filePath.c_str(),				//コンパイル対称のhlslファイル名
		L"-E", L"main",					//エントリーポイントの指定。基本的にmain以外にはしない
		L"-T", profile,					//Sharderprofileの設定
		L"-Zi", L"-Qembed_debug",		//デバッグ用の情報を埋め込む
		L"-Od",							//最適化を外しておく
		L"-Zpr",						//メモリレイアウトは行優先
	};

	//実際にシェーダーをコンパイルする
	IDxcResult* shaderResult = nullptr;
	hr = dxcCompiler_->Compile(
		&shaderSourceBuffer,
		arguments,
		_countof(arguments),
		includeHandler_.Get(),
		IID_PPV_ARGS(&shaderResult)
	);
	//コンパイルエラーではなくdxcで起動できないなど致命的な状況
	assert(SUCCEEDED(hr));


	///===================================================================
	///3.警告・エラーが出ていないか確認する
	///===================================================================

	//警告・エラーが出てたらログに出して止める
	IDxcBlobUtf8* shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0)
	{
		Logger::Log(shaderError->GetStringPointer());
		//警告・エラーダメ絶対
		assert(SUCCEEDED(false));
	}


	///===================================================================
	///4.コンパイル結果を受け取って返す
	///===================================================================

	//コンパイル結果から実行用のバイナリ部分を取得
	IDxcBlob* shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));
	//成功したログを出す
	Logger::Log(StringUtility::ConvertString(std::format(L"Compile Succeeded, path:{}, profile:{}\n", filePath, profile)));
	//もう使わないリソースを解放
	shaderSource->Release();
	shaderResult->Release();
	//実行用のバイナリを返却
	return shaderBlob;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateBufferResource(size_t sizeInBytes)
{
	Microsoft::WRL::ComPtr<ID3D12Resource> bufferResource = nullptr;
	//頂点リソース用のヒープ設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;
	//頂点リソースの設定
	D3D12_RESOURCE_DESC bufferResourceDesc{};
	//バッファリソース。テクスチャの場合はまた別の設定をする
	bufferResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferResourceDesc.Width = (sizeInBytes + 255) & ~255;
	//バッファの場合はこれらは１にする決まり
	bufferResourceDesc.Height = 1;
	bufferResourceDesc.DepthOrArraySize = 1;
	bufferResourceDesc.MipLevels = 1;
	bufferResourceDesc.SampleDesc.Count = 1;
	//バッファの場合はこれにする決まり
	bufferResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT hr = device_->CreateCommittedResource(&uploadHeapProperties, D3D12_HEAP_FLAG_NONE, &bufferResourceDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&bufferResource));
	assert(SUCCEEDED(hr));

	return bufferResource;
}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::CreateTextureResource(const DirectX::TexMetadata& metadata)
{
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metadata.width);									//Textureの幅
	resourceDesc.Height = UINT(metadata.height);								//Textureの高さ
	resourceDesc.MipLevels = UINT(metadata.mipLevels);							//mipmapの数
	resourceDesc.DepthOrArraySize = UINT(metadata.arraySize);					//奥行き or 配列Textureの配列数
	resourceDesc.Format = metadata.format;										//TextureのFormat
	resourceDesc.SampleDesc.Count = 1;											//サンプリングカウント。１固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metadata.dimension);		//Textureの次元数。普段使っているのは２次元

	//利用するHeapの設定。非常に特殊な運用。02_04exで一般的なケース版がある
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;								//細かい設定を行う

	//resourceの生成
	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;
	HRESULT hr = device_->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&resource)
	);
	assert(SUCCEEDED(hr));

	return resource;


}

Microsoft::WRL::ComPtr<ID3D12Resource> DirectXCommon::UploadTextureData(Microsoft::WRL::ComPtr<ID3D12Resource> texture, const DirectX::ScratchImage& mipImages)
{
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	DirectX::PrepareUpload(device_.Get(), mipImages.GetImages(), mipImages.GetImageCount(), mipImages.GetMetadata(), subresources);
	uint64_t intermediateSize = GetRequiredIntermediateSize(texture.Get(), 0, UINT(subresources.size()));
	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = CreateBufferResource(intermediateSize);
	UpdateSubresources(commandList_.Get(), texture.Get(), intermediateResource.Get(), 0, 0, UINT(subresources.size()), subresources.data());
	//Textureへの転送後は利用できるよう、D3D12_RESOURCE_STATE_COPY_DESTからD3D12_RESOURCE_STATE_GENERI_READへResourceStateを変更する
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture.Get();
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	commandList_->ResourceBarrier(1, &barrier);
	return intermediateResource;
}

