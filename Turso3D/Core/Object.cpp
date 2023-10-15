#include "Object.h"

namespace Turso3D
{
	std::map<RTTI::typeid_t, Object*> Object::subsystems;

	void Object::RegisterSubsystem(Object* subsystem)
	{
		if (!subsystem) {
			return;
		}
		subsystems[subsystem->GetTypeId()] = subsystem;
	}

	void Object::RemoveSubsystem(Object* subsystem)
	{
		if (!subsystem) {
			return;
		}
		auto it = subsystems.find(subsystem->GetTypeId());
		if (it != subsystems.end() && it->second == subsystem) {
			subsystems.erase(it);
		}
	}

	void Object::RemoveSubsystem(RTTI::typeid_t type)
	{
		subsystems.erase(type);
	}

	Object* Object::Subsystem(RTTI::typeid_t type)
	{
		auto it = subsystems.find(type);
		return it != subsystems.end() ? it->second : nullptr;
	}
}
