#include "pch.h"
#include "Camera.h"

using namespace ExecuteIndirect;

/// <summary>
/// Initializes a new instance of the Camera class.
/// </summary>
Camera::Camera()
{
	//set vertical field of view to be 90 degrees, near plane at 1 and far plane at 1000
	m_FieldOfViewY = XM_PIDIV2;
	m_AspectRatio = 1.f;
	m_NearPlane = 1.f;
	m_FarPlane = 1000.f;

	//set camera origin and axis
	m_ViewMatrixOrigin = XMFLOAT3(0.f, 100.f, 0.f);
	m_ViewMatrixX = XMFLOAT3(1.f, 0.f, 0.f);
	m_ViewMatrixY = XMFLOAT3(0.f, 1.f, 0.f);
	m_ViewMatrixZ = XMFLOAT3(0.f, 0.f, 1.f);
	//create view and projection matrices
	BuildViewMatrix();
	BuildProjectionMatrix();
}



/// <summary>
/// Gets the view matrix.
/// </summary>
/// <returns>Reference to 4x4 view matrix</returns>
XMFLOAT4X4& Camera::GetViewMatrix()
{
	BuildViewMatrix();
	return m_ViewMatrix;
}

/// <summary>
/// Gets the projection matrix.
/// </summary>
/// <returns>Reference to 4x4 projection matrix</returns>
XMFLOAT4X4& Camera::GetProjectionMatrix()
{
	BuildProjectionMatrix();
	return m_ProjectionMatrix;
}

/// <summary>
/// Sets the frustum.
/// </summary>
/// <param name="fov">Field of view.</param>
/// <param name="aspect">The aspect ratio of width and height.</param>
/// <param name="nearZ">The near z plane.</param>
/// <param name="farZ">The far z plane.</param>
void Camera::SetFrustum(float fov, float aspect, float nearZ, float farZ)
{
	m_FieldOfViewY = fov;
	m_AspectRatio = aspect;
	m_NearPlane = nearZ;
	m_FarPlane = farZ;
}

/// <summary>
/// Builds the view matrix.
/// </summary>
void Camera::BuildViewMatrix()
{
	//Using the generic vector type as most ot XM functions use it instead of XMFLOAT{n}
	XMVECTOR R = XMLoadFloat3(&m_ViewMatrixX);
	XMVECTOR U = XMLoadFloat3(&m_ViewMatrixY);
	XMVECTOR L = XMLoadFloat3(&m_ViewMatrixZ);
	XMVECTOR P = XMLoadFloat3(&m_ViewMatrixOrigin);

	// Keep camera's axes orthogonal to each other and of unit length.
	L = XMVector3Normalize(L);
	U = XMVector3Normalize(XMVector3Cross(L, R));

	// U, L already ortho-normal, so no need to normalize cross product.
	R = XMVector3Cross(U, L);

	// Fill in the view matrix entries.
	float Qx = -XMVectorGetX(XMVector3Dot(P, R));
	float Qy = -XMVectorGetX(XMVector3Dot(P, U));
	float Qz = -XMVectorGetX(XMVector3Dot(P, L));

	//store the data from the generic types in the XMFLOAT{n} vectors
	XMStoreFloat3(&m_ViewMatrixX, R);
	XMStoreFloat3(&m_ViewMatrixY, U);
	XMStoreFloat3(&m_ViewMatrixZ, L);

	//fill the matrix elements 
	m_ViewMatrix(0, 0) = m_ViewMatrixX.x;
	m_ViewMatrix(1, 0) = m_ViewMatrixX.y;
	m_ViewMatrix(2, 0) = m_ViewMatrixX.z;
	m_ViewMatrix(3, 0) = Qx;

	m_ViewMatrix(0, 1) = m_ViewMatrixY.x;
	m_ViewMatrix(1, 1) = m_ViewMatrixY.y;
	m_ViewMatrix(2, 1) = m_ViewMatrixY.z;
	m_ViewMatrix(3, 1) = Qy;

	m_ViewMatrix(0, 2) = m_ViewMatrixZ.x;
	m_ViewMatrix(1, 2) = m_ViewMatrixZ.y;
	m_ViewMatrix(2, 2) = m_ViewMatrixZ.z;
	m_ViewMatrix(3, 2) = Qz;

	m_ViewMatrix(0, 3) = 0;
	m_ViewMatrix(1, 3) = 0;
	m_ViewMatrix(2, 3) = 0;
	m_ViewMatrix(3, 3) = 1.0f;


}

