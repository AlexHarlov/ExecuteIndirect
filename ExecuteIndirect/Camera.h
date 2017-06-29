#pragma once

#include <DirectXMath.h>

using namespace DirectX;
namespace ExecuteIndirect {
	class Camera
	{
	public:
		Camera();
		void Pitch(float angle);
		void Yaw(float angle);
		void MoveForwardOrBackward(float distance);
		void MoveLeftOrRight(float distance);
		XMFLOAT4X4& GetViewMatrix();
		XMFLOAT4X4& GetProjectionMatrix();
		XMFLOAT3& GetMatrixOrigin() { return m_ViewMatrixOrigin; }
		void SetFrustum(float fov, float aspect, float nearZ, float farZ);

	private:
		void BuildViewMatrix();
		void BuildProjectionMatrix();

		float m_NearPlane = 0.0f;
		float m_FarPlane = 0.0f;
		float m_AspectRatio = 0.0f;
		float m_FieldOfViewY = 0.0f;

		XMFLOAT4X4 m_ViewMatrix;
		XMFLOAT4X4 m_ProjectionMatrix;

		XMFLOAT3 m_ViewMatrixOrigin = { 0.0f, 0.0f, 0.0f };
		XMFLOAT3 m_ViewMatrixX = { 1.0f, 0.0f, 0.0f };
		XMFLOAT3 m_ViewMatrixY = { 0.0f, 1.0f, 0.0f };
		XMFLOAT3 m_ViewMatrixZ = { 0.0f, 0.0f, 1.0f };
	};

}