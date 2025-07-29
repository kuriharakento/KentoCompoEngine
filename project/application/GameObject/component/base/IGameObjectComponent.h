#pragma once

class CameraManager;
class GameObject;

class IGameObjectComponent
{
public:
	virtual ~IGameObjectComponent() = default;
	virtual void Update(GameObject* owner) = 0;
};