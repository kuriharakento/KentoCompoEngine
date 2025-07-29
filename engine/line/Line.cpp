#include "Line.h"
#include "base/DirectXCommon.h"
#include "base/Camera.h"

Line::~Line()
{
	if (vertexResource_) {
		vertexResource_->Unmap(0, nullptr);
		vertexResource_.Reset();
	}
	if (wvpResource_) {
		wvpResource_->Unmap(0, nullptr);
		wvpResource_.Reset();
	}
	vertexData_ = nullptr;
	wvpData_ = nullptr;
	vertices_.clear();
}

void Line::Initialize(LineCommon* lineCommon) {
    lineCommon_ = lineCommon;
	CreateVertexData();
    CreateWVPResource();
}

void Line::AddLine(const Vector3& start, const Vector3& end, const Vector4& color) {
    vertices_.push_back({ start, color });
    vertices_.push_back({ end, color });
}

void Line::Update(Camera* camera)
{
	UpdateVertexData();
	UpdateMatrix(camera);
}

void Line::CreateVertexData() {
    // バッファリソースの作成
    vertexResource_ = lineCommon_->GetDirectXCommon()->CreateBufferResource(
        sizeof(LineVertex) * kMaxVertexCount
    );

    // バッファにデータを書き込む
    vertexResource_->Map(
        0, 
        nullptr,
		reinterpret_cast<void**>(&vertexData_)
        );

    // バッファビューの設定
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(LineVertex) * kMaxVertexCount;
    vertexBufferView_.StrideInBytes = sizeof(LineVertex);
}

void Line::CreateWVPResource()
{
    // 定数バッファの作成
    wvpResource_ = lineCommon_->GetDirectXCommon()->CreateBufferResource(sizeof(Matrix4x4));

	// バッファにデータを書き込む
    wvpResource_->Map(0, 
        nullptr,
		reinterpret_cast<void**>(&wvpData_)
    );

    wvpData_->WVP = MakeIdentity4x4();
    wvpData_->World = MakeIdentity4x4();
}

void Line::UpdateMatrix(Camera* camera)
{
    if (!camera) { return; }

    // カメラのビュー・プロジェクション行列を取得
    Matrix4x4 viewProjectionMatrix = camera->GetViewProjectionMatrix();

    // ワールド行列とビュー・プロジェクション行列を掛け合わせる
    wvpData_->WVP = wvpData_->World * viewProjectionMatrix;
}

void Line::UpdateVertexData()
{
    // バッファにデータを書き込む
    std::memcpy(vertexData_, vertices_.data(), sizeof(LineVertex) * vertices_.size());
}

void Line::Draw() {
    if (vertices_.empty() || !vertexResource_ || !wvpResource_) return;

    auto commandList = lineCommon_->GetDirectXCommon()->GetCommandList();
    commandList->SetPipelineState(lineCommon_->GetPipelineState().Get());
    commandList->SetGraphicsRootSignature(lineCommon_->GetRootSignature().Get());

    // バッファが有効かチェック
    if (vertexBufferView_.BufferLocation == 0) return;

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

    // WVP行列をGPUに送る
    commandList->SetGraphicsRootConstantBufferView(0, wvpResource_->GetGPUVirtualAddress());

    // 描画
    commandList->DrawInstanced(static_cast<UINT>(vertices_.size()), 1, 0, 0);
}

void Line::Clear() {
    vertices_.clear();
}