/// <summary>
/// Builds the projection matrix.
/// </summary>
void Camera::BuildProjectionMatrix()
{
	//using library function for creating left hand perspective matrix
	//XMMatrixPerspectiveFovLH returns generic XMMATRIX so we use XMStoreFloat4x4 to store it in member XMFLOAT4x4
	XMStoreFloat4x4(&m_ProjectionMatrix, XMMatrixPerspectiveFovLH(m_FieldOfViewY, m_AspectRatio, m_NearPlane, m_FarPlane));
}


/// <summary>
/// Pitches the specified angle (rotation around X axis).
/// </summary>
/// <param name="angle">The angle of rotation.</param>
void Camera::Pitch(float angle)
{
	//Create matrix for rotation around X axis of camera for the angle 
	XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&m_ViewMatrixX), angle);
	//Transform the other axes with the transformation matrix
	//The axis we are rotating around mustn't be transformed
	XMStoreFloat3(&m_ViewMatrixY, XMVector3TransformNormal(XMLoadFloat3(&m_ViewMatrixY), R));
	XMStoreFloat3(&m_ViewMatrixZ, XMVector3TransformNormal(XMLoadFloat3(&m_ViewMatrixZ), R));
}

/// <summary>
/// Yaws the specified angle (rotation around Y axis).
/// </summary>
/// <param name="angle">The angle.</param>
void Camera::Yaw(float angle)
{
	//Create matrix for rotation around Y world axis for the angle 
	XMMATRIX R = XMMatrixRotationY(angle); 
	//This time we are transforming all axes as we are not rotating around camera axis but around the world Y axis
	XMStoreFloat3(&m_ViewMatrixX, XMVector3TransformNormal(XMLoadFloat3(&m_ViewMatrixX), R));
	XMStoreFloat3(&m_ViewMatrixY, XMVector3TransformNormal(XMLoadFloat3(&m_ViewMatrixY), R));
	XMStoreFloat3(&m_ViewMatrixZ, XMVector3TransformNormal(XMLoadFloat3(&m_ViewMatrixZ), R));
}

/// <summary>
/// Moves the camera forward or backward.
/// </summary>
/// <param name="distance">Distance of moving.</param>
void Camera::MoveForwardOrBackward(float distance)
{
	//Copy the distance value inside all elements of temp vector
	XMVECTOR s = XMVectorReplicate(distance);
	//load camera origin and z-axis in general XMVECTOR type
	XMVECTOR l = XMLoadFloat3(&m_ViewMatrixZ);
	XMVECTOR p = XMLoadFloat3(&m_ViewMatrixOrigin);
	//multiply the z vector by the distance vector and add it to the camera origin
	XMStoreFloat3(&m_ViewMatrixOrigin, XMVectorMultiplyAdd(s, l, p));
}

/// <summary>
/// Moves the the camera left or right.
/// </summary>
/// <param name="distance">The distance.</param>
void Camera::MoveLeftOrRight(float distance)
{
	//Copy the distance value inside all elements of temp vector
	XMVECTOR s = XMVectorReplicate(distance);
	//load camera origin and x-axis in general XMVECTOR type
	XMVECTOR r = XMLoadFloat3(&m_ViewMatrixX);
	XMVECTOR p = XMLoadFloat3(&m_ViewMatrixOrigin);
	//multiply the x vector by the distance vector and add it to the camera origin
	XMStoreFloat3(&m_ViewMatrixOrigin, XMVectorMultiplyAdd(s, r, p));
}