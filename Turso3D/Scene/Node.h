#pragma once

#include <Turso3D/Math/Quaternion.h>
#include <Turso3D/Utils/StringHash.h>
#include <vector>
#include <memory>

namespace Turso3D
{
	class Scene;

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
		// Construct.
		Node();
		// Destruct. Destroy any child nodes.
		virtual ~Node();

		// Set name. Is not required to be unique within the scene.
		void SetName(const std::string& newName);
		// Set name.
		void SetName(const char* newName);
		// Return name.
		const std::string& Name() const { return name; }
		// Return hash of name.
		const StringHash& NameHash() const { return nameHash; }

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

		// Create a new node and add it as a child.
		template <typename T, typename ...Args>
		T* CreateChild(Args&&... args)
		{
			T* child = new T(std::forward<Args>(args)...);
			AddChild(std::unique_ptr<Node> {static_cast<Node*>(child)});
			return child;
		}

		// Remove child node.
		void RemoveChild(Node* child);

		// Remove child node.
		template <typename T>
		void RemoveChild(T* child)
		{
			RemoveChild(static_cast<Node*>(child));
		}

		// Remove self from the parent node.
		// No-op if no parent.
		void RemoveSelf();

		// Destroy child node.
		void DestroyChild(Node* child);
		// Destroy all child nodes.
		void DestroyAllChildren();

		// Return number of immediate child nodes.
		size_t NumChildren() const { return children.size(); }
		// Return number of immediate child nodes that are not temporary.
		size_t NumPersistentChildren() const;

		Node* FindChild(StringHash childNameHash, bool recursive = false) const
		{
			for (size_t i = 0; i < children.size(); ++i) {
				if (children[i]->nameHash == childNameHash) {
					return children[i].get();
				}
			}
			if (recursive) {
				for (size_t i = 0; i < children.size(); ++i) {
					Node* node = children[i]->FindChild(childNameHash, true);
					if (node) {
						return node;
					}
				}
			}
			return nullptr;
		}

		// Return all immediate child nodes.
		const std::vector<std::unique_ptr<Node>>& Children() const { return children; }

		// Set view mask.
		void SetViewMask(unsigned mask);
		// Return the view mask.
		unsigned ViewMask() const { return viewMask; }

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
		// Assign child to a new parent.
		// Also changes scene.
		void SetParent(Node* newParent);

		// Handle being assigned to a new scene.
		virtual void OnSceneSet(Scene* newScene, Scene* oldScene);
		// Handle being assigned to a new parent node.
		virtual void OnParentSet(Node* newParent, Node* oldParent);
		// Handle the enabled status changing.
		virtual void OnEnabledChanged(bool newEnabled);
		// Handle the viewMask changing.
		virtual void OnViewMaskChanged(unsigned oldViewMask);

	private:
		// Parent scene.
		Scene* scene;
		// Parent node.
		Node* parent;

		// Node name.
		std::string name;
		// Node name hash.
		StringHash nameHash;

		// View mask, used for filtering.
		unsigned viewMask;
		// Node flags.
		// Used to hold several boolean values to reduce memory use.
		mutable unsigned flags;

	protected:
		// Child nodes.
		std::vector<std::unique_ptr<Node>> children;
	};
}
