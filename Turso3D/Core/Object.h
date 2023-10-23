#pragma once

#include <RTTI/RTTI.hpp>
#include <map>

namespace Turso3D
{
	// Base class for objects with type identification.
	class Object
	{
	public:
		virtual ~Object();

		RTTI_IMPL();

		// Register an object as a subsystem that can be accessed globally.
		// Note that the subsystems container does not own the objects.
		static void RegisterSubsystem(Object* subsystem);
		// Remove a subsystem by object pointer.
		static void RemoveSubsystem(Object* subsystem);
		// Remove a subsystem by type.
		static void RemoveSubsystem(RTTI::typeid_t type);
		// Return a subsystem by type, or null if not registered.
		static Object* Subsystem(RTTI::typeid_t type);

		// Return a subsystem, template version.
		template <class T>
		static T* Subsystem()
		{
			return static_cast<T*>(Subsystem(RTTI::GetTypeId<T>()));
		}

	private:
		// Registered subsystems.
		static std::map<RTTI::typeid_t, Object*> subsystems;
	};
}

RTTI_REGISTER(Turso3D::Object);
