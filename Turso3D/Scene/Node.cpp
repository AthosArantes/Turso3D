#include <Turso3D/Scene/Node.h>
#include <Turso3D/IO/Log.h>
#include <Turso3D/Scene/Scene.h>

namespace Turso3D
{
	Node::Node() :
		scene(nullptr),
		parent(nullptr),
		flags(FLAG_ENABLED),
		viewMask(1u)
	{
	}

	Node::~Node()
	{
		DestroyAllChildren();
	}

	void Node::SetName(const std::string& newName)
	{
		name = newName;
		nameHash = StringHash {newName};
	}

	void Node::SetName(const char* newName)
	{
		name = newName;
		nameHash = StringHash {newName};
	}

	void Node::SetEnabled(bool enable)
	{
		if (enable != TestFlag(FLAG_ENABLED)) {
			SetFlag(FLAG_ENABLED, enable);
			OnEnabledChanged(enable);
		}
	}

	void Node::SetEnabledRecursive(bool enable)
	{
		SetEnabled(enable);
		for (size_t i = 0; i < children.size(); ++i) {
			Node* child = children[i].get();
			child->SetEnabledRecursive(enable);
		}
	}

	void Node::AddChild(std::unique_ptr<Node> child)
	{
		// Check for illegal or redundant parent assignment
		if (!child || child->parent == this) {
			return;
		}

#ifdef _DEBUG
		// Check for possible illegal or cyclic parent assignment
		if (child.get() == this) {
			LOG_ERROR("Attempted parenting node to self");
			return;
		}

		Node* current = parent;
		while (current) {
			if (current == child.get()) {
				LOG_ERROR("Attempted cyclic node parenting");
				return;
			}
			current = current->parent;
		}
#endif

		Node* child_node = child.get();
		children.push_back(std::move(child));
		child_node->SetParent(this);
	}

	void Node::RemoveChild(Node* child)
	{
		if (child && child->parent == this) {
			for (size_t i = 0; i < children.size(); ++i) {
				Node* node_child = children[i].get();
				if (node_child == child) {
					children[i].release();
					children.back().swap(children[i]);
					children.pop_back();

					node_child->SetParent(nullptr);
					return;
				}
			}
		}
	}

	void Node::DestroyChild(Node* child)
	{
		if (child && child->parent == this) {
			for (size_t i = 0; i < children.size(); ++i) {
				Node* node_child = children[i].get();
				if (node_child == child) {
					children.back().swap(children[i]);
					children.pop_back();
					return;
				}
			}
		}
	}

	void Node::DestroyAllChildren()
	{
		children.clear();
	}

	void Node::RemoveSelf()
	{
		if (parent) {
			parent->RemoveChild(this);
		}
	}

	size_t Node::NumPersistentChildren() const
	{
		size_t ret = 0;
		for (size_t i = 0; i < children.size(); ++i) {
			Node* child = children[i].get();
			if (!child->TestFlag(FLAG_TEMPORARY)) {
				++ret;
			}
		}
		return ret;
	}

	void Node::SetViewMask(unsigned mask)
	{
		unsigned oldMask = viewMask;
		viewMask = mask;
		OnViewMaskChanged(oldMask);
	}

	void Node::SetScene(Scene* newScene)
	{
		Scene* oldScene = scene;
		scene = newScene;
		OnSceneSet(scene, oldScene);

		// Also set for the children
		for (size_t i = 0; i < children.size(); ++i) {
			children[i]->SetScene(newScene);
		}
	}

	void Node::SetParent(Node* newParent)
	{
		Node* oldParent = parent;
		parent = newParent;
		SetScene(newParent ? newParent->scene : nullptr);

		OnParentSet(newParent, oldParent);
	}

	void Node::OnSceneSet(Scene*, Scene*)
	{
	}

	void Node::OnParentSet(Node*, Node*)
	{
	}

	void Node::OnEnabledChanged(bool)
	{
	}

	void Node::OnViewMaskChanged(unsigned)
	{
	}
}
