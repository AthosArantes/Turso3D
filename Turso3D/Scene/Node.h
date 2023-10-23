#pragma once

#include <Turso3D/Math/Quaternion.h>
#include <Turso3D/Utils/StringHash.h>
#include <RTTI/RTTI.hpp>
#include <vector>
#include <memory>

namespace Turso3D
{
	class Scene;

	constexpr uint8_t LAYER_DEFAULT = 0;
	constexpr unsigned LAYERMASK_ALL = 0xffffffff;

	// Base class for scene nodes.
	class Node
	{
		friend class Scene;

	public:
		enum Flag
		{
			FLAG_ENABLED = 0x1,
			FLAG_TEMPORARY = 0x2,
			FLAG_SPATIAL = 0x4,
			FLAG_SPATIALPARENT = 0x8,
			FLAG_WORLDTRANSFORMDIRTY = 0x10,
			FLAG_BONE = 0x20,
			FLAG_HELPER = 0x40
		};
	
	public:
		// Destruct. Destroy any child nodes.
		virtual ~Node();

		RTTI_IMPL();

		// Set name. Is not required to be unique within the scene.
		void SetName(const std::string& newName);
		// Set name.
		void SetName(const char* newName);
		// Return name.
		const std::string& Name() const { return name; }
		// Return hash of name.
		const StringHash& NameHash() const { return nameHash; }

		// Set node's layer.
		// Usage is subclass specific, for example rendering nodes selectively.
		// Default is 0.
		void SetLayer(uint8_t newLayer);
		// Return layer.
		uint8_t Layer() const { return layer; }
		// Return bitmask corresponding to layer.
		unsigned LayerMask() const { return 1 << layer; }

		// Set enabled status.
		// Meaning is subclass specific.
		void SetEnabled(bool enable);
		// Set enabled status recursively in the child hierarchy.
		void SetEnabledRecursive(bool enable);
		// Return enabled status.
		bool IsEnabled() const { return TestFlag(FLAG_ENABLED); }

		// Return parent node.
		Node* Parent() const { return parent; }

		// Add node as a child.
		void AddChild(std::unique_ptr<Node> child);

		template <typename T, typename ...Args>
		T* CreateChild(Args&&... args)
		{
			T* child = new T(std::forward<Args>(args)...);
			AddChild(std::unique_ptr<Node> {static_cast<Node*>(child)});
			return child;
		}

		// Remove child node.
		std::unique_ptr<Node> RemoveChild(Node* child);

		// Remove child node.
		template <typename T>
		std::unique_ptr<T> RemoveChild(T* child)
		{
			std::unique_ptr<Node> node = RemoveChild(static_cast<Node*>(child));
			return std::unique_ptr<T> {static_cast<T*>(node.release())};
		}

		// Remove all child nodes.
		void RemoveAllChildren();
		// Remove self from the parent node. No-op if no parent.
		// Note: Causes deletion of self.
		void RemoveSelf();

		// Return number of immediate child nodes.
		size_t NumChildren() const { return children.size(); }
		// Return number of immediate child nodes that are not temporary.
		size_t NumPersistentChildren() const;

		template <typename T>
		T* FindChild(bool recursive = false) const
		{
			const RTTI::typeid_t type = RTTI::GetTypeId<T>();
			for (const std::unique_ptr<Node>& node : children) {
				if (node->GetTypeId() == type) {
					return static_cast<T*>(node.get());
				} else if (recursive) {
					if (T* obj = node->FindChild<T>(true); obj) {
						return obj;
					}
				}
			}
			return nullptr;
		}

		template <typename T>
		T* FindChild(StringHash childNameHash, bool recursive = false) const
		{
			const RTTI::typeid_t type = RTTI::GetTypeId<T>();
			for (const std::unique_ptr<Node>& node : children) {
				if (node->GetTypeId() == type && node->nameHash == childNameHash) {
					return static_cast<T*>(node.get());
				} else if (recursive) {
					if (T* obj = node->FindChild<T>(childNameHash, true); obj) {
						return obj;
					}
				}
			}
			return nullptr;
		}

		// Return all immediate child nodes.
		const std::vector<std::unique_ptr<Node>>& Children() const { return children; }

		// Set bit flag.
		// Called internally.
		void SetFlag(unsigned bit, bool set) const
		{
			if (set) {
				flags |= bit;
			} else {
				flags &= ~bit;
			}
		}
		// Test bit flag.
		// Called internally.
		bool TestFlag(unsigned bit) const { return (flags & bit) != 0; }
		// Return bit flags.
		unsigned Flags() const { return flags; }

	protected:
		// Assign node to a new scene.
		// Called internally.
		void SetScene(Scene* newScene);

		// Handle being assigned to a new parent node.
		virtual void OnParentSet(Node* newParent, Node* oldParent);
		// Handle being assigned to a new scene.
		virtual void OnSceneSet(Scene* newScene, Scene* oldScene);
		// Handle the enabled status changing.
		virtual void OnEnabledChanged(bool newEnabled);
		// Handle the layer changing.
		virtual void OnLayerChanged(uint8_t newLayer);

	private:
		// Parent scene.
		Scene* scene = nullptr;
		// Parent node.
		Node* parent = nullptr;

		// Node name.
		std::string name;
		// Node name hash.
		StringHash nameHash;

		// Node flags.
		// Used to hold several boolean values to reduce memory use.
		mutable unsigned flags = FLAG_ENABLED;
		// Layer number.
		uint8_t layer = 0;

	protected:
		// Child nodes.
		std::vector<std::unique_ptr<Node>> children;
	};
}

RTTI_REGISTER(Turso3D::Node);
